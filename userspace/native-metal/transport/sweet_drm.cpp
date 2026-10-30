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
#include <new>
#include <vector>

namespace {
constexpr unsigned MaxBO = 4096;
constexpr uint64_t HostPage = 16384, GuardBytes = HostPage;
constexpr uint64_t WaitNS = 5000000000ull;
struct Buffer {
    uint64_t kernel = 0, iova = 0, size = 0, cpu = 0, last = 0;
    IOSurfaceRef surface = nullptr;
    uint32_t flags = 0;
    bool own = false, closing = false;
    std::vector<uint8_t> metadata;
};
struct Fence { uint32_t number; uint64_t kernel, queue; };
struct State {
    io_connect_t connection = 0;
    int fd = -1;
    NativeAdrenoAbiParams params{};
    NativeAdrenoAbiDeviceInfo info{};
    Buffer buffers[MaxBO];
    std::vector<Fence> fences;
    uint64_t completed = 0;
    uint32_t next = 1;
    uint64_t queues[NativeAdrenoAbiMaxQueues]{};
    bool lost = false;
};
// Kept for process lifetime: closing one Gallium fd must not invalidate another
// screen or IOSurface. In-flight storage is never freed after an unknown timeout.
State state;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
struct Lock { Lock(){pthread_mutex_lock(&mutex);} ~Lock(){pthread_mutex_unlock(&mutex);} };
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
    int e=error(IOConnectCallStructMethod(state.connection, selector, in, n,
                                         out, size?&actual:nullptr));
    return !e && actual!=size ? EPROTO : e;
}
uint64_t rounded(uint64_t value, uint64_t align=HostPage) {
    return value && value<=UINT64_MAX-(align-1) ? (value+align-1)&~(align-1) : 0;
}
Buffer *buffer(uint32_t handle) {
    return handle && handle<=MaxBO && state.buffers[handle-1].kernel
        ? &state.buffers[handle-1] : nullptr;
}
int wait(uint64_t fence, uint64_t ns) {
    if (!fence || fence<=state.completed) return 0;
    NativeAdrenoAbiWaitFence req{fence,ns};
    int e=call(NativeAdrenoMethodWaitFence,&req,sizeof req);
    if (!e) state.completed=std::max(state.completed,fence);
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
uint64_t reserve(uint64_t bytes) {
    uint64_t end=state.info.va_start+state.info.va_size;
    if (end<state.info.va_start) return 0;
    uint64_t at=rounded(state.info.va_start,65536);
    for (unsigned tries=0;tries<=MaxBO;++tries) {
        if (!at || at>=end || bytes>end-at || GuardBytes>end-at-bytes) return 0;
        uint64_t next=at;
        for (auto &b:state.buffers) if (b.kernel && b.iova) {
            if (at<b.iova+b.size+GuardBytes && b.iova<at+bytes+GuardBytes)
                next=std::max(next,rounded(b.iova+b.size+GuardBytes,65536));
        }
        if (next==at) return at;
        at=next;
    }
    return 0;
}
int release(Buffer &b) {
    int e=wait(b.last,WaitNS);
    if (e) { b.closing=true; return e; } // retained/quarantined; no use-after-free
    NativeAdrenoAbiBoFree req{b.kernel};
    e=call(NativeAdrenoMethodBoFree,&req,sizeof req);
    if (e) { b.closing=true; return e; }
    if (b.own) vm_deallocate(mach_task_self(),b.cpu,b.size);
    if (b.surface) CFRelease(b.surface);
    b=Buffer{};
    return 0;
}
// IOSurfaceAddress on Sweet can begin inside its VM allocation. The allocation
// size includes that prefix. Verify the entire pinned range, rather than import
// unrelated pages beyond a nominal row length.
int surface_description(uint32_t id, struct redwood_drm_surface *out, IOSurfaceRef *keep=nullptr) {
    IOSurfaceRef s=IOSurfaceLookup(id);
    if (!s) return ENOENT;
    uint64_t pixel=(uintptr_t)IOSurfaceGetBaseAddress(s);
    uint64_t base=pixel&~(HostPage-1), bytes=rounded(IOSurfaceGetAllocSize(s));
    size_t width=IOSurfaceGetWidth(s), height=IOSurfaceGetHeight(s), pitch=IOSurfaceGetBytesPerRow(s);
    size_t planes=IOSurfaceGetPlaneCount(s);
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
    if (!e) {
        *out={id,0,bytes,uint32_t(pixel-base),uint32_t(pitch),uint32_t(width),uint32_t(height),
              IOSurfaceGetPixelFormat(s),uint32_t(planes)};
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
    uint64_t va=reserve(bytes);
    if (!va) return ENOSPC;
    Buffer b;
    b.size=bytes;b.flags=flags;
    kern_return_t kr=KERN_SUCCESS;
    if(s) {
        b.cpu=(uintptr_t)IOSurfaceGetBaseAddress(s)&~(HostPage-1);
        b.surface=s;CFRetain(s);
    } else {
        vm_address_t address=0;
        kr=vm_allocate(mach_task_self(),&address,bytes,VM_FLAGS_ANYWHERE);
        b.cpu=address;b.own=true;
    }
    if(kr) {if(b.surface)CFRelease(b.surface);return error(kr);}
    NativeAdrenoAbiBoImport req{b.cpu,bytes};NativeAdrenoAbiBoAllocReply reply{};
    int e=call(NativeAdrenoMethodBoImport,&req,sizeof req,&reply,sizeof reply);
    if(!e && !reply.handle)e=EPROTO;
    if(e) {if(b.own)vm_deallocate(mach_task_self(),b.cpu,b.size);if(b.surface)CFRelease(b.surface);return e;}
    b.kernel=reply.handle;state.buffers[index]=std::move(b);
    Buffer &live=state.buffers[index];
    NativeAdrenoAbiBoMap map{live.kernel,va,3,0,2,0};NativeAdrenoAbiBoMapReply mapped{};
    e=call(NativeAdrenoMethodBoMap,&map,sizeof map,&mapped,sizeof mapped);
    if(!e && mapped.iova!=va)e=EPROTO;
    if(e){release(live);return e;}
    live.iova=va;*handle=index+1;
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
int submit(drm_msm_gem_submit *s) {
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
    uint64_t last=0;
    for(size_t at=0;at<ibs.size() && !e;at+=NativeAdrenoAbiMaxBuffers) {
        req->commands.buffer_count=std::min<size_t>(NativeAdrenoAbiMaxBuffers,ibs.size()-at);
        std::copy_n(ibs.data()+at,req->commands.buffer_count,req->commands.buffers);
        NativeAdrenoAbiSubmitReply reply{};
        e=call(NativeAdrenoMethodSubmitResources,req,sizeof *req,&reply,sizeof reply);
        if(e==EBUSY && !state.fences.empty()) {
            e=wait(state.fences.back().kernel,WaitNS);
            if(!e)e=call(NativeAdrenoMethodSubmitResources,req,sizeof *req,&reply,sizeof reply);
        }
        if(!e && !reply.fence)e=EPROTO;
        if(!e) {
            last=reply.fence;
            for(unsigned i=0;i<s->nr_bos;++i)buffer(bos[i].handle)->last=last;
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
        int e=wait(b->last,p->op&MSM_PREP_NOSYNC?0:remaining(p->timeout));
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

extern "C" int drmOpenWithType(const char *name,const char *,int type) {
    Lock lock;
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
    struct redwood_drm_surface info{};IOSurfaceRef s=nullptr;
    int e=surface_description(surface,&info,&s);
    if(!e){e=allocate(info.size,MSM_BO_WC,s,handle);CFRelease(s);}
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
