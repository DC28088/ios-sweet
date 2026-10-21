/* SPDX-License-Identifier: MIT */
#pragma once
#include <IOKit/IOKitLib.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
kern_return_t sweet_gpu_open(io_connect_t *connection, uint64_t info[5]);
kern_return_t sweet_gpu_scalar(io_connect_t, uint32_t, const uint64_t *, uint32_t, uint64_t *, uint32_t *);
kern_return_t sweet_gpu_method(io_connect_t, uint32_t, const uint64_t *, uint32_t, const void *, size_t,
                             uint64_t *, uint32_t *, void *, size_t *);
uint64_t sweet_gpu_timestamp(void);
int sweet_gpu_status(uint64_t *faults, uint64_t *suspends);
#ifdef __cplusplus
}
#endif
