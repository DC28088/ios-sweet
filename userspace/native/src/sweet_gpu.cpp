/* SPDX-License-Identifier: MIT */
/*
 * NativeAdreno ABI 1 over IOKit, without Vulkan, libdrm or Turnip.
 *
 * The user client is reached through the AGX accelerator service
 * (IOAcceleratorES, open type 0x4147) and, because that client admits a
 * sandboxed caller, through the 'NTNL' tunnel: one admitted external-method
 * selector carries {magic, method, payload}. This is the same transport
 * Turnip's Darwin KMD uses; the methods and records are NativeAdrenoABI.h.
 */
#include "sweet_gpu.h"

#include <IOKit/IOKitLib.h>
#include <mach/mach.h>

#include <map>
#include <new>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "NativeAdrenoABI.h"

namespace {

constexpr uint32_t kTunnelMagic = 0x4e544e4cu;   /* 'NTNL' */
constexpr uint32_t kOpenType = 0x4147;           /* the AGX accelerator's */
constexpr const char *kService = "IOAcceleratorES";

struct Mapping {
    uint32_t token;
    void *address;
};

struct SweetGpuImpl {
    io_connect_t conn = 0;
    uint32_t selector = 0;
    std::map<uint64_t, Mapping> mappings;
};

void whyf(char *why, size_t size, const char *fmt, ...)
{
    if (!why || !size)
        return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(why, size, fmt, ap);
    va_end(ap);
    why[size - 1] = '\0';
}

kern_return_t call(SweetGpuImpl *gpu, uint32_t method, const void *in, size_t in_size,
                   void *out, size_t out_size)
{
    uint8_t buffer[8 + sizeof(NativeAdrenoAbiSubmit)];
    if (in_size > sizeof(NativeAdrenoAbiSubmit))
        return kIOReturnBadArgument;
    const uint32_t header[2] = {kTunnelMagic, method};
    memcpy(buffer, header, sizeof(header));
    if (in_size)
        memcpy(buffer + sizeof(header), in, in_size);
    size_t out_bytes = out_size;
    return IOConnectCallStructMethod(gpu->conn, gpu->selector, buffer,
                                     sizeof(header) + in_size,
                                     out_size ? out : nullptr,
                                     out_size ? &out_bytes : nullptr);
}

}  // namespace

extern "C" {

SweetGpu *sweet_gpu_open(char *why, size_t why_size)
{
    io_service_t service = IOServiceGetMatchingService(kIOMainPortDefault,
                                                       IOServiceMatching(kService));
    if (!service) {
        whyf(why, why_size, "no %s service", kService);
        return nullptr;
    }
    io_connect_t conn = 0;
    kern_return_t kr = IOServiceOpen(service, mach_task_self(), kOpenType, &conn);
    IOObjectRelease(service);
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "IOServiceOpen(%s, 0x%x) failed: 0x%x", kService, kOpenType, kr);
        return nullptr;
    }

    SweetGpuImpl *gpu = new (std::nothrow) SweetGpuImpl;
    if (!gpu) {
        IOServiceClose(conn);
        whyf(why, why_size, "out of memory");
        return nullptr;
    }
    gpu->conn = conn;

    /* Ask each selector whether the sandbox admits it: the driver answers a
     * probe (method 0xffffffff) with success for the ones it lets through. */
    for (uint32_t s = 0; s < 256; s++) {
        const uint32_t probe[2] = {kTunnelMagic, 0xffffffffu};
        if (IOConnectCallStructMethod(conn, s, probe, sizeof(probe), nullptr, nullptr) ==
            KERN_SUCCESS) {
            gpu->selector = s;
            break;
        }
    }
    if (!gpu->selector) {
        IOServiceClose(conn);
        delete gpu;
        whyf(why, why_size, "no admitted external-method selector");
        return nullptr;
    }

    NativeAdrenoAbiParams params{};
    kr = call(gpu, NativeAdrenoMethodQuery, nullptr, 0, &params, sizeof(params));
    if (kr != KERN_SUCCESS || params.abi_version != NativeAdrenoAbiVersion) {
        sweet_gpu_close(reinterpret_cast<SweetGpu *>(gpu));
        whyf(why, why_size, "ABI query failed: 0x%x version %u", kr, params.abi_version);
        return nullptr;
    }
    return reinterpret_cast<SweetGpu *>(gpu);
}

void sweet_gpu_close(SweetGpu *handle)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu)
        return;
    IOServiceClose(gpu->conn);
    delete gpu;
}

int sweet_gpu_params(SweetGpu *handle, struct SweetGpuParams *out, char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !out)
        return SweetGpuError;
    NativeAdrenoAbiParams params{};
    kern_return_t kr = call(gpu, NativeAdrenoMethodQuery, nullptr, 0, &params, sizeof(params));
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "query failed: 0x%x", kr);
        return SweetGpuError;
    }
    out->abi_version = params.abi_version;
    out->input_bits = params.input_bits;
    out->output_bits = params.output_bits;
    out->flags = params.flags;
    out->max_bo_bytes = params.max_bo_bytes;
    return SweetGpuSuccess;
}

int sweet_gpu_info(SweetGpu *handle, struct SweetGpuInfo *out, char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !out)
        return SweetGpuError;
    NativeAdrenoAbiDeviceInfo info{};
    kern_return_t kr = call(gpu, NativeAdrenoMethodGetDeviceInfo, nullptr, 0, &info, sizeof(info));
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "device info failed: 0x%x", kr);
        return SweetGpuError;
    }
    out->chip_id = info.chip_id;
    out->gmem_bytes = info.gmem_bytes;
    out->gmem_base = info.gmem_base;
    out->uche_trap_base = info.uche_trap_base;
    out->va_start = info.va_start;
    out->va_size = info.va_size;
    out->highest_bank_bit = info.highest_bank_bit;
    out->ubwc_swizzle = info.ubwc_swizzle;
    out->macrotile_mode = info.macrotile_mode;
    out->ubwc_version = info.ubwc_version;
    out->flags = info.flags;
    return SweetGpuSuccess;
}

uint64_t sweet_gpu_bo_alloc(SweetGpu *handle, uint64_t bytes, uint32_t align_log2,
                            char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !bytes)
        return 0;
    NativeAdrenoAbiBoAlloc request{};
    request.bytes = bytes;
    request.align_log2 = align_log2;
    request.contiguous = 0;
    NativeAdrenoAbiBoAllocReply reply{};
    kern_return_t kr = call(gpu, NativeAdrenoMethodBoAlloc, &request, sizeof(request),
                            &reply, sizeof(reply));
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "BoAlloc(%llu) failed: 0x%x", (unsigned long long)bytes, kr);
        return 0;
    }
    return reply.handle;
}

int sweet_gpu_bo_map(SweetGpu *handle, uint64_t bo, uint64_t iova, uint32_t permissions,
                     char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !bo)
        return SweetGpuError;
    NativeAdrenoAbiBoMap request{};
    request.handle = bo;
    request.iova = iova;
    request.permissions = permissions;
    request.writeback = 0;
    request.shareability = 2;
    NativeAdrenoAbiBoMapReply reply{};
    kern_return_t kr = call(gpu, NativeAdrenoMethodBoMap, &request, sizeof(request),
                            &reply, sizeof(reply));
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "BoMap(0x%llx) failed: 0x%x", (unsigned long long)iova, kr);
        return SweetGpuError;
    }
    return SweetGpuSuccess;
}

uint64_t sweet_gpu_bo_import(SweetGpu *handle, void *address, uint64_t bytes, uint64_t iova,
                             char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !address || !bytes)
        return 0;
    NativeAdrenoAbiBoImport request{};
    request.address = (uint64_t)(uintptr_t)address;
    request.bytes = bytes;
    NativeAdrenoAbiBoAllocReply reply{};
    kern_return_t kr = call(gpu, NativeAdrenoMethodBoImport, &request, sizeof(request),
                            &reply, sizeof(reply));
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "BoImport(%llu) failed: 0x%x", (unsigned long long)bytes, kr);
        return 0;
    }
    if (sweet_gpu_bo_map(handle, reply.handle, iova, 3, why, why_size) != SweetGpuSuccess) {
        NativeAdrenoAbiBoFree undo{reply.handle};
        call(gpu, NativeAdrenoMethodBoFree, &undo, sizeof(undo), nullptr, 0);
        return 0;
    }
    return reply.handle;
}

int sweet_gpu_bo_free(SweetGpu *handle, uint64_t bo, char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !bo)
        return SweetGpuError;
    NativeAdrenoAbiBoFree request{bo};
    kern_return_t kr = call(gpu, NativeAdrenoMethodBoFree, &request, sizeof(request), nullptr, 0);
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "BoFree(0x%llx) failed: 0x%x", (unsigned long long)bo, kr);
        return SweetGpuError;
    }
    return SweetGpuSuccess;
}

void *sweet_gpu_bo_cpu_map(SweetGpu *handle, uint64_t bo, uint64_t *bytes,
                           char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !bo)
        return nullptr;
    NativeAdrenoAbiBoCpuMap request{bo};
    NativeAdrenoAbiBoCpuMapReply reply{};
    kern_return_t kr = call(gpu, NativeAdrenoMethodBoCpuMap, &request, sizeof(request),
                            &reply, sizeof(reply));
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "BoCpuMap(0x%llx) failed: 0x%x", (unsigned long long)bo, kr);
        return nullptr;
    }
    mach_vm_address_t address = 0;
    mach_vm_size_t length = 0;
    kr = IOConnectMapMemory64(gpu->conn, reply.token, mach_task_self(), &address, &length,
                              kIOMapAnywhere | kIOMapWriteCombineCache);
    if (kr != KERN_SUCCESS || !address || length < reply.bytes) {
        NativeAdrenoAbiBoCpuUnmap undo{bo, reply.token};
        call(gpu, NativeAdrenoMethodBoCpuUnmap, &undo, sizeof(undo), nullptr, 0);
        whyf(why, why_size, "IOConnectMapMemory64(0x%llx) failed: 0x%x",
             (unsigned long long)bo, kr);
        return nullptr;
    }
    Mapping mapping{reply.token, (void *)(uintptr_t)address};
    gpu->mappings[bo] = mapping;
    if (bytes)
        *bytes = length;
    return mapping.address;
}

int sweet_gpu_bo_cpu_unmap(SweetGpu *handle, uint64_t bo, void *mapping,
                           char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !bo || !mapping)
        return SweetGpuError;
    auto it = gpu->mappings.find(bo);
    if (it == gpu->mappings.end() || it->second.address != mapping) {
        whyf(why, why_size, "unmap of a buffer that is not mapped: 0x%llx",
             (unsigned long long)bo);
        return SweetGpuError;
    }
    kern_return_t kr = IOConnectUnmapMemory64(gpu->conn, it->second.token, mach_task_self(),
                                              (mach_vm_address_t)(uintptr_t)mapping);
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "IOConnectUnmapMemory64(0x%llx) failed: 0x%x",
             (unsigned long long)bo, kr);
        return SweetGpuError;
    }
    NativeAdrenoAbiBoCpuUnmap undo{bo, it->second.token};
    call(gpu, NativeAdrenoMethodBoCpuUnmap, &undo, sizeof(undo), nullptr, 0);
    gpu->mappings.erase(it);
    return SweetGpuSuccess;
}

int sweet_gpu_submit(SweetGpu *handle, const struct SweetGpuSubmitBuffer *buffers,
                     unsigned count, uint64_t wait_fence, uint64_t wait_timeout_ns,
                     uint64_t *fence, char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !buffers || !count || count > NativeAdrenoAbiMaxBuffers || !fence)
        return SweetGpuError;
    NativeAdrenoAbiSubmit request{};
    request.buffer_count = count;
    request.queue = 0;
    request.wait_fence = wait_fence;
    request.wait_timeout_ns = wait_timeout_ns;
    for (unsigned i = 0; i < count; i++) {
        request.buffers[i].handle = buffers[i].handle;
        request.buffers[i].iova = buffers[i].iova;
        request.buffers[i].bytes = buffers[i].bytes;
    }
    NativeAdrenoAbiSubmitReply reply{};
    kern_return_t kr = call(gpu, NativeAdrenoMethodSubmit, &request, sizeof(request),
                            &reply, sizeof(reply));
    if (kr == kIOReturnBusy)
        return SweetGpuBusy;
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "submit failed: 0x%x", kr);
        return SweetGpuError;
    }
    *fence = reply.fence;
    return SweetGpuSuccess;
}

int sweet_gpu_wait(SweetGpu *handle, uint64_t fence, uint64_t timeout_ns,
                   char *why, size_t why_size)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu || !fence)
        return SweetGpuError;
    NativeAdrenoAbiWaitFence request{fence, timeout_ns};
    kern_return_t kr = call(gpu, NativeAdrenoMethodWaitFence, &request, sizeof(request),
                            nullptr, 0);
    if (kr == kIOReturnTimeout) {
        whyf(why, why_size, "fence 0x%llx timed out", (unsigned long long)fence);
        return SweetGpuBusy;
    }
    if (kr != KERN_SUCCESS) {
        whyf(why, why_size, "wait fence 0x%llx failed: 0x%x", (unsigned long long)fence, kr);
        return SweetGpuError;
    }
    return SweetGpuSuccess;
}

uint64_t sweet_gpu_timestamp(SweetGpu *handle)
{
    SweetGpuImpl *gpu = reinterpret_cast<SweetGpuImpl *>(handle);
    if (!gpu)
        return 0;
    NativeAdrenoAbiTimestamp reply{};
    if (call(gpu, NativeAdrenoMethodGetTimestamp, nullptr, 0, &reply, sizeof(reply)) !=
        KERN_SUCCESS)
        return 0;
    return reply.ticks;
}

}  // extern "C"
