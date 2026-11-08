/* SPDX-License-Identifier: MIT */
/* sweet-hfs-grow: grow the mounted iOS HFS+ root into the rest of userdata.
 *
 * The root lives at the start of the userdata partition (no GPT change). The
 * kernel's console log ring (xnu patch 0069) owns the LAST 4 MiB + 4 KiB of that
 * partition, so the volume must stop short of the end: the new size is
 * partition_bytes - reserve (default 16 MiB), rounded down to the volume's
 * allocation block. The kernel (hfs_extendfs) itself checks: HFS+ journaled,
 * root caller, size <= device, multiple of the sector sizes.
 *
 *   sweet-hfs-grow PARTITION_BYTES            dry run: print what would happen
 *   sweet-hfs-grow PARTITION_BYTES --apply    grow (HFSIOC_RESIZE_VOLUME on "/")
 *
 * HFSIOC_RESIZE_VOLUME = _IOW('h', 2, u_int64_t): apple-oss-distributions/hfs
 * core/hfs_fsctl.h; handler core/hfs_readwrite.c, hfs_extendfs core/hfs_resize.c. */
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#ifndef HFSIOC_RESIZE_VOLUME
#define HFSIOC_RESIZE_VOLUME _IOW('h', 2, u_int64_t)
#endif
int fsctl(const char *, unsigned long, void *, unsigned int);

#define RESERVE_BYTES (16ull << 20)   /* >= the 4 MiB + 4 KiB log ring, with margin */

static int show(const char *when, struct statfs *s)
{
	if (statfs("/", s)) {
		printf("statfs /: %s\n", strerror(errno));
		return -1;
	}
	printf("%s: type=%s dev=%s bsize=%u blocks=%llu (%.2f GiB) free=%llu (%.1f MiB)\n", when, s->f_fstypename,
	    s->f_mntfromname, (unsigned)s->f_bsize, (unsigned long long)s->f_blocks,
	    (double)s->f_blocks * s->f_bsize / (1u << 30), (unsigned long long)s->f_bfree,
	    (double)s->f_bfree * s->f_bsize / (1u << 20));
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2 || argc > 3 || (argc == 3 && strcmp(argv[2], "--apply"))) {
		fprintf(stderr, "usage: %s PARTITION_BYTES [--apply]\n", argv[0]);
		return 2;
	}
	char *end = NULL;
	unsigned long long part = strtoull(argv[1], &end, 10);
	if (!*argv[1] || *end || part < (32ull << 30)) {
		fprintf(stderr, "partition size must be a byte count >= 32 GiB\n");
		return 2;
	}
	struct statfs s;
	if (show("before", &s))
		return 1;
	if (strcmp(s.f_fstypename, "hfs")) {
		printf("root is not HFS+; nothing done\n");
		return 1;
	}
	uint64_t bs = s.f_bsize, cur = (uint64_t)s.f_blocks * bs;
	uint64_t want = (part - RESERVE_BYTES) / bs * bs;
	printf("partition %llu bytes, reserve %llu at the end (log ring), new size %llu bytes (%.2f GiB), +%.2f GiB\n",
	    part, RESERVE_BYTES, (unsigned long long)want, (double)want / (1u << 30), (double)(want - cur) / (1u << 30));
	if (want <= cur) {
		printf("volume already at or beyond the target; nothing done\n");
		return 0;
	}
	if (argc != 3) {
		printf("dry run only; add --apply to grow\n");
		return 0;
	}
	u_int64_t size = want;
	if (fsctl("/", HFSIOC_RESIZE_VOLUME, &size, 0)) {
		printf("HFSIOC_RESIZE_VOLUME failed: %s (%d); volume unchanged\n", strerror(errno), errno);
		return 1;
	}
	if (show("after", &s))
		return 1;
	return (uint64_t)s.f_blocks * bs == want ? 0 : 1;
}
