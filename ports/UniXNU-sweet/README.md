# UniXNU sweet: iOS16 migration

The owner selected UniXNU's porting architecture while retaining iOS16.7.16,
20H392, iPhone10,3/D22AP. This device directory follows the upstream board,
defconfig and feed layout. It is an initial source configuration, not a
bootable port. The upstream `example` readiness flag deliberately prevents
building or packing an incomplete board.

## First milestone

Build an exact sweet UEFI with framebuffer, button input, storage protocols and
the UniXNU menu. Then integrate DarwinBoot and a diagnostic XNU payload. Use
temporary fastboot boots for bring-up. The running HFS+ iOS installation and
its recovery remain the fallback; no new userdata layout is selected here.

An exact-board Mu-Silicium reference exists in tagicmi's fork (pin in
sources.json). Its sweet.conf places the3MiB firmware at0x9fc00000; the adjacent
ports/uefi-sweet directory translates that input to the current TOML format.
The old package uses DeviceMemoryMapLib/DeviceConfigurationMapLib and legacy
paths; current Mu uses a different configuration/build contract. Reconcile
those interfaces, vendor dependencies and output naming before compiling.
Current Project-Silicium has sweet_k6a, a different device. Do not substitute it.

## iOS16 integration

The base UniXNU tree selects XNU10063/Darwin23 and iOS17 deployment targets.
This directory overrides XNU to8796.141.3/Darwin22 and libpthread514.100.2.
These overrides select source identities; they do not make the current4KiB
patches, corecrypto14, prelink ABI or Metal builders compatible with iOS16.
Adapt those interfaces deliberately. Audit the selected iOS16 kernelcache
before setting prelink roots, slides, initialization addresses and trust pins.
NativeAdreno's A740 power/command implementation still needs an A618 backend.

Do not import the old stub IOSurface into the prelinked path blindly: evaluate
using the matching original iOS16 IOSurface/IOMobileGraphics modules instead.
Storage should start with a diagnostic payload; board power and UFS must be
adapted before mounting the installed root. The current root is HFS+; upstream's
raw APFS windows do not describe that on-device layout. GPT LBAs and UUIDs are
intentionally absent until measured, rather than copied from sheng.

## Configure the workspace

From a UniXNU checkout at the source pin in sources.json:

```
./unixnu defconfig sweet_ios16_defconfig -D /home/dan/ios-sweet/ports/UniXNU-sweet --out /home/dan/ios-sweet/research/unixnu-ios16-out
./unixnu status -D /home/dan/ios-sweet/ports/UniXNU-sweet --out /home/dan/ios-sweet/research/unixnu-ios16-out
```

These commands configure/report the source workspace. They do not compile,
boot, write the phone or establish hardware compatibility. Keep proprietary
firmware, Apple components and device-specific inputs outside Git.

References: https://unixnu.com/porting/ and its firmware/device-tree pages.

## Initial build artifact

The unixnu-menu workflow builds generic AArch64 UniXNU.efi/UniXNUDxe.efi on
Ubuntu from pinned sources. Dispatch the existing xnu workflow on this branch
with uefi_menu=true, graphics_only=false, run_tests=false. This compiles the
menu applications, not a sweet firmware/boot image or the iOS16 kernel. The
board remains gated until its firmware integration is complete.

The target raw kernelcache, DeviceTree and StaticTrustCache were extracted by
HTTP ranges with ZIP CRC checking, SHA256-pinned in board.toml, and retained
outside Git. The kernelcache is LZFSE and was decoded on Fedora using the
already available lzfse library. Its original version is8796.142.1.703.8; the
source baseline8796.141.3 therefore needs an explicit prelink ABI audit.

The firmware CI stage builds the pinned legacy sweet firmware with the two
compiled EFI images embedded in its main FV. The registrar keeps its variable
protocol DEPEX and installs the UniXNU boot option. This prototype uses the
legacy firmware's own BootShim/Android-image packaging and pinned submodules;
it does not turn the incomplete current board.toml into a kernel-ready port.
The output name is SWEET_UEFI.fd (confirmed from its source packaging script).
NativePrelink's input parser also reads the selected iOS16 kernelcache:18
segments and239 prelinked components. No native kernel has been linked yet.

## First UEFI prototype built

CI39636995649 succeeded on4d353bf after pinning setuptools80.9.0 for the
legacy build tools. Verified artifacts are in
research/unixnu-ios16-out/firmware-39636995649: Mu-sweet.img and SWEET_UEFI.fd.
The image contains the generic UniXNU application and registrar in the sweet
FV. It does not contain DarwinBoot or a native iOS16 kernel/payload. It has not
been booted on the phone. The compiled FD, ARM64 BootShim header and appended
DTB were inspected against the packed image, and published SHA256 checks pass.
See ../uefi-sweet/initial-build.json for exact hashes and header parameters.

The reference firmware DTB has matching SM7150 ID365 and generic board ID0;
the saved phone tree has board ID46. This difference is recorded for the first
boot investigation. No device tree or partition on the phone has been changed.
