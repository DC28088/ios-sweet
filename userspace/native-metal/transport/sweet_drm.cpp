// SPDX-License-Identifier: MIT
// NativeAdreno ABI1 transport for Mesa 25.0.7's MSM Gallium winsys.
// Mesa owns GEM identities; this file owns their task memory and kernel handles.
// No Linux device, Vulkan loader, or foreign Redwood user-client selector is used.
#include "xf86drm.h"
#include "NativeAdrenoABI.h"
#include "drm-uapi/msm_drm.h"
#include <IOSurface/IOSurfaceRef.h>
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>
#include <pthread.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <new>
#include <vector>

namespace {
constexpr unsigned MaxBO = 4096;
constexpr uint64_t HostPage = 16384, GuardBytes = HostPage;
constexpr uint64_t WaitNS = 5000000000ull;
struct Buffer {
    uint64_t kernel = 0, iova = 0, size = 0, cpu = 0, last = 0;
    IOSurfaceRef surface = nullptr;
    uint32_t flags = 0, cpu_token = 0, surface_id = 0;
    bool cpu_lease = false, cpu_mapped = false;
    bool closing = false, gpu_dirty = false, imported_published = false;
    // IOSurface seed at this handle's last pre-submit clean. CPU writers bump
    // the seed when they unlock; CPU access through this driver clears
    // clean_valid. Unchanged seed = no CPU data to publish.
    bool clean_valid = false;
    uint32_t clean_seed = 0;
    std::vector<uint8_t> metadata;
    // Shadow of an IOSurface whose rows are not 64-byte aligned (A618 linear
    // sampling needs that): an owned BO at the rounded pitch, refilled from
    // the surface on the CPU when its seed changes (see shadow_refill()).
    // A bi-planar YCbCr surface has one shadow BO holding both planes (ycbcr_shadow_layout()).
    struct ShadowPlane { uint32_t src_offset = 0, src_pitch = 0, offset = 0, pitch = 0, rows = 0, row_bytes = 0; };
    IOSurfaceRef shadow_src = nullptr;
    ShadowPlane shadow_plane[2];
    uint32_t shadow_id = 0, shadow_planes = 0, shadow_seed = 0;
    bool shadow_valid = false;
    // 26bB: a converted shadow (ycbcr_convert_layout()): plane 0 = luma source + the RGBA destination rows,
    // plane 1 = CbCr source; shadow_refill() writes one (Cr, Y, Cb, 255) texel per luma texel.
    bool shadow_convert = false;
};
struct Fence { uint32_t number; uint64_t kernel, queue; };
struct State {
    io_connect_t connection = 0;
    int fd = -1;
    NativeAdrenoAbiParams params{};
    NativeAdrenoAbiDeviceInfo info{};
    Buffer buffers[MaxBO];
    std::vector<Fence> fences;
    uint64_t completed = 0, submitted = 0;
    uint32_t next = 1;
    uint64_t queues[NativeAdrenoAbiMaxQueues]{};
    bool lost = false, command_active = false;
    bool strict_publish = false;   // /var/local/sweet-native-strict-publish
    bool always_clean = false;     // /var/local/sweet-native-always-clean
    bool eager_retire = false;     // /var/local/sweet-native-eager-retire
    uint64_t cleans = 0, clean_skips = 0;
    uint64_t shadow_fills = 0, shadow_fill_bytes = 0, shadow_waits = 0;
    uint64_t busy_backoffs = 0;   // submit retries while other clients hold every kernel job slot
};
// Kept for process lifetime: closing one Gallium fd must not invalidate another
// screen or IOSurface. In-flight storage is never freed after an unknown timeout.
State state;
// Owned-buffer recycle pool (see release()).
constexpr uint64_t PoolBytes=64ull<<20, PoolMaxBuffer=16ull<<20;
std::vector<Buffer> pool;
uint64_t pool_bytes=0, pool_hits=0, pool_misses=0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct Lock { Lock(){pthread_mutex_lock(&mutex);} ~Lock(){pthread_mutex_unlock(&mutex);} };
uint64_t clock_ns() {
    timespec now{};clock_gettime(CLOCK_MONOTONIC_RAW,&now);
    return uint64_t(now.tv_sec)*1000000000ull+now.tv_nsec;
}
struct Timings {
    uint64_t began=0,submits=0,ibs=0,total=0,wait=0,cache=0,cache_bytes=0,ioctl=0;
} timings;
struct SubmitTiming {
    uint64_t began=clock_ns();
    SubmitTiming() {if(!timings.began)timings.began=began;}
    ~SubmitTiming() {
        const uint64_t now=clock_ns();
        timings.total+=now-began;++timings.submits;
        if(now-timings.began<5000000000ull)return;
        fprintf(stderr,"SweetNativeMetal: transport window_ms=%llu submits=%llu ibs=%llu submit_cpu_us=%llu fence_us=%llu cache_us=%llu cache_MiB=%llu submit_ioctl_us=%llu bo_pool_hits=%llu bo_pool_misses=%llu bo_pool_MiB=%llu surface_cleans=%llu surface_clean_skips=%llu\n",
            (unsigned long long)((now-timings.began)/1000000),
            (unsigned long long)timings.submits,(unsigned long long)timings.ibs,
            (unsigned long long)(timings.total/1000),(unsigned long long)(timings.wait/1000),
            (unsigned long long)(timings.cache/1000),(unsigned long long)(timings.cache_bytes>>20),
            (unsigned long long)(timings.ioctl/1000),
            (unsigned long long)pool_hits,(unsigned long long)pool_misses,(unsigned long long)(pool_bytes>>20),
            (unsigned long long)state.cleans,(unsigned long long)state.clean_skips);
        if(state.shadow_fills || state.shadow_waits)
            fprintf(stderr,"SweetNativeMetal: shadow surfaces refilled=%llu bytes=%llu gpu_waits=%llu\n",
                (unsigned long long)state.shadow_fills,(unsigned long long)state.shadow_fill_bytes,
                (unsigned long long)state.shadow_waits);
        if(state.busy_backoffs)
            fprintf(stderr,"SweetNativeMetal: submit backed off %llu x 500 us (job slots held by other clients)\n",
                (unsigned long long)state.busy_backoffs);
        state.shadow_fills=state.shadow_fill_bytes=state.shadow_waits=state.busy_backoffs=0;
        pool_hits=pool_misses=0;state.cleans=state.clean_skips=0;
        timings=Timings{};timings.began=now;
    }
};
int error(kern_return_t r) {
    switch (r) {
    case KERN_SUCCESS: return 0;
    case kIOReturnNoMemory: return ENOMEM;
    case kIOReturnNoResources: return ENOSPC;
    case kIOReturnBadArgument: return EINVAL;
    case kIOReturnTimeout: return ETIMEDOUT;
    case kIOReturnBusy: case kIOReturnNotReady: return EBUSY;
    case kIOReturnNotFound: return ENOENT;
    case kIOReturnUnsupported: return ENOTSUP;
    case kIOReturnNotPermitted: return EACCES;
    default: return EIO;
    }
}
int call(uint32_t selector, const void *in, size_t n, void *out=nullptr, size_t size=0) {
    if (!state.connection || state.lost) return EIO;
    size_t actual=size;
    const uint64_t began=clock_ns();
    int e=error(IOConnectCallStructMethod(state.connection, selector, in, n,
                                         out, size?&actual:nullptr));
    if(selector==NativeAdrenoMethodSubmitResources)timings.ioctl+=clock_ns()-began;
    return !e && actual!=size ? EPROTO : e;
}
uint64_t rounded(uint64_t value, uint64_t align=HostPage) {
    return value && value<=UINT64_MAX-(align-1) ? (value+align-1)&~(align-1) : 0;
}
Buffer *buffer(uint32_t handle) {
    return handle && handle<=MaxBO && state.buffers[handle-1].kernel
        ? &state.buffers[handle-1] : nullptr;
}
// Sweet's Adreno does not snoop the CPU caches. 0172 delegates large
// imported BOs to the client (sweetimportsync=0, sweetimportmin=2048).
// libSystem sys_dcache_flush is not DMA maintenance on this iOS build.
// A55/A76 use 64-byte lines; the same EL0 operation is used by the stable
// SweetMetal2 frontend on this kernel (SCTLR_EL1.UCI permits it).
void cache_clean_invalidate(const Buffer &b) {
    // Owned BO cache policy and maintenance belong to the kernel. Only
    // imported IOSurfaces retain cached task pages requiring client walks.
    if (!b.surface) return;
    const uint64_t began=clock_ns();
    __asm__ volatile("dsb sy" ::: "memory");
    for(uint64_t at=b.cpu; at<b.cpu+b.size; at+=64)
        __asm__ volatile("dc civac, %0" :: "r"(at) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
    timings.cache+=clock_ns()-began;timings.cache_bytes+=b.size;
}
// Return GPU-written imported pages to the CPU once their last use retired.
// Another handle of the same IOSurface may still be in flight under an
// asynchronous commit; its pages stay untouched until that use retires too.
void retire_dirty(uint64_t fence) {
    // Lazy by default: a GPU-written IOSurface keeps gpu_dirty and is cleaned
    // when the CPU asks for it through this driver (CPU_PREP) or when it is
    // freed; the kernel's present acquire cleans the swap surface itself.
    // Eager walks ran on the completion thread under the transport lock and
    // blocked CoreAnimation's render thread (~10% of a 240 s sample).
    if(!state.eager_retire)return;
    for(auto &b:state.buffers) {
        if(!b.kernel || !b.gpu_dirty || b.last>fence)continue;
        bool busy=false;
        if(b.surface)for(auto &alias:state.buffers)
            if(alias.kernel && alias.surface_id==b.surface_id && alias.last>fence){busy=true;break;}
        if(busy)continue;
        cache_clean_invalidate(b);
        b.gpu_dirty=false;
    }
}
int wait(uint64_t fence, uint64_t ns) {
    if (!fence || fence<=state.completed) return 0;
    NativeAdrenoAbiWaitFence req{fence,ns};
    const uint64_t began=clock_ns();
    int e=call(NativeAdrenoMethodWaitFence,&req,sizeof req);
    timings.wait+=clock_ns()-began;
    if (!e) {
        // Only after a real GPU fence. Before-submit maintenance removed CPU
        // dirty lines, so this cannot write stale CPU data over device output.
        state.completed=std::max(state.completed,fence);
        if(!state.command_active)retire_dirty(state.completed);
    }
    return e;
}
uint64_t remaining(const drm_msm_timespec &absolute) {
    if (absolute.tv_sec<0 || absolute.tv_nsec<0 || absolute.tv_nsec>=1000000000) return 0;
    timespec now{};
    clock_gettime(CLOCK_MONOTONIC_RAW,&now); // matches donor get_abs_timeout
    if (absolute.tv_sec<now.tv_sec ||
        (absolute.tv_sec==now.tv_sec && absolute.tv_nsec<=now.tv_nsec)) return 0;
    uint64_t seconds=uint64_t(absolute.tv_sec-now.tv_sec);
    if (seconds>UINT64_MAX/1000000000ull-1) return UINT64_MAX;
    return seconds*1000000000ull+absolute.tv_nsec-now.tv_nsec;
}
// GPU VA of every live (or quarantined) buffer: iova -> size. Kept exactly in
// step with Buffer::iova so reserve() is one ordered pass over the gaps. The
// previous search rescanned all 4096 slots after every collision (quadratic in
// live buffers); with freedreno's BO cache holding thousands of buffers it was
// ~20% of BackBoard's render thread while animations allocated textures.
std::map<uint64_t,uint64_t> ranges;
uint64_t reserve(uint64_t bytes) {
    uint64_t end=state.info.va_start+state.info.va_size;
    if (end<state.info.va_start) return 0;
    uint64_t at=rounded(state.info.va_start,65536);
    auto fits=[&]{return at && at<end && bytes<=end-at && GuardBytes<=end-at-bytes;};
    for (auto &r:ranges) {
        if (!fits()) return 0;
        // Same rule as before: a guard page on both sides of every buffer.
        if (at+bytes+GuardBytes<=r.first) return at;
        uint64_t after=rounded(r.first+r.second+GuardBytes,65536);
        if (after>at) at=after;
    }
    return fits() ? at : 0;
}
// Retired owned buffers, still allocated, CPU-mapped and GPU-mapped at their
// VA. BoAlloc+BoMap+BoCpuMap cost ~0.8 ms and BoFree waits for the GPU
// (~1.3 ms, more while a frame is in flight); CoreAnimation frees and
// allocates same-sized textures every animation. Only buffers whose last GPU
// use has retired enter the pool; contents are not cleared (freedreno's own
// BO cache never clears either).
int destroy(Buffer &b);
int release(Buffer &b) {
    int e=wait(b.last,WaitNS);
    if (e) { b.closing=true; return e; } // retained/quarantined; no use-after-free
    if(b.shadow_src) {
        CFRelease(b.shadow_src);b.shadow_src=nullptr;b.shadow_id=0;b.shadow_valid=false;
        b.shadow_convert=false;b.shadow_planes=0;
    }
    if(b.gpu_dirty) {cache_clean_invalidate(b);b.gpu_dirty=false;}
    if(!b.surface && b.cpu_mapped && b.cpu_lease && b.iova && !b.closing && b.size<=PoolMaxBuffer && !state.lost) {
        while(pool_bytes+b.size>PoolBytes && !pool.empty()) {
            Buffer old=std::move(pool.front());
            pool.erase(pool.begin());pool_bytes-=old.size;
            destroy(old); // a failure leaks this one buffer's storage, never reuses it
        }
        b.metadata.clear();b.last=0;b.imported_published=false;
        pool_bytes+=b.size;pool.push_back(std::move(b));
        b=Buffer{};
        return 0;
    }
    return destroy(b);
}
int destroy(Buffer &b) {
    int e=0;
    if (b.cpu_mapped) {
        e=error(IOConnectUnmapMemory64(state.connection,b.cpu_token,
                                      mach_task_self(),b.cpu));
        if(e) { b.closing=true; return e; }
        b.cpu_mapped=false;b.cpu=0;
    }
    if (b.cpu_lease) {
        NativeAdrenoAbiBoCpuUnmap unmap{b.kernel,b.cpu_token,0};
        e=call(NativeAdrenoMethodBoCpuUnmap,&unmap,sizeof unmap);
        if(e) { b.closing=true; return e; }
        b.cpu_lease=false;
    }
    NativeAdrenoAbiBoFree req{b.kernel};
    e=call(NativeAdrenoMethodBoFree,&req,sizeof req);
    if (e) { b.closing=true; return e; }
    if (b.surface) CFRelease(b.surface);
    if (b.iova) ranges.erase(b.iova);
    b=Buffer{};
    return 0;
}
// IOSurfaceAddress on Sweet can begin inside its VM allocation. The allocation
// size includes that prefix. Verify the entire pinned range, rather than import
// unrelated pages beyond a nominal row length.
// Shadow layout of an 8-bit bi-planar YCbCr IOSurface ('420f'/'420v') whose plane rows are not 64-byte aligned
// (26b8: wallpaper planes 579x1254 with 592-byte rows). Both planes go into one owned BO: plane 0 at its own
// in-page offset from the allocation's first page, plane 1 after it at the next 4 KiB, each plane's rows rounded
// to 64 bytes. frontend/SweetNativeDrawable.m computes the same offsets for the textures it lays over this BO.
bool ycbcr_shadow_layout(IOSurfaceRef s, Buffer::ShadowPlane plane[2], uint64_t *bytes) {
    const uint64_t surface=(uintptr_t)IOSurfaceGetBaseAddress(s), base=surface&~(HostPage-1);
    uint64_t at=0;
    for(unsigned i=0;i<2;++i) {
        const uint64_t pb=(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s,i), pitch=IOSurfaceGetBytesPerRowOfPlane(s,i);
        const uint64_t rows=IOSurfaceGetHeightOfPlane(s,i), bpe=IOSurfaceGetBytesPerElementOfPlane(s,i);
        const uint64_t row_bytes=IOSurfaceGetWidthOfPlane(s,i)*bpe, aligned=(pitch+63)&~uint64_t(63);
        if(!surface || pb<surface || bpe!=(i?2u:1u) || row_bytes>pitch || pb-surface>UINT32_MAX || aligned>UINT32_MAX)
            return false;
        const uint64_t offset=i?rounded(at,4096):pb-base;
        plane[i]={uint32_t(pb-surface),uint32_t(pitch),uint32_t(offset),uint32_t(aligned),uint32_t(rows),uint32_t(row_bytes)};
        at=offset+aligned*rows;
        if(at>UINT32_MAX)return false;
    }
    *bytes=rounded(at);
    return true;
}
// 26bB: CoreAnimation imports a '420f'/'420v' surface as ONE texture of its private Metal format 500 and the
// uber shader (image function 36) samples it as (Cr, Y, Cb) = (.r, .g, .b) -- the Vulkan YCbCr channel order,
// one sample reading both planes. Importing plane 0 alone as R8 gave (Y, 0, 0) -> Cr=Y, Y=0, Cb=0 -> red app
// switcher snapshots. The frontend asks for such a texture as surface ID | YCbCrConvertFlag: an owned RGBA8 BO
// at a 64-byte-aligned pitch that shadow_refill() fills with (Cr, Y, Cb, 255) per luma texel (chroma nearest)
// whenever the surface's seed changes. IOSurface IDs are small; the flag keeps the "fd" positive.
constexpr uint32_t YCbCrConvertFlag = 0x40000000u;
bool ycbcr_convert_layout(IOSurfaceRef s, Buffer::ShadowPlane plane[2], uint64_t *bytes) {
    const uint32_t f=IOSurfaceGetPixelFormat(s);
    const uint64_t surface=(uintptr_t)IOSurfaceGetBaseAddress(s);
    if(IOSurfaceGetPlaneCount(s)!=2 || (f!=0x34323066u && f!=0x34323076u) || !surface)return false;
    const uint64_t p0=(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s,0), p1=(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s,1);
    const uint64_t w0=IOSurfaceGetWidthOfPlane(s,0), h0=IOSurfaceGetHeightOfPlane(s,0);
    const uint64_t w1=IOSurfaceGetWidthOfPlane(s,1), h1=IOSurfaceGetHeightOfPlane(s,1);
    const uint64_t r0=IOSurfaceGetBytesPerRowOfPlane(s,0), r1=IOSurfaceGetBytesPerRowOfPlane(s,1);
    if(p0<surface || p1<surface || p0-surface>UINT32_MAX || p1-surface>UINT32_MAX ||
       IOSurfaceGetBytesPerElementOfPlane(s,0)!=1 || IOSurfaceGetBytesPerElementOfPlane(s,1)!=2 ||
       !w0 || !h0 || !w1 || !h1 || w1*2<w0 || h1*2<h0 || w0>r0 || w1*2>r1 || r0>UINT32_MAX || r1>UINT32_MAX)
        return false;
    const uint64_t pitch=(w0*4+63)&~uint64_t(63);
    if(pitch>UINT32_MAX || pitch*h0>UINT32_MAX)return false;
    plane[0]={uint32_t(p0-surface),uint32_t(r0),0,uint32_t(pitch),uint32_t(h0),uint32_t(w0)};
    plane[1]={uint32_t(p1-surface),uint32_t(r1),0,0,uint32_t(h1),uint32_t(w1*2)};
    *bytes=rounded(pitch*h0);
    return true;
}
int surface_description(uint32_t id, struct redwood_drm_surface *out, IOSurfaceRef *keep=nullptr) {
    if(id&YCbCrConvertFlag) {
        IOSurfaceRef s=IOSurfaceLookup(id&~YCbCrConvertFlag);
        if(!s)return ENOENT;
        Buffer::ShadowPlane plane[2]; uint64_t bytes=0;
        if(!ycbcr_convert_layout(s,plane,&bytes) || bytes>state.params.max_bo_bytes){CFRelease(s);return EINVAL;}
        *out={id,0,bytes,0,plane[0].pitch,plane[0].row_bytes,plane[0].rows,'RGBA',0};
        if(keep){*keep=s;return 0;}
        CFRelease(s);return 0;
    }
    IOSurfaceRef s=IOSurfaceLookup(id);
    if (!s) return ENOENT;
    uint64_t pixel=(uintptr_t)IOSurfaceGetBaseAddress(s);
    uint64_t base=pixel&~(HostPage-1), bytes=rounded(IOSurfaceGetAllocSize(s));
    size_t width=IOSurfaceGetWidth(s), height=IOSurfaceGetHeight(s), pitch=IOSurfaceGetBytesPerRow(s);
    size_t planes=IOSurfaceGetPlaneCount(s);
    uint32_t format=IOSurfaceGetPixelFormat(s);
    // iOS 26 '&BGA' render targets: one linear plane with the surface's pitch, possibly after metadata
    // (frontend SweetNativeDrawable.m imports them as BGRA by the same rule); described as plane-less BGRA
    // whose rows start at plane 0. The pinned range still begins at the allocation's first page.
    uint64_t p0=(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s,0);
    if (planes==1 && format==0x26424741u && p0 && p0>=pixel &&
        IOSurfaceGetBytesPerRowOfPlane(s,0)==IOSurfaceGetBytesPerRow(s) &&
        IOSurfaceGetWidthOfPlane(s,0)==IOSurfaceGetWidth(s) && IOSurfaceGetHeightOfPlane(s,0)==IOSurfaceGetHeight(s)) {
        planes=0; format='BGRA'; pixel=p0;
    }
    // iOS 26 wallpapers/photos: 8-bit bi-planar YCbCr ('420f'/'420v'). The whole allocation is one BO; the
    // frontend lays each plane over it at that plane's offset and pitch (SweetNativeDrawable.m), so the
    // description is the allocation itself. Planes whose rows are not 64-byte aligned are read through one
    // shadow BO holding both planes (ycbcr_shadow_layout(), drmPrimeFDToHandle()).
    bool ycbcr_shadow=false;
    if (planes==2 && (format==0x34323066u || format==0x34323076u)) {
        planes=0;
        pitch=IOSurfaceGetBytesPerRowOfPlane(s,0);
        width=IOSurfaceGetWidthOfPlane(s,0); height=IOSurfaceGetHeightOfPlane(s,0);
        ycbcr_shadow=((IOSurfaceGetBytesPerRowOfPlane(s,0)|IOSurfaceGetBytesPerRowOfPlane(s,1))&63)!=0;
    }
    int e=0;
    if (!base || !bytes || bytes>state.params.max_bo_bytes || planes ||
        width>UINT32_MAX || height>UINT32_MAX || pitch>UINT32_MAX ||
        pixel-base>=bytes || (height && pitch>(bytes-(pixel-base))/height)) e=EINVAL;
    if (!e) {
        vm_address_t region=base; vm_size_t length=0;
        vm_region_basic_info_data_64_t info{};
        mach_msg_type_number_t count=VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object=MACH_PORT_NULL;
        kern_return_t kr=vm_region_64(mach_task_self(),&region,&length,VM_REGION_BASIC_INFO_64,
                             (vm_region_info_t)&info,&count,&object);
        if (object) mach_port_deallocate(mach_task_self(),object);
        if (kr || region>base || length<base-region || bytes>length-(base-region) ||
            !(info.protection&VM_PROT_READ) || !(info.protection&VM_PROT_WRITE)) e=EINVAL;
    }
    if (!e && ycbcr_shadow) {
        Buffer::ShadowPlane plane[2]; uint64_t need=0;
        if(!ycbcr_shadow_layout(s,plane,&need) || !need || need>state.params.max_bo_bytes)e=EINVAL;
        else {pixel=base+plane[0].offset;pitch=plane[0].pitch;bytes=need;}
    } else if (!e && (pitch&63)) {
        // Shadow layout (frontend SweetNativeDrawable.m uses the same rule):
        // same in-page offset, rows rounded to 64 bytes, owned storage.
        const uint64_t aligned=(uint64_t(pitch)+63)&~uint64_t(63);
        const uint64_t need=rounded((pixel-base)+aligned*height);
        if(aligned>UINT32_MAX || !need || need>state.params.max_bo_bytes)e=EINVAL;
        else {pitch=size_t(aligned);bytes=need;}
    }
    if (!e) {
        *out={id,0,bytes,uint32_t(pixel-base),uint32_t(pitch),uint32_t(width),uint32_t(height),
              format,uint32_t(planes)};
        if (keep) { *keep=s; return 0; }
    }
    CFRelease(s); return e;
}
int allocate(uint64_t bytes,uint32_t flags,IOSurfaceRef s,uint32_t *handle) {
    unsigned index=0;
    while(index<MaxBO && state.buffers[index].kernel) ++index;
    bytes=rounded(bytes);
    if (index==MaxBO) return ENOSPC;
    if (!bytes || bytes>state.params.max_bo_bytes) return EINVAL;
    if(!s) {
        for(size_t i=pool.size();i--;) if(pool[i].size==bytes) {
            pool_bytes-=bytes;++pool_hits;
            state.buffers[index]=std::move(pool[i]);
            pool.erase(pool.begin()+i);
            state.buffers[index].flags=flags;
            *handle=index+1;
            return 0;
        }
        ++pool_misses;
    }
    uint64_t va=reserve(bytes);
    if (!va) return ENOSPC;
    Buffer b;
    b.size=bytes;b.flags=flags;
    NativeAdrenoAbiBoAllocReply reply{};
    int e=0;
    if(s) {
        b.cpu=(uintptr_t)IOSurfaceGetBaseAddress(s)&~(HostPage-1);
        b.surface=s;CFRetain(s);b.surface_id=IOSurfaceGetID(s);
        if(state.command_active)for(auto &alias:state.buffers)
            if(alias.kernel && alias.surface_id==b.surface_id && alias.imported_published)
                b.imported_published=true;
        NativeAdrenoAbiBoImport req{b.cpu,bytes};
        e=call(NativeAdrenoMethodBoImport,&req,sizeof req,&reply,sizeof reply);
    } else {
        // Avoid pinning cached vm_allocate pages for every texture, shader,
        // command stream and intermediate. 0172 owns WC storage and its DMA
        // cache policy; the CPU mapping is a lease on that same allocation.
        NativeAdrenoAbiBoAlloc req{bytes,14,0};
        e=call(NativeAdrenoMethodBoAlloc,&req,sizeof req,&reply,sizeof reply);
    }
    if(!e && !reply.handle)e=EPROTO;
    if(e) {if(b.surface)CFRelease(b.surface);return e;}
    b.kernel=reply.handle;state.buffers[index]=std::move(b);
    Buffer &live=state.buffers[index];
    NativeAdrenoAbiBoMap map{live.kernel,va,3,0,2,0};NativeAdrenoAbiBoMapReply mapped{};
    e=call(NativeAdrenoMethodBoMap,&map,sizeof map,&mapped,sizeof mapped);
    if(!e && mapped.iova!=va)e=EPROTO;
    if(e){release(live);return e;}
    live.iova=va;ranges[va]=live.size;
    if(!s) {
        NativeAdrenoAbiBoCpuMap req{live.kernel};
        NativeAdrenoAbiBoCpuMapReply mapped_cpu{};
        e=call(NativeAdrenoMethodBoCpuMap,&req,sizeof req,&mapped_cpu,sizeof mapped_cpu);
        if(!e) {
            live.cpu_token=mapped_cpu.token;live.cpu_lease=true;
            if(mapped_cpu.reserved || mapped_cpu.bytes<bytes)e=EPROTO;
        }
        mach_vm_address_t address=0;mach_vm_size_t length=0;
        if(!e) {
            // Preserve the descriptor's cache policy, including the kernel's
            // cacheable fallback if its WC pool cannot satisfy an allocation.
            e=error(IOConnectMapMemory64(state.connection,live.cpu_token,
                       mach_task_self(),&address,&length,kIOMapAnywhere|kIOMapDefaultCache));
            if(!e) {
                live.cpu=address;live.cpu_mapped=true;
                if(!address || length<bytes)e=EPROTO;
            }
        }
        if(e){release(live);return e;}
    }
    *handle=index+1;
    return 0;
}
int open_gpu() {
    if(state.lost)return EIO;
    if(state.connection)return 0;
    io_service_t service=IOServiceGetMatchingService(0,IOServiceMatching("AGXAccelerator"));
    if(!service)return ENODEV;
    int e=error(IOServiceOpen(service,mach_task_self(),0x4147,&state.connection));
    IOObjectRelease(service);
    if(e){state.connection=0;return e;}
    e=call(NativeAdrenoMethodQuery,nullptr,0,&state.params,sizeof state.params);
    if(!e)e=call(NativeAdrenoMethodGetDeviceInfo,nullptr,0,&state.info,sizeof state.info);
    unsigned chip=state.info.chip_id;
    unsigned gpu=((chip>>24)&255)*100+((chip>>16)&255)*10+((chip>>8)&255);
    unsigned flags=NativeAdrenoParamsHostImport|NativeAdrenoParamsSubmitResources|NativeAdrenoParamsCompositor4096;
    if(!e && (state.params.abi_version!=1 || gpu!=618 || !state.info.gmem_bytes ||
              !state.info.va_size || (state.params.flags&flags)!=flags)) e=ENOTSUP;
    if(e){IOServiceClose(state.connection);state.connection=0;return e;}
    fprintf(stderr,"SweetNativeMetal: NativeAdreno ABI1 A618 chip=%#x GMEM=%u scoped residency\n",chip,state.info.gmem_bytes);
    fprintf(stderr,"SweetNativeMetal: kernel-owned private BOs with CPU map leases; imported IOSurface cache ownership\n");
    fprintf(stderr,"SweetNativeMetal: deferred submit merging includes shared IOSurface batches, <=16 IBs/1024 BOs per ioctl\n");
    state.strict_publish=!access("/var/local/sweet-native-strict-publish",F_OK);
    state.always_clean=!access("/var/local/sweet-native-always-clean",F_OK);
    state.eager_retire=!access("/var/local/sweet-native-eager-retire",F_OK);
    fprintf(stderr,state.eager_retire
        ?"SweetNativeMetal: GPU-written IOSurfaces are cache-cleaned as soon as they retire (eager-retire)\n"
        :"SweetNativeMetal: GPU-written IOSurfaces are cache-cleaned on CPU access or free, not on retire "
         "(touch /var/local/sweet-native-eager-retire to restore)\n");
    fprintf(stderr,state.always_clean
        ?"SweetNativeMetal: every IOSurface publication cleans (always-clean)\n"
        :"SweetNativeMetal: IOSurface publication skips the clean while the surface seed is unchanged "
         "(touch /var/local/sweet-native-always-clean to restore)\n");
    fprintf(stderr,state.strict_publish
        ?"SweetNativeMetal: strict IOSurface publication (waits all GPU work, cleans every surface)\n"
        :"SweetNativeMetal: IOSurface publication without GPU drain; surfaces with an in-flight GPU write are not cleaned "
         "(touch /var/local/sweet-native-strict-publish to restore)\n");
    return 0;
}
int get_param(drm_msm_param *p) {
    if(p->pipe!=MSM_PIPE_3D0 && p->pipe!=MSM_PIPE_NONE)return EINVAL;
    NativeAdrenoAbiTimestamp time{};NativeAdrenoAbiStatus status{};
    switch(p->param) {
    case MSM_PARAM_GPU_ID:p->value=618;break;
    case MSM_PARAM_CHIP_ID:p->value=state.info.chip_id;break;
    case MSM_PARAM_GMEM_SIZE:p->value=state.info.gmem_bytes;break;
    case MSM_PARAM_GMEM_BASE:p->value=state.info.gmem_base;break;
    case MSM_PARAM_MAX_FREQ:return ENOTSUP; // frequency is not exposed by this ABI
    case MSM_PARAM_TIMESTAMP: {
        int e=call(NativeAdrenoMethodGetTimestamp,nullptr,0,&time,sizeof time);
        if(e)return e;
        p->value=time.ticks;break;
    }
    case MSM_PARAM_FAULTS:case MSM_PARAM_SUSPENDS: {
        int e=call(NativeAdrenoMethodGetStatus,nullptr,0,&status,sizeof status);
        if(e)return e;
        p->value=p->param==MSM_PARAM_FAULTS?status.faults:status.suspends;break;
    }
    case MSM_PARAM_PRIORITIES:p->value=1;break;
    case MSM_PARAM_VA_START:p->value=state.info.va_start;break;
    case MSM_PARAM_VA_SIZE:p->value=state.info.va_size;break;
    case MSM_PARAM_HIGHEST_BANK_BIT:p->value=state.info.highest_bank_bit;break;
    case MSM_PARAM_UBWC_SWIZZLE:p->value=state.info.ubwc_swizzle;break;
    case MSM_PARAM_MACROTILE_MODE:p->value=state.info.macrotile_mode;break;
    #ifdef MSM_PARAM_UCHE_TRAP_BASE
    case MSM_PARAM_UCHE_TRAP_BASE:p->value=state.info.uche_trap_base;break;
    #endif
    default:return ENOTSUP;
    }
    return 0;
}
// Copy the shadow's source rows to the rounded pitch when the surface changed.
// Writers bump the seed on unlock; the seed is read before the copy, so a write
// racing the copy leaves a newer seed and a refill at the next submission. A
// shadow the GPU may still be reading is waited for first (glyph surfaces
// change rarely). The submission's dsb publishes these WC stores.
int shadow_refill(Buffer &b) {
    uint32_t seed=IOSurfaceGetSeed(b.shadow_src);
    if(b.shadow_valid && seed==b.shadow_seed)return 0;
    if(b.last>state.completed) {
        int e=wait(b.last,WaitNS);
        if(e)return e;
        ++state.shadow_waits;
    }
    // Best effort: a refused lock still copies (the seed check repeats it).
    const bool locked=IOSurfaceLock(b.shadow_src,kIOSurfaceLockReadOnly,nullptr)==kIOReturnSuccess;
    const uint8_t *src=(const uint8_t*)IOSurfaceGetBaseAddress(b.shadow_src);
    bool fits=b.shadow_planes>0;
    for(uint32_t i=0;i<b.shadow_planes;++i) {
        const Buffer::ShadowPlane &p=b.shadow_plane[i];
        fits=fits && uint64_t(p.offset)+uint64_t(p.pitch)*p.rows<=b.size;
    }
    uint64_t copied=0;
    if(src && b.cpu && fits && b.shadow_convert && b.shadow_planes==2) {
        const Buffer::ShadowPlane &l=b.shadow_plane[0], &c=b.shadow_plane[1];
        for(uint32_t y=0;y<l.rows;++y) {
            const uint8_t *luma=src+l.src_offset+uint64_t(y)*l.src_pitch;
            const uint8_t *cbcr=src+c.src_offset+uint64_t(y>>1)*c.src_pitch;
            uint32_t *dst=(uint32_t*)((uint8_t*)(uintptr_t)b.cpu+uint64_t(y)*l.pitch);
            for(uint32_t x=0;x<l.row_bytes;++x) {
                const uint8_t *cc=cbcr+(x&~1u);   // NV12 order: Cb, Cr
                dst[x]=uint32_t(cc[1])|uint32_t(luma[x])<<8|uint32_t(cc[0])<<16|0xff000000u;
            }
        }
        copied=uint64_t(l.row_bytes)*4*l.rows;
    } else if(src && b.cpu && fits)
        for(uint32_t i=0;i<b.shadow_planes;++i) {
            const Buffer::ShadowPlane &p=b.shadow_plane[i];
            uint8_t *dst=(uint8_t*)(uintptr_t)b.cpu+p.offset;
            const uint8_t *from=src+p.src_offset;
            for(uint32_t y=0;y<p.rows;++y)
                memcpy(dst+uint64_t(y)*p.pitch,from+uint64_t(y)*p.src_pitch,p.row_bytes);
            copied+=uint64_t(p.row_bytes)*p.rows;
        }
    if(locked)IOSurfaceUnlock(b.shadow_src,kIOSurfaceLockReadOnly,nullptr);
    if(!src || !b.cpu || !fits)return 0;    // keeps the old contents; never fails the frame
    b.shadow_seed=seed;b.shadow_valid=true;
    ++state.shadow_fills;state.shadow_fill_bytes+=copied;
    return 0;
}
int submit(drm_msm_gem_submit *s) {
    SubmitTiming profile;
    if ((s->flags&MSM_PIPE_ID_MASK)!=MSM_PIPE_3D0 ||
        (s->flags&~(MSM_PIPE_ID_MASK|MSM_SUBMIT_NO_IMPLICIT)) ||
        s->nr_in_syncobjs || s->nr_out_syncobjs || s->pad || !s->bos || !s->cmds ||
        !s->nr_bos || s->nr_bos>NativeAdrenoAbiMaxResources ||
        !s->nr_cmds || s->nr_cmds>4096 || !state.next) return EINVAL;
    if(s->queueid && std::find(std::begin(state.queues),std::end(state.queues),s->queueid)==std::end(state.queues))return EINVAL;
    auto req=new(std::nothrow) NativeAdrenoAbiSubmitResources{};
    if(!req)return ENOMEM;
    auto bos=(drm_msm_gem_submit_bo *)(uintptr_t)s->bos;
    auto cmds=(drm_msm_gem_submit_cmd *)(uintptr_t)s->cmds;
    std::vector<NativeAdrenoAbiSubmitBuffer> ibs;
    req->resource_count=s->nr_bos;req->commands.queue=s->queueid;
    int e=0;
    for(unsigned i=0;i<s->nr_bos && !e;++i) {
        Buffer *b=buffer(bos[i].handle);
        uint32_t access=bos[i].flags&(MSM_SUBMIT_BO_READ|MSM_SUBMIT_BO_WRITE);
        if(!b || b->closing || !access || (bos[i].flags&~MSM_SUBMIT_BO_FLAGS)) {e=EINVAL;break;}
        for(unsigned j=0;j<i;++j)if(bos[j].handle==bos[i].handle)e=EINVAL;
        req->resources[i]={b->kernel,access,0};bos[i].presumed=b->iova;
    }
    // Validate the complete list before sending its first 16-IB chunk.
    for(unsigned i=0;i<s->nr_cmds && !e;++i) {
        auto &c=cmds[i];
        if(c.submit_idx>=s->nr_bos || c.nr_relocs || c.pad ||
           !c.size || (c.size&3) || (c.submit_offset&3) || c.size>0xfffff*4) {e=EINVAL;break;}
        auto b=buffer(bos[c.submit_idx].handle);
        if(!b || !(bos[c.submit_idx].flags&MSM_SUBMIT_BO_READ) ||
           c.submit_offset>b->size || c.size>b->size-c.submit_offset) {e=EINVAL;break;}
        if(c.type==MSM_SUBMIT_CMD_BUF || c.type==MSM_SUBMIT_CMD_CTX_RESTORE_BUF)
            ibs.push_back({b->kernel,b->iova+c.submit_offset,c.size});
        else if(c.type!=MSM_SUBMIT_CMD_IB_TARGET_BUF)e=ENOTSUP;
    }
    if(ibs.empty()&&!e)e=EINVAL;
    timings.ibs+=ibs.size();
    for(unsigned i=0;i<s->nr_bos && !e;++i) {
        Buffer *b=buffer(bos[i].handle);
        if(b->shadow_src)e=shadow_refill(*b);
    }
    // Publish cached pages once per Metal transaction. Intermediate waits do
    // not transfer ownership back to the CPU.
    // Strict mode: a first publication waits all previous GPU work, including
    // possible alternate handles for these pages, then cleans every surface.
    // Default: the clean only exists for CPU-written contents (app layers).
    // A surface whose GPU write is still in flight (CoreAnimation's render
    // targets, the swap surface) has no CPU data to publish, and cleaning it
    // before the write retires could never be both correct and useful, so it
    // is skipped and nothing waits. Its stale lines are dropped when its last
    // use retires (retire_dirty), on CPU access (CPU_PREP) or by the kernel's
    // present acquire. Cleaning a surface the GPU is only reading is harmless.
    auto writing=[](const Buffer &b) {
        for(auto &alias:state.buffers)
            if(alias.kernel && alias.surface_id==b.surface_id && alias.gpu_dirty && alias.last>state.completed)return true;
        return false;
    };
    bool publish=false;
    if(!e)for(unsigned i=0;i<s->nr_bos;++i) {
        Buffer *b=buffer(bos[i].handle);
        publish |= b->surface && (!state.command_active || !b->imported_published);
    }
    if(!e && publish && state.strict_publish && !state.fences.empty())e=wait(state.fences.back().kernel,WaitNS);
    if(!e)for(unsigned i=0;i<s->nr_bos;++i) {
        Buffer *b=buffer(bos[i].handle);
        if(!b->surface || (state.command_active && b->imported_published))continue;
        if(state.strict_publish || !writing(*b)) {
            const uint32_t seed=IOSurfaceGetSeed(b->surface);
            if(state.always_clean || !b->clean_valid || b->clean_seed!=seed){cache_clean_invalidate(*b);++state.cleans;}
            else ++state.clean_skips;
            b->clean_seed=seed;b->clean_valid=true;
        }
        if(state.command_active)for(auto &alias:state.buffers)
            if(alias.kernel && alias.surface_id==b->surface_id)alias.imported_published=true;
    }
    // WC command and upload stores must reach the device even when the
    // submission contains no imported surfaces and hence no cache walk.
    __asm__ volatile("dsb sy" ::: "memory");
    uint64_t last=0;
    for(size_t at=0;at<ibs.size() && !e;at+=NativeAdrenoAbiMaxBuffers) {
        req->commands.buffer_count=std::min<size_t>(NativeAdrenoAbiMaxBuffers,ibs.size()-at);
        std::copy_n(ibs.data()+at,req->commands.buffer_count,req->commands.buffers);
        NativeAdrenoAbiSubmitReply reply{};
        e=call(NativeAdrenoMethodSubmitResources,req,sizeof *req,&reply,sizeof reply);
        // Kernel job slots are full: retire the oldest outstanding job, not
        // the whole queue, so asynchronous commits keep the GPU fed.
        // 26bB: 10 backboardd command buffers per boot failed with EBUSY (lost frames): the slots were held by
        // another client's jobs (extra Metal slots: SpringBoard, PosterBoard), so there was no own fence to
        // wait for and the loop gave up at once. Without an own fence it now backs off 500 us and retries,
        // up to 200 times (100 ms), before the command buffer fails.
        for(unsigned tries=0,foreign=0;e==EBUSY && tries<8 && foreign<200;) {
            uint64_t oldest=0;
            for(auto &f:state.fences)if(f.kernel>state.completed && (!oldest || f.kernel<oldest))oldest=f.kernel;
            if(!oldest && last>state.completed)oldest=last;
            if(!oldest) {
                ++foreign;++state.busy_backoffs;usleep(500);
                e=call(NativeAdrenoMethodSubmitResources,req,sizeof *req,&reply,sizeof reply);
                continue;
            }
            ++tries;
            e=wait(oldest,WaitNS);
            if(!e)e=call(NativeAdrenoMethodSubmitResources,req,sizeof *req,&reply,sizeof reply);
        }
        if(!e && !reply.fence)e=EPROTO;
        if(!e) {
            last=reply.fence;
            for(unsigned i=0;i<s->nr_bos;++i) {
                Buffer *b=buffer(bos[i].handle);
                b->last=last;
                b->gpu_dirty |= (bos[i].flags&MSM_SUBMIT_BO_WRITE)!=0;
            }
        }
    }
    delete req;
    if(e) {
        // A partially submitted command buffer has an uncertain result. Preserve
        // its pages and refuse further work, rather than report a completed frame.
        if(last)state.lost=true;
        return e;
    }
    state.fences.erase(std::remove_if(state.fences.begin(),state.fences.end(),[](const Fence &f){return f.kernel<=state.completed;}),state.fences.end());
    if(state.fences.size()>=4096){state.lost=true;return ENOSPC;}
    state.submitted=std::max(state.submitted,last);
    s->fence=state.next++;s->fence_fd=-1;
    state.fences.push_back({s->fence,last,s->queueid});
    return 0;
}
int dispatch(unsigned n,void *data,size_t bytes) {
    if(!data)return EINVAL;
#define REQUEST(sel,type,body) case sel: { if(bytes!=sizeof(type))return EINVAL;auto p=(type*)data;body; }
    switch(n) {
    REQUEST(DRM_MSM_GET_PARAM,drm_msm_param,return get_param(p))
    REQUEST(DRM_MSM_GEM_NEW,drm_msm_gem_new,
        if(p->flags&~(MSM_BO_WC|MSM_BO_SCANOUT|MSM_BO_GPU_READONLY))return ENOTSUP;
        return allocate(p->size,p->flags,nullptr,&p->handle))
    REQUEST(DRM_MSM_GEM_INFO,drm_msm_gem_info,
        auto b=buffer(p->handle);if(!b || b->closing)return ENOENT;
        switch(p->info){
        case MSM_INFO_GET_OFFSET:p->value=uint64_t(p->handle)*HostPage;return 0;
        case MSM_INFO_GET_IOVA:p->value=b->iova;return 0;
        case MSM_INFO_GET_FLAGS:p->value=b->flags;return 0;
        case MSM_INFO_SET_NAME:return 0; // diagnostic only, no cache/layout effect
        case MSM_INFO_SET_METADATA:
            if(p->len>4096 || (p->len&&!p->value))return EINVAL;
            if(p->len)b->metadata.assign((uint8_t*)(uintptr_t)p->value,(uint8_t*)(uintptr_t)p->value+p->len);
            else b->metadata.clear();return 0;
        case MSM_INFO_GET_METADATA:
            if(!p->value){p->len=b->metadata.size();return 0;}
            if(p->len<b->metadata.size())return ENOSPC;
            if(!b->metadata.empty())memcpy((void*)(uintptr_t)p->value,b->metadata.data(),b->metadata.size());
            p->len=b->metadata.size();return 0;
        default:return ENOTSUP;})
    REQUEST(DRM_MSM_GEM_CPU_PREP,drm_msm_gem_cpu_prep,
        auto b=buffer(p->handle);if(!b)return ENOENT;
        if(p->op&~MSM_PREP_FLAGS)return EINVAL;
        // The CPU may write these pages now without touching the seed.
        b->clean_valid=false;
        if(b->surface)for(auto &alias:state.buffers)
            if(alias.kernel && alias.surface_id==b->surface_id)alias.clean_valid=false;
        uint64_t last=b->last;bool dirty=b->gpu_dirty;
        if(b->surface)for(auto &alias:state.buffers)
            if(alias.kernel && alias.surface_id==b->surface_id) {
                last=std::max(last,alias.last);dirty|=alias.gpu_dirty;
            }
        int e=wait(last,p->op&MSM_PREP_NOSYNC?0:remaining(p->timeout));
        if(!e) {
            if(dirty)cache_clean_invalidate(*b);
            b->gpu_dirty=false;
            for(auto &alias:state.buffers)if(alias.kernel && b->surface && alias.surface_id==b->surface_id) {
                alias.imported_published=false;alias.gpu_dirty=false;
            }
        }
        return (e==ETIMEDOUT && (p->op&MSM_PREP_NOSYNC))?EBUSY:e)
    REQUEST(DRM_MSM_GEM_CPU_FINI,drm_msm_gem_cpu_fini,return buffer(p->handle)?0:ENOENT)
    REQUEST(DRM_MSM_GEM_SUBMIT,drm_msm_gem_submit,return submit(p))
    REQUEST(DRM_MSM_WAIT_FENCE,drm_msm_wait_fence,
        if(p->flags&~MSM_WAIT_FENCE_FLAGS)return EINVAL;
        for(auto &f:state.fences)if(f.number==p->fence)return f.queue==p->queueid?wait(f.kernel,remaining(p->timeout)):EINVAL;
        return p->fence && p->fence<state.next?0:EINVAL)
    REQUEST(DRM_MSM_GEM_MADVISE,drm_msm_gem_madvise,
        if(!buffer(p->handle) || p->madv>MSM_MADV_DONTNEED)return EINVAL;
        p->retained=1;return 0) // storage remains pinned until GEM_CLOSE
    REQUEST(DRM_MSM_SUBMITQUEUE_NEW,drm_msm_submitqueue,
        if(p->flags || p->prio)return ENOTSUP;
        unsigned i=0;while(i<NativeAdrenoAbiMaxQueues && state.queues[i])++i;
        if(i==NativeAdrenoAbiMaxQueues)return ENOSPC;
        NativeAdrenoAbiQueue request{};NativeAdrenoAbiQueueReply reply{};
        int e=call(NativeAdrenoMethodCreateQueue,&request,sizeof request,&reply,sizeof reply);
        if(e)return e;if(!reply.queue || reply.queue>UINT32_MAX)return EPROTO;
        state.queues[i]=reply.queue;p->id=reply.queue;return 0)
    case DRM_MSM_SUBMITQUEUE_CLOSE: {
        if(bytes!=sizeof(uint32_t))return EINVAL;
        uint32_t id=*(uint32_t*)data;
        for(auto &queue:state.queues)if(queue && queue==id){
            NativeAdrenoAbiQueue request{queue};int e=call(NativeAdrenoMethodCloseQueue,&request,sizeof request);
            if(!e)queue=0;
            return e;
        }return ENOENT;
    }
    default:return ENOTSUP;
    }
#undef REQUEST
}
} // namespace

// Called only by the native backend's serialized command transaction.
extern "C" int sweet_transport_command_begin(void) {
    Lock lock;if(state.command_active)return EBUSY;
    for(auto &b:state.buffers)b.imported_published=false;
    state.command_active=true;return 0;
}
extern "C" void sweet_transport_command_end(void) {
    Lock lock;state.command_active=false;
    // Only pages whose GPU use has retired. Never acquire unfinished pages
    // after an error/timeout; those retain dirty state and stay pinned.
    retire_dirty(state.completed);
}
// Newest kernel fence handed to the GPU. One ring retires in order, so it
// covers every submission of the command that has just been flushed.
extern "C" uint64_t sweet_transport_submitted_fence(void) {
    Lock lock;return state.submitted;
}
// Non-blocking completion check for the asynchronous completion thread. The
// kernel call runs without the transport lock, so a check never stalls the
// render thread's submissions; only the bookkeeping after it is serialized.
extern "C" int sweet_transport_poll_fence(uint64_t fence) {
    io_connect_t connection;
    {
        Lock lock;
        if(!state.connection || state.lost)return EIO;
        if(!fence || fence<=state.completed){retire_dirty(state.completed);return 0;}
        connection=state.connection;
    }
    NativeAdrenoAbiWaitFence req{fence,0};
    int e=error(IOConnectCallStructMethod(connection,NativeAdrenoMethodWaitFence,&req,sizeof req,nullptr,nullptr));
    if(e)return e;
    Lock lock;
    state.completed=std::max(state.completed,fence);
    retire_dirty(state.completed);
    return 0;
}

// Diagnostics without a rebuild: /var/local/sweet-native-env holds KEY=VALUE
// lines for Mesa (FD_*, IR3_*, MESA_*, GALLIUM_*), applied before freedreno
// reads them at screen creation. Absent file = no change.
static void load_debug_environment() {
    static bool loaded=false;
    if(loaded)return;
    loaded=true;
    FILE *f=fopen("/var/local/sweet-native-env","r");
    if(!f)return;
    char line[512];
    while(fgets(line,sizeof line,f)) {
        line[strcspn(line,"\r\n")]=0;
        char *eq=strchr(line,'=');
        if(!eq || line[0]=='#')continue;
        *eq=0;
        if(strncmp(line,"FD_",3) && strncmp(line,"IR3_",4) && strncmp(line,"MESA_",5) && strncmp(line,"GALLIUM_",8)) {
            fprintf(stderr,"SweetNativeMetal: env file ignores %s\n",line);continue;
        }
        setenv(line,eq+1,1);
        fprintf(stderr,"SweetNativeMetal: env file sets %s=%s\n",line,eq+1);
    }
    fclose(f);
}
extern "C" int drmOpenWithType(const char *name,const char *,int type) {
    Lock lock;
    load_debug_environment();
    if(!name || strcmp(name,"msm") || type!=DRM_NODE_RENDER){errno=ENODEV;return -1;}
    int e=open_gpu();
    if(e){errno=e;return -1;}
    if(state.fd<0)state.fd=open("/dev/null",O_RDWR|O_CLOEXEC);
    return state.fd<0?-1:dup(state.fd);
}
extern "C" int drmIoctl(int,unsigned long request,void *data) {
    Lock lock;int e;
    unsigned number=request&0xff;
    if(number==0x09) {
        auto p=(drm_gem_close*)data;auto b=p?buffer(p->handle):nullptr;
        e=IOCPARM_LEN(request)!=sizeof(*p)?EINVAL:b?release(*b):ENOENT;
    } else e=number>=DRM_COMMAND_BASE?dispatch(number-DRM_COMMAND_BASE,data,IOCPARM_LEN(request)):ENOTSUP;
    if(e){errno=e;return -1;}return 0;
}
extern "C" int drmCommandWriteRead(int,unsigned long index,void *data,unsigned long bytes) {
    Lock lock;int e=0;
    try {e=dispatch(index,data,bytes);}
    catch(const std::bad_alloc &) {state.lost=true;e=ENOMEM;}
    if(e)errno=e;
    return -e;
}
extern "C" int drmCommandWrite(int fd,unsigned long index,const void *data,unsigned long bytes) {
    return drmCommandWriteRead(fd,index,const_cast<void*>(data),bytes);
}
extern "C" drmVersionPtr drmGetVersion(int) {
    auto v=(drmVersionPtr)calloc(1,sizeof(drmVersion));
    if(v){v->version_major=1;v->version_minor=7;v->name=strdup("msm");v->name_len=3;
          v->date=strdup("");v->desc=strdup("Sweet NativeAdreno ABI1");
          if(!v->name || !v->date || !v->desc){drmFreeVersion(v);return nullptr;}
          v->desc_len=strlen(v->desc);}
    return v;
}
extern "C" void drmFreeVersion(drmVersionPtr v) {
    if(v){free(v->name);free(v->date);free(v->desc);free(v);}
}
extern "C" int drmGetCap(int,uint64_t,uint64_t *value) {
    if(!value){errno=EINVAL;return -1;}*value=0;return 0;
}
extern "C" void *redwood_drm_map(int,uint32_t handle,uint64_t bytes) {
    Lock lock;auto b=buffer(handle);
    if(!b || b->closing || bytes>b->size){errno=EINVAL;return MAP_FAILED;}
    return (void*)(uintptr_t)b->cpu;
}
extern "C" void redwood_drm_unmap(int,uint32_t,void *) {} // owned until GEM_CLOSE
extern "C" int redwood_drm_surface(int,struct redwood_drm_surface *surface) {
    Lock lock;
    int e=surface?surface_description(surface->surface,surface):EINVAL;
    if(e){errno=e;return -1;}return 0;
}
extern "C" int drmPrimeFDToHandle(int,int surface,uint32_t *handle) {
    Lock lock;if(!handle){errno=EINVAL;return -1;}
    // freedreno's handle table serializes repeat imports of one surface.
    for(unsigned i=0;i<MaxBO;++i)if(state.buffers[i].surface && !state.buffers[i].closing &&
        IOSurfaceGetID(state.buffers[i].surface)==uint32_t(surface)){*handle=i+1;return 0;}
    for(unsigned i=0;i<MaxBO;++i)if(state.buffers[i].shadow_src && !state.buffers[i].closing &&
        state.buffers[i].shadow_id==uint32_t(surface)){*handle=i+1;return 0;}
    struct redwood_drm_surface info{};IOSurfaceRef s=nullptr;
    int e=surface_description(surface,&info,&s);
    if(!e && (uint32_t(surface)&YCbCrConvertFlag)) {
        Buffer::ShadowPlane plane[2]; uint64_t need=0;
        e=ycbcr_convert_layout(s,plane,&need) && need==info.size?0:EINVAL;
        if(!e)e=allocate(info.size,MSM_BO_WC,nullptr,handle);
        Buffer *b=e?nullptr:buffer(*handle);
        if(b) {
            b->shadow_src=s;CFRetain(s);b->shadow_id=uint32_t(surface);
            b->shadow_plane[0]=plane[0];b->shadow_plane[1]=plane[1];b->shadow_planes=2;
            b->shadow_convert=true;b->shadow_valid=false;
            static unsigned logged;
            if(logged++<8)fprintf(stderr,"SweetNativeMetal: YCbCr IOSurface %u converted to (Cr, Y, Cb) RGBA8 shadow BO %u: %ux%u rows %u -> %u bytes\n",
                uint32_t(surface)&~YCbCrConvertFlag,*handle,plane[0].row_bytes,plane[0].rows,plane[0].src_pitch,plane[0].pitch);
        }
        CFRelease(s);
        if(e){errno=e;return -1;}return 0;
    }
    // a bi-planar YCbCr surface (the only 2-plane kind surface_description accepts) has a row pitch per plane
    const bool ycbcr=s && IOSurfaceGetPlaneCount(s)==2;
    const size_t src_pitch=!s?0:ycbcr?(IOSurfaceGetBytesPerRowOfPlane(s,0)|IOSurfaceGetBytesPerRowOfPlane(s,1)):
                              IOSurfaceGetBytesPerRow(s);
    if(!e && ycbcr && (src_pitch&63)) {
        Buffer::ShadowPlane plane[2]; uint64_t need=0;
        e=ycbcr_shadow_layout(s,plane,&need) && need==info.size?0:EINVAL;
        if(!e)e=allocate(info.size,MSM_BO_WC,nullptr,handle);
        Buffer *b=e?nullptr:buffer(*handle);
        if(b) {
            b->shadow_src=s;CFRetain(s);b->shadow_id=uint32_t(surface);
            b->shadow_plane[0]=plane[0];b->shadow_plane[1]=plane[1];b->shadow_planes=2;b->shadow_valid=false;
            static unsigned logged;
            if(logged++<8)fprintf(stderr,"SweetNativeMetal: YCbCr shadow BO %u for IOSurface %u: planes at %u/%u, rows %u/%u -> %u/%u bytes\n",
                *handle,uint32_t(surface),plane[0].offset,plane[1].offset,plane[0].src_pitch,plane[1].src_pitch,
                plane[0].pitch,plane[1].pitch);
        }
    } else if(!e && (src_pitch&63)) {
        // Owned BO; the surface stays retained as the shadow's source.
        e=allocate(info.size,MSM_BO_WC,nullptr,handle);
        Buffer *b=e?nullptr:buffer(*handle);
        const size_t bpe=IOSurfaceGetBytesPerElement(s);
        if(b && bpe && info.width<=src_pitch/bpe) {
            b->shadow_src=s;CFRetain(s);b->shadow_id=uint32_t(surface);
            b->shadow_plane[0]={0,uint32_t(src_pitch),info.offset,info.row_bytes,info.height,uint32_t(info.width*bpe)};
            b->shadow_planes=1;b->shadow_valid=false;
            static unsigned logged;
            if(logged++<16)fprintf(stderr,"SweetNativeMetal: shadow BO %u for IOSurface %u %ux%u rows %zu -> %u bytes\n",
                *handle,uint32_t(surface),info.width,info.height,src_pitch,info.row_bytes);
        } else if(b) {release(*b);e=EINVAL;}
    } else if(!e)e=allocate(info.size,MSM_BO_WC,s,handle);
    if(s)CFRelease(s);
    if(e){errno=e;return -1;}return 0;
}
static int unsupported(){errno=ENOTSUP;return -1;}
extern "C" int drmPrimeHandleToFD(int,uint32_t,uint32_t,int*){return unsupported();}
extern "C" int drmSyncobjCreate(int,uint32_t,uint32_t*){return unsupported();}
extern "C" int drmSyncobjDestroy(int,uint32_t){return unsupported();}
extern "C" int drmSyncobjReset(int,const uint32_t*,uint32_t){return unsupported();}
extern "C" int drmSyncobjFDToHandle(int,int,uint32_t*){return unsupported();}
extern "C" int drmSyncobjHandleToFD(int,uint32_t,int*){return unsupported();}
extern "C" int drmSyncobjImportSyncFile(int,uint32_t,int){return unsupported();}
extern "C" int drmSyncobjExportSyncFile(int,uint32_t,int*){return unsupported();}
