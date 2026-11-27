/* SWEET_DYLIB
 * sweet-mtlfail.dylib: loaded into SpringBoard (kernel boot-arg sweetsbmps=1, xnu 2029). Installs Metal's report-failure
 * block, so an MTLReportFailure logs to /var/tmp/sweet-mtlfail.log (first 200) and returns instead of aborting.
 *
 * Why: with the GPU, SpringBoard prewarms an MPSImageGaussianBlur (Siri's orb). At metal_profile=0 MPS finds no MPSDevice
 * (MPSKernel.mm:60), at metal_profile=1 our AIR frontend cannot compile MPS kernel D8F18H (MPSLibrary.mm:639); both go
 * through MTLReportFailure -> __assert_rtn -> abort, and launchd reboots after three SpringBoard crashes.
 * MTLReportFailure (iOS 26.6.2 Metal) first loads a global block and, when it is set, calls it with
 * (type, function, line, message) and returns: that global is what MTLSetReportFailureBlock stores.
 * This only stops the abort; the MPS kernel that failed still has no pipeline.
 *
 * Also (any boot with 2029): if /var/local/sweet-sb-keeppages holds N > 0, +[SBFolderView
 * countOfAdditionalPagesToKeepVisibleInOneDirection] answers N instead of 0, so SpringBoard keeps the icon views of N pages
 * on each side alive instead of building them on every page swipe (_addIconViewsForIcons / removeIconView, NOTES 2026-11-27).
 */
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <objc/runtime.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#define LOG_PATH "/var/tmp/sweet-mtlfail.log"

typedef void (^fail_block)(uint64_t type, const char *function, uint64_t line, CFStringRef message);
typedef void (*set_block_fn)(fail_block);

static atomic_int reports;

static void
log_line(const char *fmt, const char *a, uint64_t b, uint64_t c, const char *d)
{
	FILE *f = fopen(LOG_PATH, "a");
	if (!f)
		return;
	fprintf(f, fmt, (int)getpid(), a, (unsigned long long)b, (unsigned long long)c, d);
	fclose(f);
}

static long keep_pages;

static long
keep_pages_imp(id self, SEL _cmd)
{
	(void)self;
	(void)_cmd;
	return keep_pages;
}

static void
install_keep_pages(void)
{
	FILE *f = fopen("/var/local/sweet-sb-keeppages", "r");
	if (!f)
		return;
	long n = 0;
	if (fscanf(f, "%ld", &n) != 1)
		n = 0;
	fclose(f);
	if (n <= 0 || n > 8)
		return;
	keep_pages = n;
	const char *names[] = {"SBFolderView", "SBRootFolderView", NULL};
	SEL sel = sel_registerName("countOfAdditionalPagesToKeepVisibleInOneDirection");
	for (int i = 0; names[i]; i++) {
		Class c = objc_getClass(names[i]);
		/* only a class method the class itself defines (a subclass that inherits picks the new one up) */
		unsigned count = 0;
		Method *list = c ? class_copyMethodList(object_getClass((id)c), &count) : NULL;
		int done = 0;
		for (unsigned k = 0; k < count; k++) {
			if (method_getName(list[k]) == sel) {
				method_setImplementation(list[k], (IMP)keep_pages_imp);
				done = 1;
			}
		}
		free(list);
		log_line("pid %d: %s (%llu %llu) %s\n", names[i], (uint64_t)n, (uint64_t)done,
		    c ? (done ? "countOfAdditionalPagesToKeepVisibleInOneDirection replaced" : "has no own class method") : "class not found");
	}
}

__attribute__((constructor)) static void
install(void)
{
	install_keep_pages();
	set_block_fn set = (set_block_fn)dlsym(RTLD_DEFAULT, "MTLSetReportFailureBlock");
	if (!set) {
		dlopen("/System/Library/Frameworks/Metal.framework/Metal", RTLD_LAZY);
		set = (set_block_fn)dlsym(RTLD_DEFAULT, "MTLSetReportFailureBlock");
	}
	if (!set) {
		log_line("pid %d: %s (%llu %llu) %s\n", "MTLSetReportFailureBlock not found, failures still abort", 0, 0, "");
		return;
	}
	set(^(uint64_t type, const char *function, uint64_t line, CFStringRef message) {
		int n = atomic_fetch_add(&reports, 1);
		if (n >= 200)
			return;
		char buf[2048] = "(no message)";
		if (message)
			CFStringGetCString(message, buf, sizeof(buf), kCFStringEncodingUTF8);
		log_line("pid %d: %s line %llu type %llu: %s\n", function ? function : "?", line, type, buf);
	});
	log_line("pid %d: %s (%llu %llu) %s\n", "Metal report-failure block installed", 0, 0, "");
}
