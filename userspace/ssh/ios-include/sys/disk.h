/* ios-sweet: the iOS SDK has no <sys/disk.h>; toybox's lib/portability.c only needs these two ioctls
 * (the macOS SDK's values) for a block device's size. */
#ifndef _SWEET_SYS_DISK_H_
#define _SWEET_SYS_DISK_H_
#include <stdint.h>
#include <sys/ioccom.h>
#define DKIOCGETBLOCKSIZE  _IOR('d', 24, uint32_t)
#define DKIOCGETBLOCKCOUNT _IOR('d', 25, uint64_t)
#endif
