/* SWEET_DYLIB
 * sweet-icr-log.dylib: loaded into iconservicesagent (kernel boot-arg sweeticr=1, xnu 2028). Wraps
 * -[ICRFinalizedIcon serializedDataWithError:] (IconRendering) and, when it fails, appends the NSError that IconServices
 * discards ("Failed to serialize finalized icon ...") to /var/tmp/sweet-icr.log (first 40 failures). Changes no result.
 */
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <objc/message.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef id (*ser_fn)(id, SEL, id *);
static ser_fn orig;
static int logged;

static void
put_desc(FILE *f, const char *label, id obj)
{
	if (!obj) {
		fprintf(f, "%s: (nil)\n", label);
		return;
	}
	CFStringRef s = (CFStringRef)((id (*)(id, SEL))objc_msgSend)(obj, sel_registerName("debugDescription"));
	char buf[4096];
	if (s && CFStringGetCString(s, buf, sizeof(buf), kCFStringEncodingUTF8)) {
		fprintf(f, "%s: %s\n", label, buf);
	} else {
		fprintf(f, "%s: (no description)\n", label);
	}
}

static id
hook(id self, SEL cmd, id *err)
{
	id local = nil;
	id *e = err ? err : &local;
	id r = orig(self, cmd, e);
	if (!r && logged < 40) {
		logged++;
		FILE *f = fopen("/var/tmp/sweet-icr.log", "a");
		if (f) {
			fprintf(f, "=== failure %d (pid %d)\n", logged, (int)getpid());
			put_desc(f, "error", *e);
			put_desc(f, "icon", self);
			fclose(f);
		}
	}
	return r;
}

extern id MTLCreateSystemDefaultDevice(void);

/* 26c: why does RenderBox give this process no GPU device? Ask the same questions at load time and log the answers. */
static void
probe_devices(FILE *f)
{
	const char *keys[] = {"FORCE_SOFTWARE_RENDER", "RC_XBS", "ALLOW_GPU_DESPITE_XBS", "DYLD_INSERT_LIBRARIES", NULL};
	for (int i = 0; keys[i]; i++) {
		const char *v = getenv(keys[i]);
		fprintf(f, "env %s=%s\n", keys[i], v ? v : "(unset)");
	}
	id dev = MTLCreateSystemDefaultDevice();
	fprintf(f, "MTLCreateSystemDefaultDevice -> %s\n", dev ? object_getClassName(dev) : "nil");
	dlopen("/System/Library/PrivateFrameworks/RenderBox.framework/RenderBox", RTLD_LAZY);
	Class rb = objc_getClass("RBDevice");
	if (rb) {
		id d1 = ((id (*)(id, SEL))objc_msgSend)((id)rb, sel_registerName("sharedDefaultDevice"));
		id d2 = ((id (*)(id, SEL))objc_msgSend)((id)rb, sel_registerName("sharedLowPowerDevice"));
		fprintf(f, "RBDevice sharedDefaultDevice -> %s, sharedLowPowerDevice -> %s\n", d1 ? object_getClassName(d1) : "nil",
		    d2 ? object_getClassName(d2) : "nil");
	} else {
		fprintf(f, "RBDevice class not found\n");
	}
}

__attribute__((constructor)) static void
install(void)
{
	dlopen("/System/Library/PrivateFrameworks/IconRendering.framework/IconRendering", RTLD_LAZY);
	Class c = objc_getClass("ICRFinalizedIcon");
	Method m = c ? class_getInstanceMethod(c, sel_registerName("serializedDataWithError:")) : NULL;
	FILE *f = fopen("/var/tmp/sweet-icr.log", "a");
	if (m) {
		orig = (ser_fn)method_setImplementation(m, (IMP)hook);
	}
	if (f) {
		fprintf(f, "sweet-icr-log loaded in pid %d: %s\n", (int)getpid(), m ? "hooked" : "ICRFinalizedIcon not found");
		probe_devices(f);
		fclose(f);
	}
}
