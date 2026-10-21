// SPDX-License-Identifier: MIT
// Sailfish sweetgl scalar transport -> sweet NativeAdreno ABI1.
// Kernel handles, userspace GEM handles, CPU addresses and GPU VAs are distinct.
#include "sweet_native.h"
#include "NativeAdrenoABI.h"
#include <IOSurface/IOSurface.h>
#include <libkern/OSCacheControl.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <pthread.h>
#include <algorithm>
#include <cstring>
#include <limits>

extern "C" void sweet_drm_log(const char *, ...);
namespace {
pthread_mutex_t gate=PTHREAD_MUTEX_INITIALIZER;
struct Guard { Guard(){pthread_mutex_lock(&gate);} ~Guard(){pthread_mutex_unlock(&gate);} };
constexpr size_t MaxBos=1024;
struct Bo {
    uint64_t handle=0, va=0, bytes=0, cpu=0, mapped_bytes=0, fence=0;
    uint32_t token=0;
    IOSurfaceRef surface=nullptr;
    bool imported=false, quarantined=false;
};
Bo bos[MaxBos];
io_connect_t connection=0;
NativeAdrenoAbiDeviceInfo device{};
NativeAdrenoAbiParams params{};
uint64_t last_fence=0;
bool lost=false;

kern_return_t call(uint32_t method,const void *in,size_t bytes,void *out,size_t output) {
    if (!connection || lost) return kIOReturnNotReady;
    size_t actual=output;
    kern_return_t kr=IOConnectCallStructMethod(connection,method,in,bytes,out,output?&actual:nullptr);
    if (kr==KERN_SUCCESS && actual!=output) return kIOReturnBadArgument;
    return kr;
}
Bo *bo(uint64_t h) { return h && h<=MaxBos && bos[h-1].handle ? &bos[h-1]:nullptr; }
uint64_t align_up(uint64_t n,uint64_t alignment) {
    return n<=UINT64_MAX-(alignment-1)?(n+alignment-1)&~(alignment-1):0;
}
uint64_t reserve_va(uint64_t bytes) {
    const uint64_t limit=device.va_start+device.va_size;
    if (limit<device.va_start) return 0;
    uint64_t at=align_up(device.va_start,65536);
    // First fit, with one 16KiB unmapped guard after each allocation.
    for (size_t iteration=0;iteration<=MaxBos;++iteration) {
        if (!at || at>=limit || bytes>limit-at || limit-at-bytes<16384) return 0;
        uint64_t next=at;
        for (const Bo &b:bos) {
            if (!b.handle || !b.va) continue;
            if (at<b.va+b.bytes+16384 && b.va<at+bytes+16384)
                next=std::max(next,align_up(b.va+b.bytes+16384,65536));
        }
        if (next==at) return at;
        at=next;
    }
    return 0;
}
kern_return_t wait(uint64_t fence,uint64_t timeout_ns) {
    if (!fence) return KERN_SUCCESS;
    NativeAdrenoAbiWaitFence request{fence,timeout_ns};
    return call(NativeAdrenoMethodWaitFence,&request,sizeof request,nullptr,0);
}
void invalidate_surfaces(uint64_t completed) {
    for (Bo &b:bos)
        if (b.handle && b.imported && b.cpu && b.fence && b.fence<=completed)
            sys_dcache_invalidate(reinterpret_cast<void *>(b.cpu),b.bytes);
}
kern_return_t destroy(Bo &b) {
    kern_return_t kr=wait(b.fence,5000000000ull);
    if (kr) { b.quarantined=true; lost=true; return kr; }
    if (b.imported && b.cpu && b.fence) sys_dcache_invalidate(reinterpret_cast<void *>(b.cpu),b.bytes);
    if (b.token) {
        kr=IOConnectUnmapMemory64(connection,b.token,mach_task_self(),b.cpu);
        if (kr) { b.quarantined=true; lost=true; return kr; }
        NativeAdrenoAbiBoCpuUnmap request{b.handle,b.token,0};
        kr=call(NativeAdrenoMethodBoCpuUnmap,&request,sizeof request,nullptr,0);
        if (kr) { b.quarantined=true; lost=true; return kr; }
        b.token=0;
    }
    NativeAdrenoAbiBoFree request{b.handle};
    kr=call(NativeAdrenoMethodBoFree,&request,sizeof request,nullptr,0);
    if (kr) { b.quarantined=true; lost=true; return kr; }
    if (b.surface) CFRelease(b.surface);
    b={};return KERN_SUCCESS;
}
kern_return_t make_bo(uint64_t bytes,IOSurfaceRef surface,uint64_t *out) {
    if (!out || bytes>params.max_bo_bytes || !bytes) return kIOReturnBadArgument;
    bytes=align_up(bytes,16384);
    if (!bytes || bytes>params.max_bo_bytes) return kIOReturnBadArgument;
    size_t index=0;
    while (index<MaxBos && bos[index].handle) ++index;
    if (index==MaxBos) return kIOReturnNoResources;
    const uint64_t va=reserve_va(bytes);
    if (!va) return kIOReturnNoMemory;
    Bo &b=bos[index];NativeAdrenoAbiBoAllocReply reply{};
    kern_return_t kr;
    if (surface) {
        const auto pixel_base=reinterpret_cast<uint64_t>(IOSurfaceGetBaseAddress(surface));
        const uint64_t base=pixel_base & ~uint64_t(16383);
        const size_t size=IOSurfaceGetAllocSize(surface);
        // The kernel reports the complete page-backed allocation, including
        // IOSurfaceAddress's first-page offset. Verify that whole extent is
        // mapped in this task before asking NativeAdreno to pin it.
        if (!base || !size || (size&16383) || size!=bytes) return kIOReturnBadArgument;
        mach_vm_address_t region=base;mach_vm_size_t region_bytes=0;
        vm_region_basic_info_data_64_t region_info{};
        mach_msg_type_number_t info_count=VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object=MACH_PORT_NULL;
        auto region_kr=mach_vm_region(mach_task_self(),&region,&region_bytes,VM_REGION_BASIC_INFO_64,
                         reinterpret_cast<vm_region_info_t>(&region_info),&info_count,&object);
        if (object) mach_port_deallocate(mach_task_self(),object);
        if (region_kr || region>base || region_bytes<base-region || bytes>region_bytes-(base-region) ||
            !(region_info.protection&VM_PROT_READ)) return kIOReturnBadArgument;
        NativeAdrenoAbiBoImport request{base,bytes};
        kr=call(NativeAdrenoMethodBoImport,&request,sizeof request,&reply,sizeof reply);
        if (kr) return kr;
        b.handle=reply.handle;b.bytes=bytes;b.cpu=base;b.imported=true;
        b.surface=surface;CFRetain(surface);
    } else {
        NativeAdrenoAbiBoAlloc request{bytes,14,0};
        kr=call(NativeAdrenoMethodBoAlloc,&request,sizeof request,&reply,sizeof reply);
        if (kr) return kr;
        b.handle=reply.handle;b.bytes=bytes;
        NativeAdrenoAbiBoCpuMap map{b.handle};NativeAdrenoAbiBoCpuMapReply mapping{};
        kr=call(NativeAdrenoMethodBoCpuMap,&map,sizeof map,&mapping,sizeof mapping);
        if (!kr) {
            b.token=mapping.token;b.mapped_bytes=mapping.bytes;
            mach_vm_address_t address=0;mach_vm_size_t size=mapping.bytes;
            kr=IOConnectMapMemory64(connection,mapping.token,mach_task_self(),&address,&size,kIOMapAnywhere);
            if (!kr && size>=bytes) {b.cpu=address;b.mapped_bytes=size;}
            else {
                // Release the lease even if mapClientMemory failed. Never
                // unmap address0 as though a CPU mapping had succeeded.
                if (!kr) {
                    auto unmap_kr=IOConnectUnmapMemory64(connection,mapping.token,mach_task_self(),address);
                    if (unmap_kr) {b.quarantined=true;lost=true;return unmap_kr;}
                }
                NativeAdrenoAbiBoCpuUnmap unmap{b.handle,b.token,0};
                kern_return_t drop=call(NativeAdrenoMethodBoCpuUnmap,&unmap,sizeof unmap,nullptr,0);
                if (drop) {b.quarantined=true;lost=true;return drop;}
                b.token=0;
                if (!kr) kr=kIOReturnNoMemory;
            }
        }
        if (kr) {kern_return_t cleanup=destroy(b);return cleanup?cleanup:kr;}
    }
    NativeAdrenoAbiBoMap map{b.handle,va,3,0,2,0};NativeAdrenoAbiBoMapReply mapped{};
    kr=call(NativeAdrenoMethodBoMap,&map,sizeof map,&mapped,sizeof mapped);
    if (!kr && mapped.iova!=va) kr=kIOReturnBadArgument;
    if (kr) {kern_return_t cleanup=destroy(b);return cleanup?cleanup:kr;}
    b.va=va;out[0]=index+1;out[1]=va;out[2]=b.cpu;
    return KERN_SUCCESS;
}
}

extern "C" kern_return_t sweet_gpu_open(io_connect_t *out,uint64_t info[5]) {
    Guard guard;
    if (!out || !info || lost) return kIOReturnNotReady;
    if (!connection) {
        io_service_t service=IOServiceGetMatchingService(kIOMainPortDefault,IOServiceMatching("AGXAccelerator"));
        if (!service) {sweet_drm_log("sweetgl: A618 renderer service not published");return kIOReturnNotReady;}
        auto kr=IOServiceOpen(service,mach_task_self(),0x4147,&connection);
        IOObjectRelease(service);
        if (kr) {connection=0;return kr;}
        kr=call(NativeAdrenoMethodQuery,nullptr,0,&params,sizeof params);
        if (!kr) kr=call(NativeAdrenoMethodGetDeviceInfo,nullptr,0,&device,sizeof device);
        unsigned gpu=((device.chip_id>>24)&255)*100+((device.chip_id>>16)&255)*10+((device.chip_id>>8)&255);
        if (!kr && (params.abi_version!=NativeAdrenoAbiVersion || gpu!=618 ||
                    !device.gmem_bytes || !device.va_size || !(params.flags&NativeAdrenoParamsHostImport)))
            kr=kIOReturnUnsupported;
        if (kr) {IOServiceClose(connection);connection=0;return kr;}
        sweet_drm_log("sweetgl: NativeAdreno ABI%u A%u chip=%#x GMEM=%u VA=%#llx+%#llx",
                      params.abi_version,gpu,device.chip_id,device.gmem_bytes,device.va_start,device.va_size);
    }
    *out=connection;info[0]=device.chip_id;info[1]=device.gmem_bytes;info[2]=device.gmem_base;
    info[3]=0;info[4]=NativeAdrenoAbiVersion;
    return KERN_SUCCESS;
}

extern "C" kern_return_t sweet_gpu_scalar(io_connect_t c,uint32_t selector,const uint64_t *in,
    uint32_t count,uint64_t *out,uint32_t *capacity) {
    Guard guard;
    if (!c || c!=connection || lost) return kIOReturnNotReady;
    switch(selector) {
    case 10: { // Pixel BO_NEW
        if (count!=1 || !in || !out || !capacity || *capacity!=3) return kIOReturnBadArgument;
        return make_bo(in[0],nullptr,out);
    }
    case 11: { // Pixel BO_CLOSE
        if (count!=1 || !in) return kIOReturnBadArgument;
        Bo *b=bo(in[0]);return b?destroy(*b):kIOReturnBadArgument;
    }
    case 12: { // Pixel IMPORT_IOSURFACE
        if (count!=1 || !in || !out || !capacity || *capacity!=3) return kIOReturnBadArgument;
        IOSurfaceRef surface=IOSurfaceLookup(static_cast<uint32_t>(in[0]));
        if (!surface) return kIOReturnNotFound;
        uint64_t mapped[3]{};
        const size_t size=IOSurfaceGetAllocSize(surface);
        auto kr=make_bo(size,surface,mapped);CFRelease(surface);
        if (!kr) {out[0]=mapped[0];out[1]=mapped[1];out[2]=size;}
        return kr;
    }
    case 14: { // Pixel WAIT_FENCE, milliseconds -> Native nanoseconds
        if (count!=2 || !in || in[1]>UINT64_MAX/1000000) return kIOReturnBadArgument;
        auto kr=wait(in[0],in[1]*1000000);
        if (!kr) invalidate_surfaces(in[0]);
        return kr;
    }
    case 16: { // Pixel CPU_PREP invalidate after real completion
        if (count!=1 || !in) return kIOReturnBadArgument;
        Bo *b=bo(in[0]);if (!b) return kIOReturnBadArgument;
        auto kr=wait(b->fence,0);
        if (!kr && b->cpu) sys_dcache_invalidate(reinterpret_cast<void *>(b->cpu),b->bytes);
        return kr;
    }
    default:return kIOReturnUnsupported;
    }
}

extern "C" kern_return_t sweet_gpu_method(io_connect_t c,uint32_t selector,const uint64_t *,uint32_t scalars,
    const void *data,size_t bytes,uint64_t *out,uint32_t *capacity,void *,size_t *) {
    Guard guard;
    if (!c || c!=connection || lost) return kIOReturnNotReady;
    if (selector!=13 || scalars || !data || bytes<8 || !out || !capacity || *capacity!=1)
        return kIOReturnBadArgument;
    // Sailfish format: {u32 IB count, u32 BO count}, IB{u64 VA,u32 dwords,u32 reserved}, BO handles.
    const auto *buf=static_cast<const unsigned char *>(data);
    uint32_t counts[2];memcpy(counts,buf,8);
    const size_t n=counts[0],nb=counts[1];
    if (!n || n>4096 || !nb || nb>MaxBos || bytes!=8+n*16+nb*4) return kIOReturnBadArgument;
    auto kr=wait(last_fence,5000000000ull);
    if (kr) {lost=true;return kr;}
    Bo *resources[MaxBos]{};
    for (size_t i=0;i<nb;++i) {
        uint32_t handle;memcpy(&handle,buf+8+n*16+i*4,4);
        resources[i]=bo(handle);
        if (!resources[i] || resources[i]->quarantined) return kIOReturnBadArgument;
    }
    // Validate every IB before submitting any; all must lie in a listed BO.
    NativeAdrenoAbiSubmitBuffer commands[4096]{};
    for (size_t i=0;i<n;++i) {
        uint64_t va;uint32_t dw,reserved;
        memcpy(&va,buf+8+i*16,8);memcpy(&dw,buf+16+i*16,4);memcpy(&reserved,buf+20+i*16,4);
        if (!dw || reserved || (va&3)) return kIOReturnBadArgument;
        const uint64_t size=uint64_t(dw)*4;
        for (size_t j=0;j<nb;++j) {
            Bo *b=resources[j];
            if (va>=b->va && va-b->va<b->bytes && size<=b->bytes-(va-b->va))
                {commands[i]={b->handle,va,size};break;}
        }
        if (!commands[i].handle) return kIOReturnBadArgument;
    }
    for (size_t i=0;i<nb;++i)
        if (resources[i]->cpu) sys_dcache_flush(reinterpret_cast<void *>(resources[i]->cpu),resources[i]->bytes);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
    uint64_t fence=0;
    // Preserve order across Native ABI's 16-IB batches. The kernel quiesces
    // its shared context before each submit; never truncate a Mesa command list.
    for (size_t at=0;at<n;at+=NativeAdrenoAbiMaxBuffers) {
        NativeAdrenoAbiSubmit request{};
        request.buffer_count=static_cast<uint32_t>(std::min<size_t>(NativeAdrenoAbiMaxBuffers,n-at));
        for (size_t j=0;j<request.buffer_count;++j) request.buffers[j]=commands[at+j];
        NativeAdrenoAbiSubmitReply reply{};
        kr=call(NativeAdrenoMethodSubmit,&request,sizeof request,&reply,sizeof reply);
        if (kr || !reply.fence) {lost=true;return kr?kr:kIOReturnIOError;}
        fence=reply.fence;
        for (size_t j=0;j<nb;++j) resources[j]->fence=fence;
        last_fence=fence;
    }
    *out=fence;return KERN_SUCCESS;
}

extern "C" uint64_t sweet_gpu_timestamp(void) {
    Guard guard;NativeAdrenoAbiTimestamp reply{};
    return call(NativeAdrenoMethodGetTimestamp,nullptr,0,&reply,sizeof reply)==KERN_SUCCESS?reply.ticks:0;
}

extern "C" int sweet_gpu_status(uint64_t *faults,uint64_t *suspends) {
    Guard guard;NativeAdrenoAbiStatus reply{};
    if (!faults || !suspends || call(NativeAdrenoMethodGetStatus,nullptr,0,&reply,sizeof reply)) return -1;
    *faults=reply.faults;*suspends=reply.suspends;return 0;
}
