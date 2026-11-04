// sweet-pmemrw: 32-bit physical reads/writes through /dev/pmem (root, boot-arg sweetpmem=1).
//   sweet-pmemrw r PA [COUNT]      print COUNT words from PA
//   sweet-pmemrw w PA VALUE        write one word, then read it back
// Reading an unclocked block hangs the SoC: use only on known-powered registers.
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	if (argc < 3 || (argv[1][0] != 'r' && argv[1][0] != 'w') || (argv[1][0] == 'w' && argc < 4)) {
		fprintf(stderr, "usage: %s r PA [COUNT] | w PA VALUE\n", argv[0]);
		return 2;
	}
	const unsigned long long pa = strtoull(argv[2], NULL, 0);
	if (pa & 3) {
		fprintf(stderr, "PA must be 4-byte aligned\n");
		return 2;
	}
	int fd = open("/dev/pmem", O_RDWR);
	if (fd < 0) {
		perror("/dev/pmem");
		return 1;
	}
	if (argv[1][0] == 'w') {
		uint32_t v = (uint32_t)strtoul(argv[3], NULL, 0);
		if (pwrite(fd, &v, 4, (off_t)pa) != 4) {
			perror("pwrite");
			return 1;
		}
	}
	unsigned count = argv[1][0] == 'r' && argc > 3 ? (unsigned)strtoul(argv[3], NULL, 0) : 1;
	if (count > 1024)
		count = 1024;
	for (unsigned i = 0; i < count; i++) {
		uint32_t v = 0;
		if (pread(fd, &v, 4, (off_t)(pa + 4ull * i)) != 4) {
			perror("pread");
			return 1;
		}
		printf("0x%llx 0x%08x\n", pa + 4ull * i, v);
	}
	close(fd);
	return 0;
}
