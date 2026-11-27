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
 */
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
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

__attribute__((constructor)) static void
install(void)
{
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
