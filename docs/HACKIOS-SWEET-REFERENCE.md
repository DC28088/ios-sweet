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
on b3a8 surfaces with zero plane pitch; later fences time out. The existing
library is installed in userdata. The proposed original-root restore was
never executed, so no restoration of that library was necessary.

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

Source preparation completed locally. GitHub rejected the GLES dispatch with
HTTP422 "Actions has been disabled for this repository" despite the repository
permissions API returning enabled=true. No new library build has started.
Existing0116 library remains installed; no userdata changes or phone boot were
performed during this adaptation. A compiled replacement must be staged and
verified before any approved raw-root update. Firmware files alone do not
make Wi-Fi/audio or Metal available.
