/*
 * sweetmtlprobe: asks Metal for devices the way an app or daemon does and prints what it gets:
 * MTLCreateSystemDefaultDevice(), MTLCopyAllDevices() count, device class/name, supportsFamily: for Apple1-9 / Common1-3 / Metal3.
 * Copy it under another name (e.g. .../iconservicesagent) to test the kernel's per-name GPU list.
 */
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

extern id MTLCreateSystemDefaultDevice(void);
extern id MTLCopyAllDevices(void);

static const char *
cstr(id s)
{
	if (!s) {
		return "(nil)";
	}
	return ((const char *(*)(id, SEL))objc_msgSend)(s, sel_registerName("UTF8String"));
}

int
main(int argc, char **argv)
{
	/* optional argument: run as that uid (and gid), like a daemon that is not root */
	if (argc > 1) {
		int uid = atoi(argv[1]);
		if (setgid(uid) != 0 || setuid(uid) != 0) {
			perror("setuid");
			return 2;
		}
		printf("running as uid %d\n", (int)getuid());
	}
	id all = MTLCopyAllDevices();
	unsigned long n = all ? ((unsigned long (*)(id, SEL))objc_msgSend)(all, sel_registerName("count")) : 0;
	printf("MTLCopyAllDevices: %lu device(s)\n", n);
	id dev = MTLCreateSystemDefaultDevice();
	printf("MTLCreateSystemDefaultDevice: %s\n", dev ? object_getClassName(dev) : "nil");
	if (!dev) {
		return 1;
	}
	printf("name: %s\n", cstr(((id (*)(id, SEL))objc_msgSend)(dev, sel_registerName("name"))));
	static const long fam[] = {1001, 1002, 1003, 1004, 1005, 1006, 1007, 1008, 1009, 3001, 3002, 3003, 5001};
	SEL sup = sel_registerName("supportsFamily:");
	for (unsigned i = 0; i < sizeof(fam) / sizeof(fam[0]); i++) {
		BOOL ok = ((BOOL (*)(id, SEL, long))objc_msgSend)(dev, sup, fam[i]);
		printf("supportsFamily %ld: %d\n", fam[i], ok ? 1 : 0);
	}
	return 0;
}
