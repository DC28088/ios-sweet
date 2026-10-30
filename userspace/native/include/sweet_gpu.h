/* SPDX-License-Identifier: MIT */
/*
 * A native client for sweet's Adreno GPU services.
 *
 * This is the bottom of the native Metal driver: no Vulkan, no libdrm, no
 * Turnip. It speaks the kernel's NativeAdreno ABI 1 (NativeAdrenoABI.h) to the
 * IOAcceleratorES user client: buffers, host-page import (an IOSurface's
 * pages), client-chosen IOVAs, PM4 submission, CP fences and timestamps.
 *
 * The kernel appends the CACHE_FLUSH_TS + WHERE_AM_I completion fence to every
 * submission, so callers submit plain PM4 and get a fence value back. All entry
 * points are thread-safe only in the sense that the underlying IOKit connection
 * is; a driver should serialise submissions (the GPU has one ring).
 */
#ifndef SWEET_GPU_H
#define SWEET_GPU_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SweetGpu SweetGpu;

/* Facts the kernel publishes from the board code (NativeAdrenoAbiDeviceInfo). */
struct SweetGpuInfo {
    uint32_t chip_id;
    uint32_t gmem_bytes;
    uint64_t gmem_base;
    uint64_t uche_trap_base;
    uint64_t va_start;
    uint64_t va_size;
    uint32_t highest_bank_bit;
    uint32_t ubwc_swizzle;
    uint32_t macrotile_mode;
    uint32_t ubwc_version;
    uint32_t flags;
};

struct SweetGpuParams {
    uint32_t abi_version;
    uint32_t input_bits;
    uint32_t output_bits;
    uint32_t flags;
    uint64_t max_bo_bytes;
};

/* One PM4 command buffer to submit (the kernel's NativeAdrenoAbiSubmitBuffer). */
struct SweetGpuSubmitBuffer {
    uint64_t handle;
    uint64_t iova;
    uint64_t bytes;
};

enum {
    SweetGpuSuccess = 0,
    SweetGpuError = -1,          /* see `why` */
    SweetGpuBusy = -2,           /* the kernel had no free job slot */
};

/*
 * Opens IOAcceleratorES with type 0x4147 and finds an admitted external-method
 * selector (the AGX accelerator tunnel, 'NTNL'). Only a process named
 * backboardd is admitted by the current kernel; anything else fails with
 * SweetGpuError and a sentence in `why`.
 *
 * Returns a device or NULL. `why` may be NULL if `why_size` is 0.
 */
SweetGpu *sweet_gpu_open(char *why, size_t why_size);
void sweet_gpu_close(SweetGpu *gpu);

int sweet_gpu_params(SweetGpu *gpu, struct SweetGpuParams *out, char *why, size_t why_size);
int sweet_gpu_info(SweetGpu *gpu, struct SweetGpuInfo *out, char *why, size_t why_size);

/* Allocates a GPU buffer. Returns 0 on failure (handles are never zero). */
uint64_t sweet_gpu_bo_alloc(SweetGpu *gpu, uint64_t bytes, uint32_t align_log2,
                            char *why, size_t why_size);
/* Maps an allocated buffer at `iova` (4 KiB aligned, inside the published window). */
int sweet_gpu_bo_map(SweetGpu *gpu, uint64_t handle, uint64_t iova,
                     uint32_t permissions, char *why, size_t why_size);
/*
 * Pins [address, address+bytes) of the calling task as a buffer (the caller's
 * own pages, typically an IOSurface's base) and maps it at `iova`. The caller
 * keeps its own CPU mapping; there is no CPU-map token for an import.
 */
uint64_t sweet_gpu_bo_import(SweetGpu *gpu, void *address, uint64_t bytes,
                             uint64_t iova, char *why, size_t why_size);
int sweet_gpu_bo_free(SweetGpu *gpu, uint64_t handle, char *why, size_t why_size);

/* CPU view of an allocated buffer (write-combined). NULL on failure. */
void *sweet_gpu_bo_cpu_map(SweetGpu *gpu, uint64_t handle, uint64_t *bytes,
                           char *why, size_t why_size);
int sweet_gpu_bo_cpu_unmap(SweetGpu *gpu, uint64_t handle, void *mapping,
                           char *why, size_t why_size);

/* Submits 1..16 PM4 command buffers; *fence receives the completion fence.
 * wait_fence is zero or an earlier fence of this client. */
int sweet_gpu_submit(SweetGpu *gpu, const struct SweetGpuSubmitBuffer *buffers,
                     unsigned count, uint64_t wait_fence, uint64_t wait_timeout_ns,
                     uint64_t *fence, char *why, size_t why_size);
/* Waits for a fence of this client. timeout_ns 0 polls once. */
int sweet_gpu_wait(SweetGpu *gpu, uint64_t fence, uint64_t timeout_ns,
                   char *why, size_t why_size);
/* GPU always-on counter, in ticks (not nanoseconds). 0 on failure. */
uint64_t sweet_gpu_timestamp(SweetGpu *gpu);

#ifdef __cplusplus
}
#endif

#endif /* SWEET_GPU_H */
