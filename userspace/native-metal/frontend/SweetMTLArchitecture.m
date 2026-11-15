/* SPDX-License-Identifier: MIT
 *
 * iOS 26 Metal asks every device for -targetDeviceArchitecture before it loads a library:
 * Metal.framework +[MTLLoader sliceIDForDevice:legacyDriverVersion:airntDriverVersion:] (an
 * objc_direct method, 23G90 at 0x1860fb334) does
 *
 *     arch = [device targetDeviceArchitecture];          // nil: MTLReportFailure line 692,
 *                                                         // "Target device architecture is nil"
 *     *legacyDriverVersion = [arch driverVersion];        // 64-bit
 *     *airntDriverVersion  = [arch versionCombined];      // 32-bit
 *     return (uint64_t)[arch subType] << 32 | [arch cpuType];
 *
 * and picks the fat metallib slice with that (cputype, subtype). iOS 16 never asked (it used
 * -targetDeviceInfo), so the donor device inherits _MTLDevice's nil and backboardd aborted on 26b5c.
 * This answers the generic AIR slice: CPU_TYPE_AIR64 0x01000017 with the subtype iOS 26's QuartzCore
 * default.metallib uses for it (0xc). Every value can be overridden without a rebuild by
 * /var/local/sweet-mtl-arch ("cputype subtype driverVersion versionCombined", hex or decimal).
 * Selector names were read from the 23G90 dyld cache selrefs of that function.
 */
#import <Foundation/Foundation.h>
#include <objc/runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

@interface SweetMTLArchitecture : NSObject
@property (nonatomic) uint32_t cpuType;
@property (nonatomic) uint32_t subType;
@property (nonatomic) uint64_t driverVersion;
@property (nonatomic) uint32_t versionCombined;
@end

@implementation SweetMTLArchitecture
- (NSString *)name { return @"air64"; }
- (NSString *)description
{
	return [NSString stringWithFormat:@"<SweetMTLArchitecture cpu %#x sub %#x driver %#llx combined %#x>",
	    self.cpuType, self.subType, (unsigned long long)self.driverVersion, self.versionCombined];
}
@end

static SweetMTLArchitecture *sweet_arch;

static id sweet_target_device_architecture(id self, SEL _cmd)
{
	(void)self; (void)_cmd;
	static dispatch_once_t once;
	dispatch_once(&once, ^{
		SweetMTLArchitecture *a = [[SweetMTLArchitecture alloc] init];
		a.cpuType = 0x01000017u;   /* CPU_TYPE_AIR64 */
		a.subType = 0xcu;          /* QuartzCore 23G90 AIR slice subtype */
		FILE *f = fopen("/var/local/sweet-mtl-arch", "r");
		if (f) {
			unsigned long long v[4] = {a.cpuType, a.subType, 0, 0};
			int n = fscanf(f, "%lli %lli %lli %lli", &v[0], &v[1], &v[2], &v[3]);
			fclose(f);
			if (n >= 1) a.cpuType = (uint32_t)v[0];
			if (n >= 2) a.subType = (uint32_t)v[1];
			if (n >= 3) a.driverVersion = v[2];
			if (n >= 4) a.versionCombined = (uint32_t)v[3];
		}
		sweet_arch = a;   /* kept for the process lifetime */
		fprintf(stderr, "SweetNativeMetal[%s %d]: targetDeviceArchitecture -> cpu %#x sub %#x driver %#llx combined %#x"
		    " (iOS 26 MTLLoader slice selection; /var/local/sweet-mtl-arch overrides)\n",
		    getprogname(), getpid(), a.cpuType, a.subType, (unsigned long long)a.driverVersion, a.versionCombined);
	});
	return sweet_arch;
}

/*
 * iOS 26 QuartzCore sends methods the iOS 16 donor objects never had (26b5e: backboardd aborted on
 * -[SweetNativeMetalCommandBuffer setResponsibleTaskIDs:count:], an NSInvalidArgumentException). One build per
 * selector is too slow, so every class of this bundle gets -forwardingTargetForSelector: answering a sink: an
 * unimplemented message is logged once ("SweetNativeMetal: unimplemented -[Class sel] answered 0") and returns
 * 0 / nil instead of raising. -respondsToSelector: is untouched, so Metal's optional-feature probes are not
 * fooled. Methods with known meaning get real (no-op) implementations below.
 */
#include <dlfcn.h>
#include <string.h>

@interface SweetMTLSink : NSObject
@end

@implementation SweetMTLSink
- (NSMethodSignature *)methodSignatureForSelector:(SEL)sel
{
	/* Any number of pointer-sized arguments: the sink reads none of them, returns a zeroed pointer. */
	unsigned args = 0;
	for (const char *p = sel_getName(sel); *p; p++) args += (*p == ':');
	char types[64] = "@@:";
	for (unsigned i = 0; i < args && i < 16; i++) strlcat(types, "@", sizeof(types));
	return [NSMethodSignature signatureWithObjCTypes:types];
}
- (void)forwardInvocation:(NSInvocation *)inv
{
	id zero = nil;
	[inv setReturnValue:&zero];
}
@end

static SweetMTLSink *sweet_sink;
static NSMutableSet *sweet_told;

static id sweet_forwarding_target(id self, SEL _cmd, SEL sel)
{
	(void)_cmd;
	@synchronized ([SweetMTLSink class]) {
		NSString *key = [NSString stringWithFormat:@"%s %s", object_getClassName(self), sel_getName(sel)];
		if (![sweet_told containsObject:key]) {
			[sweet_told addObject:key];
			fprintf(stderr, "SweetNativeMetal[%s %d]: unimplemented -[%s %s] answered 0 (iOS 26 caller)\n",
			    getprogname(), getpid(), object_getClassName(self), sel_getName(sel));
		}
	}
	return sweet_sink;
}

/* -[MTLCommandBuffer setResponsibleTaskIDs:count:] (iOS 26 QuartzCore): attribution only; nothing to schedule. */
static void sweet_set_responsible_task_ids(id self, SEL _cmd, const void *ids, unsigned long count)
{
	(void)self; (void)_cmd; (void)ids; (void)count;
}

static void sweet_install_fallbacks(void)
{
	sweet_sink = [[SweetMTLSink alloc] init];
	sweet_told = [[NSMutableSet alloc] init];
	Dl_info info;
	if (!dladdr((const void *)sweet_install_fallbacks, &info) || !info.dli_fname) return;
	unsigned count = 0;
	const char **names = objc_copyClassNamesForImage(info.dli_fname, &count);
	SEL fwd = sel_registerName("forwardingTargetForSelector:");
	unsigned covered = 0;
	for (unsigned i = 0; i < count; i++) {
		if (!strncmp(names[i], "SweetMTLSink", 12) || !strncmp(names[i], "SweetMTLArchitecture", 20)) continue;
		Class c = objc_getClass(names[i]);
		if (c && class_addMethod(c, fwd, (IMP)sweet_forwarding_target, "@24@0:8:16")) covered++;
	}
	free(names);
	Class cb = objc_getClass("SweetNativeMetalCommandBuffer");
	if (cb) class_addMethod(cb, sel_registerName("setResponsibleTaskIDs:count:"), (IMP)sweet_set_responsible_task_ids, "v32@0:8^v16Q24");
	fprintf(stderr, "SweetNativeMetal: iOS 26 fallbacks on %u of %u classes (unimplemented messages answer 0 and are logged)\n",
	    covered, count);
}

@interface SweetMTLArchitectureInstaller : NSObject
@end

@implementation SweetMTLArchitectureInstaller
+ (void)load
{
	Class device = objc_getClass("SweetNativeMetalDevice");
	SEL sel = sel_registerName("targetDeviceArchitecture");
	if (device == Nil) {
		fprintf(stderr, "SweetNativeMetal: no SweetNativeMetalDevice class; targetDeviceArchitecture not added\n");
		return;
	}
	/* class_addMethod only adds when the class itself has no implementation (an inherited _MTLDevice
	 * one is overridden, which is the point: that one answers nil). */
	if (!class_addMethod(device, sel, (IMP)sweet_target_device_architecture, "@16@0:8")) {
		fprintf(stderr, "SweetNativeMetal: SweetNativeMetalDevice already implements targetDeviceArchitecture\n");
	}
	sweet_install_fallbacks();
}
@end
