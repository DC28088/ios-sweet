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
}
@end
