# Connected A618 renderer build0110

This is a paired kernel and userspace implementation. It is not yet a
hardware-qualified acceleration release. Do not evaluate it by booting its
kernel without installing the matching Metal bundle.

## Pieces

- Patches0108–0110: 16KiB host memory, 4KiB GPU VM, A6xx PM4 submission,
  real BO/import/map/fence ABI, A618 render configuration, IOKit provider.
- NativeAdrenoMetal.bundle: iOS16 entry class, NativeMetal frontend,
  metal2vulkan translator and Darwin Turnip. The bundle includes its Vulkan
  dylib and licenses; copy it intact after signing.
- Existing IOSurface/framebuffer presentation path. Target120Hz is a requested
  presentation rate, not evidence of120 rendered frames per second.

Build inputs are pinned in tools/build-sweet-metal.py and the upstream
compiler manifests. Kernel and graphics CI were dispatched on1fae147 with
portable tests disabled. See NOTES.md for final run results and artifacts.

## Installation location and prerequisites

Metal discovers IOAcceleratorES, reads MetalPluginName=NativeAdrenoMetal and
loads /System/Library/Extensions/NativeAdrenoMetal.bundle. The intended
principal class is SweetAdrenoMetalDevice. The minimal entry admits only
backboardd and requires an actual Vulkan/NativeMetal device. It does not
pretend a device exists after initialization fails.

The bundle must be staged into the active iOS root filesystem or its active
union overlay before trying the paired kernel. Preserve any existing bundle
as a rollback copy. The root's executable-trust policy must accept the new
ad-hoc signed executable and dylib; a successful codesign check on the build
host establishes signature integrity, not device trust admission. Never
replace or flash the16GB root image merely to copy this bundle. Root writes
and a device boot have not been performed during this build phase.

The prior0108 boot image is a kernel rollback: it does not publish the new
provider. Keep partition backups and recovery intact.

## Evidence still needed on hardware

Compilation proves neither the private iOS16 Metal SPI nor completed GPU
rendering. The frontend uses the observed528-byte _MTLDevice base guard;
its upstream implementation targets iOS17, so further runtime API work may
be necessary. Only one compositor connection is admitted, with synchronous
GPU quiescence for VM/cache operations. Other clients and process isolation
are unfinished. iPhone8-equivalent performance is not established.

A useful trace must establish each of the following independently:

1. VM setup and actual CP fence pass with no SMMU/CP faults.
2. Render profile and IOKit provider publish successfully.
3. backboardd loads the signed bundle and Turnip opens the NativeAdreno ABI.
4. NativeMetal translates compositor work and submitted GPU fences complete.
5. IOSurface output is presented, with measured frame pacing and no fallback.

For live kernel capture see usb-live-logging.md. The USB console does not
capture every userspace NSLog. Keep recovery log collection available for
startup failures. No device testing is authorized by this build document.
