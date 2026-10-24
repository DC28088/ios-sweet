# A618 / iOS16 reference: sunfish

Reviewed 2026-10-24, pinned
[Yofuness/ios-flame sunfish f09fdb58](https://github.com/Yofuness/ios-flame/tree/f09fdb58a2c074d131072947b247240532b7d97d).
The inherited top-level README describes Pixel4/A640; use `docs/sunfish/` for
Pixel4a/A618 observations. Reported results are the authors', not independently
reproduced on sweet.

## Relevant published work

- [Status](https://github.com/Yofuness/ios-flame/blob/f09fdb58a2c074d131072947b247240532b7d97d/docs/sunfish/status.md):
  iOS16.7.16 iPhoneX userland, A618 Metal via SweetMetal2 → metal2vulkan → Turnip,
  565MHz, about21 panel updates/s. Smoothness and BO leaks remain work in progress.
  The current Wi-Fi row reports scan/join/DHCP, while the same page retains
  contradictory older failure notes; no independent radio verification here.
- [Performance](https://github.com/Yofuness/ios-flame/blob/f09fdb58a2c074d131072947b247240532b7d97d/docs/sunfish/display-perf.md):
  direct IOSurface aliases avoid GPU image copies; 64-byte row alignment;
  CoreAnimation vsync gating; presentation waits behind the render queue.
  The authors report no gain from increasing430→565→650MHz once pacing is the
  limit. Thus a clock change is not a replacement for the userspace/present port.
- Turnip patches0103/0104 and bridge patch0102 handle explicit layouts,
  dedicated allocations and read-only aliases. Sweet must retain its own backing
  bounds/SMMU permissions; do not infer permission to read beyond mapped memory.

## Wallpaper prerequisites found in the engineering log

[HANDOFF, black wallpaper](https://github.com/Yofuness/ios-flame/blob/f09fdb58a2c074d131072947b247240532b7d97d/HANDOFF.md)
records the same PosterBoard first-configuration assertion:

1. MobileGestalt waits for a WLAN IOService for every bundle when the chosen
   MAC properties are missing. Provide stable locally administered identity
   placeholders; they do not make a radio work.
2. ExtensionFoundation asks the AppleMobileFileIntegrity IOService, selector6,
   whether signed extension caches are trusted. A missing service prevents
   extension-point discovery in their port.
3. Once both prerequisites worked, a one-time LaunchServices database rebuild
   produced515 extension records and10 poster providers. Their raw per-plugin
   registration calls alone were insufficient.

Prepared sweet adaptations:

- `tools/mkadt.py` now emits chosen MAC identities; `tools/sweet-mg-adt.py` adds
  missing identities to an existing firmware ADT while preserving existing MACs.
- Patch0121 adds an opt-in (`sweetamfi=1`) compatibility IOService. It supports
  only the expected cache-trust query shape; other selectors remain unsupported.
  This is a permissive development-port service, not signature verification or
  a complete AMFI implementation. Source adapted from their patch0103, MIT;
  copyright/license retained through the repository MIT license and source notice.
- `tools/prepare-backboard-vsync.py` prepares a new merged launchd plist with
  `CA_FORCE_TIMER=1` for BackBoard only, preserving other jobs and the disabled
  wallpaper registrar. Their source says this selects the IOMFB notification
  path for synthetic A11 identity; setting it in a late-loaded dylib is too late.

These files are prepared locally; active phone has none of these new
prerequisites yet. The old plugin-registration job remains disabled. Do not
re-enable it or claim wallpaper repaired before observing provider discovery and
actual rendering. A fresh recovery snapshot/approval is needed for a root update.

## Metal integration to adapt

Use their iOS16 `phone/metal2/{SweetMetal2,NativeBridgeMetalFrontend}.mm`, Rust
bridge and `phone/turnip` build/patch set. The NativeAdreno header is ABI1, but
only advertises HostImport. Sweet additionally requires SubmitResources and a
4096-object budget. Turnip's Darwin backend must preserve complete BO residency
through method17, physical-page cache ownership, real fences and our16KiB host
pages. Do not fall back to maintaining the entire VM per submit or remove waits
without equivalent ownership/completion guarantees.

Retain sweet's A618 firmware, GPUCC/SMMU initialization, video-mode display,
2400-row panel and partition layout. Sunfish's2340-row presentation constants and
bootloader/PMIC addresses are not interchangeable. This is the strongest current
same-GPU/iOS16 donor, but its source is not installed merely by checking it out.

## Chlorophyll update

[Chlorophyll 2a463aed](https://github.com/ry-diffusion/Chlorophyll/tree/2a463aedd8c761d6b9c3868c5f2b5299f12437b3)
adds a system Metal bundle, targetDeviceInfo, handling for empty colour slots and
a CoreImage CIKL graph workaround since the earlier a95c328 review. Its current
build is still iOS18/arm64e over Gallium/A630. Those fixes matter to a future
Metal frontend; they do not fix the current GLES PosterBoard registration failure.
