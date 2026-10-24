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

0123 includes these prerequisites. Kernel build40111479897 and helper build
40111462084 passed on source30bad3279c06. A fresh OrangeFox snapshot was edited
in a local overlay; HFS checks and read-only payload hashes passed. The 12-chunk,
1.6875 MiB userdata update was installed and readback verified under the user's
standing installation authorization. The old plugin-registration job remains
disabled. A separate root job attempts a one-time LaunchServices rebuild after
180seconds, records the attempt first, and refuses automatic retry after an
incomplete attempt. The matching image was temporarily booted; wallpaper
visibility and performance remain unverified pending this boot's observations.

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

## SweetMetal2 build and first device attempt

The Sunfish frontend, Rust bridge and Turnip now build through
`tools/build-sweet-metal2.py` and the `sweet-metal2` workflow. Bundle build
40121858239 succeeded on e2ca25c; the 0124 installation record shows all
54 userdata chunks (40,108,032 bytes) installed and readback verified.
The build uses the pinned source revisions above and reconstructs the kernel ABI
header from the patch series. The original GLES library is retained.

The 0124 live trace shows `sweetgles=0 sweetmtl=2`, publication of
`MetalPluginName=SweetMetal2`, and Safari/WebKit loading the plugin and receiving
its intentional client-policy refusal. It shows **no BackBoard plugin startup
and no GPU submission calls**. The user's unchanged UI performance therefore
does not establish either working or slow Metal rendering: this boot's UI never
reached the new renderer.

BackBoard starts before the asynchronous GPU initialization publishes the
accelerator. The next kernel restarts BackBoard once after publication so its
fresh Metal discovery can find the service. `sweetm2restart=0` disables this;
GLES boots do not restart it. The initial restart patch (203d781, build40123710586)
failed to compile. The revised patch uses the IOKit-visible process KPIs in
`sys/proc.h`, avoiding BSD-private `psignal` and direct process-structure fields.
Build40125083266 on9a63995 succeeded. The 0125 live trace confirms the restart
and BackBoard entering SweetMetal2. Device creation then refuses with
`ShaderCache ... /var/local/m2v-cache/backboardd ... PermissionDenied` before
Turnip loads. The shader cache must be provisioned in the mounted root using
`tools/prepare-sweet-metal2-cache.sh`: root-owned0755 parent, mobile501:5010700
child. The plugin's unchecked mkdir/fallback does not create it successfully on
this root. The directory repair was installed as an eight-chunk, 576 KiB userdata delta;
every chunk passed readback verification. The unchanged0125 kernel was
rebooted with a fresh live trace. UI Metal rendering remains unverified.

Compiling against the extended ABI header does not implement its new methods.
The donor Turnip backend still calls legacy Submit (method5), which remains
supported but publishes and retires caches across the mapped VM. It does not
construct a complete method17 resource set. Preserve that distinction when
interpreting subsequent performance, and retain real completion, SMMU bounds
and cache ownership when implementing the scoped path.

## Chlorophyll update

[Chlorophyll 2a463aed](https://github.com/ry-diffusion/Chlorophyll/tree/2a463aedd8c761d6b9c3868c5f2b5299f12437b3)
adds a system Metal bundle, targetDeviceInfo, handling for empty colour slots and
a CoreImage CIKL graph workaround since the earlier a95c328 review. Its current
build is still iOS18/arm64e over Gallium/A630. Those fixes matter to a future
Metal frontend; they do not fix the current GLES PosterBoard registration failure.
