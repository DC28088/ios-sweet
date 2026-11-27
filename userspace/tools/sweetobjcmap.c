/*
 * sweetobjcmap IMAGE...: loads each Mach-O image (a framework binary path as dyld knows it) and prints every Objective-C
 * method's implementation as "image<TAB>offset<TAB>[+-][Class selector]", offset relative to the image's load address.
 * Used on the phone to name spindump frames ("UIKitCore + 1018976"): the dyld cache there has no local symbols.
 */
#include <dlfcn.h>
#include <objc/runtime.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
dump_methods(const char *image, Class cls, int meta)
{
	unsigned int n = 0;
	Method *m = class_copyMethodList(cls, &n);
	for (unsigned int i = 0; i < n; i++) {
		IMP imp = method_getImplementation(m[i]);
		Dl_info info;
		if (!imp || !dladdr((const void *)imp, &info) || !info.dli_fbase) {
			continue;
		}
		printf("%s\t%lu\t%c[%s %s]\n", image, (unsigned long)((uintptr_t)imp - (uintptr_t)info.dli_fbase), meta ? '+' : '-',
		    class_getName(cls), sel_getName(method_getName(m[i])));
	}
	free(m);
}

int
main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: sweetobjcmap IMAGE...\n");
		return 2;
	}
	for (int a = 1; a < argc; a++) {
		if (!dlopen(argv[a], RTLD_LAZY | RTLD_GLOBAL)) {
			fprintf(stderr, "dlopen %s: %s\n", argv[a], dlerror());
		}
		unsigned int n = 0;
		const char **names = objc_copyClassNamesForImage(argv[a], &n);
		const char *leaf = strrchr(argv[a], '/');
		leaf = leaf ? leaf + 1 : argv[a];
		fprintf(stderr, "%s: %u classes\n", leaf, n);
		for (unsigned int i = 0; i < n; i++) {
			Class cls = objc_getClass(names[i]);
			if (!cls) {
				continue;
			}
			dump_methods(leaf, cls, 0);
			dump_methods(leaf, object_getClass((id)cls), 1);
		}
		free(names);
	}
	return 0;
}
