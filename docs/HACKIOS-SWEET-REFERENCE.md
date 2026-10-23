# HackiOS reference adaptation to the original Sweet port

Reviewed2026-10-23. Continue `/home/dan/ios-sweet`, branch
`codex/adreno-power-audit`, iOS16.7.16 HFS+ and the direct boot shim.

## Source pins and reported scope

- Kernel: [HackiOS d82c64a3](https://github.com/oosexgang/hackios-xnu-kernel/tree/d82c64a3a31cd27a0a0afbf6e213e467e44b1b4c).
- Userland: [HackiOS a79ff077](https://github.com/oosexgang/hackios-xnu-userland/tree/a79ff0772a3454fd1ef127e181e5b3bce6c9ce18).
- Original source licenses remain applicable; no Apple/vendor files are copied
  into Git. Clean reference checkouts are local under `research/`.

The current README reports GLES UI rendering on Adreno642L, WCN6750 Wi-Fi,
and audio. It explicitly lists Metal for applications as unfinished; its
AIR shader work is not a complete Metal API implementation. Kernel changes
since the earlier8e611870 pin touch setup.sh and the EFI loader, not new GPU,
Wi-Fi or audio drivers. The features need hardware adaptation on Sweet.

## Concrete GPU changes

Fresh0116 log: `logs/gpu-0116-failed-20261023.txt`.
NativeAdreno ready1, Mesa screen and EAGL installed. Rendering then fails
on b3a8 surfaces with zero plane pitch; later fences time out. The library was installed for0116. The user later confirmed manually restoring
userdata, and a fresh0117 library installation was staged against that root.

1. HackiOS `xnuloader/loader.c:dt_identity` advertises sRGB for its8-bit BGRA
   framebuffer. Its comment explains that P3 causes unsupported b3a8/w30r
   surfaces. Apply the same restriction to Sweet's copied product identity.
   `tools/sweet-gles-adt.py` edits only the gamut properties in the existing
   firmware-bearing ADT, retaining GPU blobs and all other nodes/properties.
   `tools/mkadt.py` also uses the corrected identity for future generated ADTs.
2. HackiOS `userland/src/eagl.c:surface_to_texture` distinguishes planar from
   non-planar IOSurfaces. Adapt that rule in the existing Sailfish bridge:
   non-planar surfaces use ordinary width/height/row/BPE/base getters, real
   planes use the plane APIs. Validate plane index, dimensions and row extent
   before constructing the Mesa resource. This corrects the observed zero
   pitch/BPE instead of interpreting an unsupported deep-colour format as BGRA.

These changes require a rebuilt sweetgl.dylib and matching sRGB boot ADT.
They do not establish correct rendered frames or resolve every fence failure.
Retain the established A618 firmware/power path and NativeAdreno ABI1; do not
copy Redwood A660-family initialization or the16.5 QuartzCore patch addresses.

## Firmware collected from this phone

Read-only modem VFAT and vendor EROFS mounts under recovery `/tmp/` were used
and unmounted after collection. `tools/pull-sweet-firmware.py` verified remote
and host SHA256 for103 files,88008754bytes, at
`research/sweet-firmware-20261023/`. All remain private/ignored. Credentials,
WLAN MAC files, EFS and persist were excluded. GPU files match the previous
collection exactly; firmware replacement is not the current GPU fix.

| Subsystem | Sweet evidence | HackiOS Redwood assumption | Required adaptation |
|---|---|---|---|
| GPU | A618 GMU, a630_sqe.fw, a615_zap.* | A660-family blobs, Adreno642L | Existing Sweet backend; port frontend/layout corrections |
| Wi-Fi | qcom,wcn3990-wifi at18800000, ath10k calibration variant xiaomi_sweet; wlanmdsp.mbn and board files | WCN6750, WPSS image, ath11k | WCN3990/ath10k transport, modem/PIL and board calibration |
| Audio | adsp.mdt and b00–b26; PMOS names two AW88263S amps | Redwood ADSP memory/rails, codec and speaker routes | Sweet ADSP carveouts, APR/GLINK services, codec and amp routing |
| Battery | Recovery bms driver qcom,qpnp-qg | PMIC GLINK/ADSP battery cache from resident UEFI | Sweet QG measurements and IOPMPowerSource publication |
| Haptic | AW8624 driver and firmware | AW8697 | Sweet I2C/reset/interrupt/RTP format |

Reference for public board facts:
`research/gpu/pmos-pinned/sm7150-xiaomi-sweet.dts` and `sm7150.dtsi`.
Sweet already has an optional read-only PM6150 battery observer in patch0062;
its voltage-derived percentage is an estimate, and its default static values
must not be described as a working calibrated battery gauge. Copying Redwood's
UEFI-dependent battery backend into the direct-shim port would not supply it
with a data producer. Wi-Fi/audio/battery/haptic runtime ports are unfinished.

## Build and installation status

Source preparation completed locally. The first dispatch failed with HTTP422,
then the user requested a retry. The macOS14 run39981853003 was queued during
its scheduled brownout and canceled. Replacement run39982823642 on4f144ff
uses macOS15 and succeeded. Artifact12515411703 has been downloaded and its
archive/API digest, CRC and file hashes verified.

The user confirmed manually restoring userdata after the black-screen boot.
Fresh read-only inspection and an HFS+ mount in a local overlay confirm that
sweetgl.dylib and its installation record are absent. A fresh staging area is
`research/gles-install-0117/`; the previous0115 block cache/delta must never be
reused after that restore. Filesystem repairs made during staging affect only
the local qcow2 overlay. No phone blocks have been changed.

Matching image: `shim/out/shim-boot-0117-gles-srgb-120hz.img`, SHA256
`225203f037160ce67f549a1d11104eab97b22f86d58bfa73eff802184a8a8b17`.
It uses the compiled0116 kernel and corrected sRGB ADT, and requires the new
GLES library once built. The library is built at `userspace/out/0117-gles-4f144ff-range/sweetgl.dylib`
(18790896bytes), SHA256
`2eb01550cbddc2619c7feaf36485935052c934acd577ee55eccaeedfafd83b09`.
The user approved the48-chunk installation (21233664bytes/20.25MiB) and a
temporary0117 boot. All original blocks matched, and every written chunk passed
readback SHA256. Originals remain available for rollback. Temporary boot was
accepted, and live USB captured312416bytes: iOS userspace, A618 provider and
Mesa/EAGL started. Formats are now BGRA1125x2436 with pitch4608. The UI remains
black because Mesa rejects the imported framebuffer layout. No accelerated
presentation has been established.

## Follow-up: A6xx import extent

The pinned Mesa `freedreno_screen.c:fd_screen_bo_from_handle` passes size0 to
`fd_bo_from_handle` for KMS handles. These IOSurface imports have not previously
been registered as Mesa BOs. `fd6_resource.cc:layout_resource_for_handle`
rejects `layout.size > fd_bo_size(bo)`, so their zero extent prevents import.

The new Darwin-only patch `userspace/gles/mesa-iosurface-size.patch` obtains
the actual allocation extent from the transport BO table, validates the32-bit
size range and passes it into Mesa's import. Mesa's layout/buffer bounds checks
remain intact. Source b2cd6ea compiled successfully in CI39988038341. Library SHA256:
`c1af3c165dda6249b1125b3d9a65ee2e5790faaa3521ae32ab52f141149d1fcd`.
The user approved the fresh 0118 update: 35 chunks / 20119552 bytes. All
original blocks matched immediately before writing, and all written chunks
passed readback SHA256. The temporary boot reached iOS userspace. Live USB
captured 456029 bytes in `logs/live-0118-usb.txt`; reconnect replay duplicated
some startup output.

The main 1125×2436 BGRA framebuffer imports now pass with pitch 4608 and a
11239424-byte allocation. Later rendering still has method 6 fence timeouts
(`0xe00002d6`), long GL finish waits, and layout failures for 281×609 surfaces.
The user reports significantly worse performance. **0118 is a regression, not
a completed GPU acceleration fix.** Wi-Fi, haptics and audio remain unported.

A CPU fallback was packaged at
`shim/out/shim-boot-0118-cpu-fallback-120hz.img` (SHA256
`84c745e436a3095da8eb4821d557c00c4970cac5f7ab1c1433ea1f34a49e9a1f`).
It disables GLES injection and the GPU worker with `sweetgles=0 sweetgpu=0`,
retaining the installed library on disk. The user authorized its temporary boot, accepted by fastboot
(send 0.518 s, boot 0.130 s). Live USB confirms the disabled GPU worker, correct
boot arguments and iOS userspace/SpringBoard startup. The 150-second capture
finished with 183787 bytes. The user confirms the home screen and previous
speed restored. The phone remains in this temporary CPU-rendering boot.
No partition flashing or userdata rewrite was performed.
