#pragma once
#include <stdint.h>

/* Read a u32/u64 (cell-size agnostic) property of /chosen. Returns 0 if absent. */
int fdt_chosen_u64(uintptr_t fdt, const char *prop, uint64_t *out);
/* Value of KEY ("androidboot.serialno=") in /chosen/bootargs, NUL-terminated in OUT; its length, 0 if absent. */
int fdt_bootarg(uintptr_t fdt, const char *key, char *out, unsigned room);
