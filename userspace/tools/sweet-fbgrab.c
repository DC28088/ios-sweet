/* sweet-fbgrab: copy the panel the display controller is scanning to a file.
 *
 *   sweet-fbgrab OUT [PHYS]
 *
 * Reads 1080x2400 BGRA (rows 4320 bytes) from /dev/pmem at PHYS (default
 * 0x9c000000, ABL's cont_splash_region = VIG0 SRC0_ADDR, see NOTES 0180).
 * /dev/pmem maps carve-outs as device memory: offsets and lengths must be
 * multiples of 4, so rows are read whole. Needs boot-arg sweetpmem=1. */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define W 1080
#define H 2400
#define STRIDE (W * 4)

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s OUT [PHYS]\n", argv[0]);
		return 2;
	}
	uint64_t phys = argc > 2 ? strtoull(argv[2], NULL, 0) : 0x9c000000ULL;
	if (phys & 3) {
		fprintf(stderr, "PHYS must be 4-byte aligned\n");
		return 2;
	}
	int in = open("/dev/pmem", O_RDONLY);
	if (in < 0) {
		perror("/dev/pmem");
		return 1;
	}
	FILE *out = fopen(argv[1], "wb");
	if (!out) {
		perror(argv[1]);
		return 1;
	}
	static uint8_t row[STRIDE];
	for (int y = 0; y < H; y++) {
		ssize_t n = pread(in, row, STRIDE, (off_t)(phys + (uint64_t)y * STRIDE));
		if (n != STRIDE) {
			fprintf(stderr, "row %d: read %zd\n", y, n);
			return 1;
		}
		if (fwrite(row, 1, STRIDE, out) != STRIDE) {
			perror("write");
			return 1;
		}
	}
	fclose(out);
	close(in);
	fprintf(stderr, "%dx%d BGRA from %#llx -> %s\n", W, H, (unsigned long long)phys, argv[1]);
	return 0;
}
