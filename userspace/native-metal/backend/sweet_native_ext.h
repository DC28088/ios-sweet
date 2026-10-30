/* SPDX-License-Identifier: MIT */
#ifndef SWEET_NATIVE_METAL_EXT_H
#define SWEET_NATIVE_METAL_EXT_H
#include "air_metal.h"
#ifdef __cplusplus
extern "C" {
#endif
#define SWEET_NATIVE_METAL_ABI 0x53574e01u
unsigned rwa_native_abi(void);
struct rwa_library;
struct rwa_library *rwa_library_open(const void *, size_t, char *, size_t);
void rwa_library_close(struct rwa_library *);
unsigned rwa_library_count(const struct rwa_library *);
const char *rwa_library_name(const struct rwa_library *, unsigned);
int rwa_library_function(const struct rwa_library *, unsigned, const void **, size_t *, unsigned *);
/* Constant bytes are packed little endian. The AIR frontend supports up to
 * 64 bits, including small vectors such as QuartzCore's uchar3 opcodes. */
struct rwa_constant { unsigned index; const char *name; uint64_t bits; unsigned bytes; };
int rwa_set_constants(struct rwa_device *, unsigned stage, const struct rwa_constant *, unsigned, char *, size_t);
/* One context transaction per committed Metal command buffer. end waits once,
 * including the prefix of a command buffer whose later encoding was refused. */
int rwa_barrier(struct rwa_device *, char *, size_t);
int rwa_command_begin(struct rwa_device *, char *, size_t);
int rwa_command_end(struct rwa_device *, char *, size_t);
int rwa_clear(struct rwa_device *, struct rwa_texture *const *, unsigned, unsigned,
              const float [8][4], struct rwa_texture *, const float *, const unsigned *, char *, size_t);
#ifdef __cplusplus
}
#endif
#endif
