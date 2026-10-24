# Claude Code continuation prompt — sweet, after 0126

Continue this existing project as the implementation engineer. Work in
`/home/dan/ios-sweet`, branch `codex/adreno-power-audit`, fork
https://github.com/DC28088/ios-sweet. Read `AGENTS.md`, the active TOP of
`NOTES.md`, recent commits and `git status` first. Preserve other agents' work.
The historical sections contain obsolete blockers; use dates and actual logs.

## Latest confirmed result — 2026-10-25

0126 is installed and temporarily booted. The user confirmed: **“glitches
improved; performance is the same.”** Continue from this partial correctness
improvement, not from an assumed acceleration/performance success.
Live USB is receiving `logs/live-0126-metal2-presentation.txt` (receiver started
for3600seconds before boot; verify it is still running when you begin).
Initial283450-byte prefix: MTLFunction adopted, owned-row alignment64,
Adreno618 Metal device created, presentation gate successes throughsample120,
no source-cache-acquire refusals, no MTLFunction validation warnings;74 explicit
layout refusals still occur (external/buffer layouts may use shadow fallback).
Sampled fence240: gpu_wait_us72698, cache_retire_us10410, result0. These are
individual samples, not a controlled frame-rate benchmark. Performance,
wallpapers, Wi-Fi, audio, Bluetooth, haptics and brightness are still unfinished.
The user requested this handoff after boot; further implementation is for you.

## My requested outcome

Finish correct, smooth GPU acceleration on Redmi Note 10 Pro/sweetin
(SM7150/Snapdragon732G, Adreno618), with working lock/home wallpapers and accurate
blur, then implement Wi-Fi, sound, Bluetooth, haptics and brightness control.
Aim for one coherent end-to-end implementation pass and one coordinated image
update, using all available source and logs before another device round trip.
Do not call this finished because a GPU service loads, a fence completes, an
API reports support, or compilation succeeds. Identify and fix the real
rendering/performance blockers. If evidence disproves a hypothesis, change the
implementation and explain it. Do not promise that every subsystem can be
completed in a single attempt; report actual remaining blockers honestly.

Keep the original direct boot shim + iOS16.7.16/20H392 HFS+ root in userdata.
Do not restart the separate `/home/dan/hackios-sweet` UEFI/APFS experiment.
HackiOS and UniXNU are references to adapt, not replacement boot architectures.

## User authorization and workflow

I already authorized ADB/fastboot, installing staged root updates and temporary
boots: “also no need to ask me for permission to boot or install these. just do
it.” Keep that authorization for this scope. Still save all original blocks,
freshly compare them before writing, verify every write, preserve recovery,
bootloaders/GPT/modem/persist/calibration and backups, and use `fastboot boot`.
Do not erase/recreate userdata or change partition layout for a driver fix.
Only ask me for necessary physical actions or genuinely missing hardware input.
Commit completed source and update NOTES with exact CI/image/log results.
Keep Apple files, firmware dumps, logs and identifiers out of Git. Build with
GitHub Actions via `gh`; kernel workflow inputs `run_tests=false`,
`keep_going=false`. No added test suites were requested.

## Current implementation and the just-installed 0126

The iOS UI path is now:
Core Animation → SweetMetal2 Metal frontend → metal2vulkan → Mesa Turnip Darwin
backend → NativeAdreno ABI1 → real A618 commands/fences → CPU framebuffer copy.
It is NOT simply CPU fallback anymore, but correct/smooth Metal rendering is
not yet established. Kernel client currently accepts backboardd only, with one
connection. Other apps/extensions do not automatically have Metal support.

0125 after shader-cache repair created the Turnip Adreno618 device. Its saved
trace shows893 commits/893 completions/0 failed commits; mean commit-to-complete
78263us, max397ms. These are end-to-end command timings, not GPU-only times or
physical display FPS. User: similar speed, missing regions, trails and cutoffs.
MTLFunction conformance warnings and explicit image-layout warnings occurred.
Runtime shadow fallbacks mean layout warnings do not prove texture creation
failed: the final texture-refusal counter was0. First-use pipeline creation
also cost20.16s. Inspect per-frame work, shader compilation/cache, shadow copies,
all-mapped-BO cache operations, fence waits and presentation separately.

0126 kernel CI40127896529 succeeded on
`e92bce0148f2745c33c23ab2bcf3c9f52a974626`.
Kernel: `xnu/out-0126-range/kernel.development.sweet`, SHA256
`9aad4df66dcd3e15abc7dc7312f4bf3bc2ef5f73fb2373e9d83380ca32d2cffe`.
Image: `shim/out/shim-boot-0126-metal2-presentation.img`, SHA256
`5b3ec7eb7b36691c9f4ae4150e71a30e83b331da9f1b2b2afe66665c0c9c5736`.
Packaging manifest: `research/gpu/0126-boot-manifest.json`.
Uses the existing verified shim/DTB/firmware ADT, sweetbuild126,
sweetgles=0, sweetmtl=2, sweetpresentwait=1, sweetfb_hz=120.
The latter is a SOFTWARE timer cap; physical120Hz/120fps is unmeasured.
HFI configured267MHz/rawcorner64; actual clock is unmeasured. Derive any clock,
voltage, GMEM or panel changes from Sweet board evidence and readback.

Final library CI40128850465 succeeded; source `3e219dbfd6941f1c14f1c6e97babe3dbb3c82812`.
Use `userspace/out-0126/metal2-build.json` and SweetMetal2.bundle there.
Frontend SHA256 `470cbb93cdeba870db47ce7b53ff348458e2191b91246c6beacbc391286aab80`.
Turnip SHA256 `7c391fb0da42766aac17b0a725b6a95347967faa86490763497d09729b244b02`.
The initial bundle CI40127901139 is preliminary and was NOT installed;
`userspace/out-0126-preliminary` is retained only as a local artifact.

New source:
- `xnu/patches/0124-sweet-metal-owned-surface-rows.patch`:64byte owned,
  single-plane rows, at least128 for8byte texels; external address/plane layouts
  are preserved. `sweetsurfalign=0` disables this new alignment.
- `xnu/patches/0125-sweet-metal-cpu-presentation-fence.patch`: opt-in shared
  submission gate, real CP quiescence and physical cache acquire before CPU
  source copying. GPU errors retain the previous panel frame. Never fabricate
  completion or bypass image/VM bounds checks.
- `userspace/metal2/patches/0001-sweet-function-protocol.patch`: actual NBFunction
  objects adopt MTLFunction; no fabricated shader handles/capabilities.
- `0002-sweet-cpu-presentation-submit-order.patch`: optional wait inside commit
  is OFF by default. The captured CA already waited scheduled893times; preserve
  donor async commit unless SWEET_CPU_PRESENT_WAIT=1 is deliberately enabled.
  A scheduled wait is not GPU completion, and callback/lock ordering matters.

The pinned sources are recorded in the build report: Sunfishf09fdb58,
UniXNUccd8b95, Mesa85fda3b, metal2vulkan720ef1c. A newer Sunfish branch head
4c67bd5911703550f45c838fd9de6c10f04ee79b was fetched separately, not checked out.
It adds optional draw logging/culling; verify semantics before updating pins.
Turnip still submits legacy method5. ABI17/scoped-resource declarations in the
header do NOT mean the userspace backend actually uses scoped residency.

Installation: `research/metal2-install-0126/delta.json` and
`installation-manifest.json`:50chunks/39911424bytes, every original saved,
freshly matched, all writes readback verified. Full bundle hashes/HFS checks
passed in a local overlay before writing. Root target is
`/System/Library/Extensions/SweetMetal2.bundle`.
Cache parent `/private/var/local/m2v-cache` is0:0/0755; backboardd child501:501/0700.
Wrong cache permissions previously prevented ANY Metal device creation.

After this iOS boot, ALL previous cached root snapshots/NBD bases/overlays are
STALE for a future update. Never reuse a before image across an iOS boot.
Take a fresh recovery snapshot, stage an overlay, save original changed blocks,
check them all against the current phone before writing and verify readback.

## Read previous logs AND current live USB logging

Start with:
- `logs/live-0126-metal2-presentation.txt` — current boot capture.
- Its `-capture.status` / `-capture.out` companions.
- `logs/live-0125-metal2-cache-fixed.txt` and
  `logs/gpu-0125-metal2-cache-fixed-recovery.txt`.
- `research/metal2-install-0126/share/boot-0125-telemetry/mtl2.log` and
  `pipelines.log`; `boot-0125-observations.json`.
- Earlier0117–0125 traces and active NOTES for regressions, not just latest
  success markers. Compare foreground animation windows, not idle intervals.

Current capture was started before0126 boot using:
`python3 tools/sweet-usb-log.py logs/live-0126-metal2-presentation.txt --duration 3600`
It may have expired by the time you start. Check the process and UDP5140 listener
before starting another receiver; keep prior logs and use a new filename.
USB Ethernet: host192.168.7.1, phone192.168.7.2; host MAC02:00:5e:77:ee:01,
phone MAC02:00:5e:77:ee:02. This is kernel UDP logging, NOT a live ADB shell.
ADB is available in OrangeFox; fastboot only in FASTBOOT. Inspect actual device
state. Request physical buttons only when those transitions cannot be automated.
Recovery pull: `bash tools/sweet-log-pull.sh logs/NEW-NAME.txt`.
The user's `/home/dan/Downloads/USB-SHIM-GUIDE.md` is also available.

0126 runtime observations will be appended below. Confirm them against the
current growing log and user report; temporary boot acceptance alone is not
proof of a home screen or corrected frames.

## Required reference repositories — compare actual current source

1. Pixel4a/sunfish, same A618:
   https://github.com/Yofuness/ios-flame/tree/sunfish
   Local `research/ios-sunfish-reference`; strongest GPU frontend/runtime/Turnip
   comparison. Inspect its actual sunfish docs and branch, not flame's top-level
   device description. See `docs/SUNFISH-A618-PORT.md`.
2. Redmi Note12/topaz, A610:
   https://github.com/MakrSas/ios-topaz/tree/topaz
   Refresh this branch: its README now reports newer GPU, audio and Wi-Fi work,
   with some older contradictory sections. Inspect the actual patches/userland
   and separate reports from source-backed functionality. Compare display
   presentation/cache, WLAN transport/iOS glue, audio and wallpaper registration.
3. HackiOS kernel and userspace:
   https://github.com/oosexgang/hackios-xnu-kernel
   https://github.com/oosexgang/hackios-xnu-userland
   Read both: GPU/Metal/AIR work and GLES integration, audio device/plugin,
   firmware/ADSP, Wi-Fi, injection/launch services. Do not assume every advertised
   feature is complete Metal or portable from their different board/EFI/APFS.
   `docs/HACKIOS-SWEET-REFERENCE.md` records older pins and Sweet adaptations.
4. UniXNU GPU/Metal stack:
   https://github.com/UniXNU/NativeMetal
   https://github.com/UniXNU/NativeAdreno
   https://github.com/UniXNU/UniXNU
   https://github.com/steelbrain/metal2vulkan
   https://unixnu.com/porting/
   Read the Metal frontend AND runtime AND Mesa KMD, plus kernel memory/fence
   ownership. NativeAdreno is an A740 driver reference; never transplant A7xx
   register settings or firmware/carveouts onto A618 unexamined.
5. UniXNU networking requested by the user:
   [NativeWiFi](https://github.com/UniXNU/NativeWiFi) currently redirects to
   [Ath12kWifi](https://github.com/UniXNU/Ath12kWifi)
   README describes ath12k/MHI/QRTR with newer PCIe radios. Sweet has WCN3990/
   ath10k evidence, so port reusable protocol/iOS interfaces and implement its
   actual transport rather than treating ath12k as a drop-in WCN3990 driver.
   https://unixnu.com/drivers/ath12k-wifi
6. Additional graphics references:
   https://github.com/ry-diffusion/Chlorophyll
   https://github.com/joranv1/ios-sailfish
   Chlorophyll's iOS18/arm64e Gallium path requires iOS16 adaptation. Sailfish's
   GLES/shader work is a reference; preserve the current Metal route unless an
   evidence-based design change is necessary.
7. Bluetooth lead:
   https://github.com/arabaci0711-ux/ios-vince
   `/home/dan/Downloads/BLUETOOTH-SUMMARY.md` and
   `docs/BLUETOOTH-SWEET-REFERENCE.md`. The earlier public pin lacked the patch
   mentioned in the summary; refresh branches/search, do not invent its source.
8. Sweet board truth: original https://github.com/dipa-studio/ios-sweet,
   postmarketOS/mainline Sweet device trees and Linux drivers. Local
   `research/gpu/pmos-pinned/sm7150-xiaomi-sweet.dts`, `sm7150.dtsi`,
   `backup/live-fdt.dtb`, `backup/dtbo.img`, `dtb/boot_unpacked/` and collected
   firmware `research/sweet-firmware-20261023/`. Verify local existence/pins.

## Subsystems and acceptance evidence

GPU first: correlate the new gate/row/protocol markers with completed commands,
frame correctness and the user's swipes, app open/close, recents and Control
Center. Diagnose missing regions/trails/cutoffs and real blur blend/precision,
texture alias/shadow/load/store/scissor issues. Measure submission, cache,
compilation and presentation time; do not merely remove waits or claim120fps.
Fix kernel and userspace together where the contract requires it.

Wallpaper: missing even before Metal. Inspect actual PosterBoard/Aegir extension
registration, LaunchServices/ExtensionKit and saved helper logs. AegirPoster is
currently denied the backboardd-only Metal plugin. Never simply allow all Metal
clients against a single-client kernel slot. Existing registration attempt
marker is `attempting`; an earlier registrar hung Apple-logo startup. Keep its
startup job disabled until its failure is understood, and don't blindly reset
its retry guard. Restore working static lock/home wallpaper first; then live
providers and shader effects as their dependencies work.

Wi-Fi: saved FDT identifies qcom,wcn3990-wifi at18800000; ath10k xiaomi_sweet
calibration, wlanmdsp.mbn/board files. Compare Topaz modem/PAS/GLINK/QRTR/WLFW,
transport/data path and wifid glue with Sweet's actual hardware. Require real
scan, association/authentication, traffic and reconnect; don't print passwords.

Bluetooth: saved bt_wcn3990, Qualcomm HCI/H4/in-band sleep and board regulators/
UART/pinmux/wake/firmware. Connect real controller replies to iOS HCI/BlueTool/
bluetoothd. Pairing, BLE and audio profiles have different dependencies; state
which actually work. Vince's Pronto bootstrap cannot be blindly reused.

Sound: Sweet ADSP blobs are adsp.mdt+b00–b26; board references identify two
AW88263S amplifiers. Adapt Sweet ADSP/PIL/carveouts, APR/GLINK/AFE/codec/I2S/
amp routes and the iOS CoreAudio output plugin. A daemon staying alive is not
sound output. Verify real speaker playback and volume with the user.

Haptics: Sweet uses AW8624, not HackiOS's AW8697. Resolve its I2C/reset/IRQ,
firmware/RTP format and iOS feedback path; verify actual vibration.

Brightness: derive the active AMOLED panel, DSI host/PHY/clocks, backlight mode
and brightness command format from live FDT/stock panel driver. Connect the
actual panel control to iOS slider/auto-brightness interfaces as supported.
Do not substitute a software black overlay or arbitrary register writes for
real panel brightness control. Verify the slider changes physical luminance.

Build a coordinated, versioned kernel/userspace update. Preserve rollback,
collect live logs and user-visible results, and leave a truthful status matrix:
implemented, built, installed, runtime observed, user confirmed, still broken.
