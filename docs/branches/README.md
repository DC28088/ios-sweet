# Branch records (folded into main on 2026-11-09)

Until 2026-11-09 the work lived on 49 side branches of DC28088/ios-sweet. They were folded into `main` and then
deleted from GitHub. Nothing was lost:

- **fully in main**: every commit of the branch is in main's history.
- **N patches**: the commits that are *not* in main, as `git format-patch` files in the folder beside this README
  (`git am docs/branches/<name>/*.patch` on top of main's history, or read them as plain diffs).
- A complete git bundle of all 50 branch tips (exact history, tip hashes below) is kept off GitHub on the
  project's USB backup: `ios-sweet-backup/git-bundles/ios-sweet-all-branches-20261109.bundle`
  (`git clone <bundle>` or `git fetch <bundle> refs/remotes/myfork/<name>:<name>`).

| Branch | Tip | Date | Not in main | Result / last commit |
|---|---|---|---|---|
| `claude/audio-0009` | `297e2c167a` | 2026-11-09 | fully in main | In main: the NOTES handoff line. |
| `claude/audio-0195` | `a623aec930` | 2026-11-04 | fully in main | xnu 0195: allow wland's ADSP proxy votes (lcx.lvl, lmx.lvl) |
| `claude/audio-0197` | `03deada84c` | 2026-11-05 | [claude-audio-0197/](claude-audio-0197/) (2 patches) | xnu 0197: AW882xx speaker amp ID probe on QUP0 SE4 (sweetamp=1, read only) |
| `claude/charge-0199` | `886d0ca953` | 2026-11-05 | [claude-charge-0199/](claude-charge-0199/) (2 patches) | xnu 0199: PM6150 charger: register dump/status (sweetcharge=1), input current limit from t |
| `claude/haptic-0198` | `662c246be5` | 2026-11-05 | [claude-haptic-0198/](claude-haptic-0198/) (2 patches) | xnu 0198: haptic continuous mode prepared once, open/write trace |
| `claude/ipa-0192` | `294332dfbc` | 2026-11-04 | fully in main | IPA v4.2 cellular data path: xnu 0192 + wland 0005 |
| `claude/perf-0234` | `0c3d41b29f` | 2026-11-09 | [claude-perf-0234/](claude-perf-0234/) (1 patches) | Tested 2026-11-09: copy 8.4 -> 3.5-5.4 ms, user: small gain. |
| `claude/perf-0235` | `8de75ec1c8` | 2026-11-09 | [claude-perf-0235/](claude-perf-0235/) (1 patches) | Tested 2026-11-09: no visible change. |
| `claude/perf-0236` | `2f278e80cf` | 2026-11-09 | [claude-perf-0236/](claude-perf-0236/) (3 patches) | Tested 2026-11-09: no visible change vs 0234. |
| `claude/pm-base-0196` | `a6fc6deeff` | 2026-11-05 | [claude-pm-base-0196/](claude-pm-base-0196/) (1 patches) | xnu 0196: sweetvaudio=1 moves VirtualAudio.plugin back into the HAL folder |
| `claude/pm-merged` | `79f8745315` | 2026-11-05 | [claude-pm-merged/](claude-pm-merged/) (6 patches) | xnu 0206 + 0207: ADSP SMMU streams via /dev/pmem; DisplayPipePlaneBaseAlignment as a dicti |
| `claude/pm-merged-intel` | `79f8745315` | 2026-11-05 | [claude-pm-merged-intel/](claude-pm-merged-intel/) (6 patches) | xnu 0206 + 0207: ADSP SMMU streams via /dev/pmem; DisplayPipePlaneBaseAlignment as a dicti |
| `claude/sensors-0237` | `bf31a073e3` | 2026-11-09 | fully in main | In main (ancestor of sensors-0238): accel/gyro/ALS stream after the soc0 + registry fix. |
| `claude/sensors-0238` | `e6c3667559` | 2026-11-09 | fully in main | Base of main: 6 sensors, proximity service live; rotation needs sweetmotion 0x40, auto-brightness and Compass not yet. |
| `claude/sleep-0200` | `bfd7193ef8` | 2026-11-05 | [claude-sleep-0200/](claude-sleep-0200/) (3 patches) | xnu 0202: panel off commands in HS mode, one black pass on display off |
| `claude/sleep-0230` | `18ba975b84` | 2026-11-09 | [claude-sleep-0230/](claude-sleep-0230/) (1 patches) | Tested 2026-11-09: cold DSI/INTF restart ran, wake still black. |
| `claude/sleep-0231` | `1c5e9e5d27` | 2026-11-09 | [claude-sleep-0231/](claude-sleep-0231/) (2 patches) | Tested 2026-11-09: + soft reset/flush, wake still black. |
| `claude/speaker-0224` | `83efd1b65b` | 2026-11-09 | fully in main | In main: speaker audio (0225). |
| `claude/wifi-0232` | `58e6ba9e0b` | 2026-11-09 | [claude-wifi-0232/](claude-wifi-0232/) (1 patches) | Tested 2026-11-09: void (boot guard bug + modem ERR-FATAL); superseded by wland-0239. |
| `claude/wifi-0233` | `6b1bb0dd82` | 2026-11-09 | [claude-wifi-0233/](claude-wifi-0233/) (2 patches) | Not booted as built (same wland stack as 0232); superseded by wland-0239. |
| `claude/wifi-0239` | `7fa30a5c6c` | 2026-11-09 | [claude-wifi-0239/](claude-wifi-0239/) (3 patches) | WORKS (2026-11-09): kept in main as userspace/wifi/wland-0239 + workflow wifi-0239.yml. |
| `codex/adreno-power-audit` | `ff1313a71e` | 2026-11-03 | [codex-adreno-power-audit/](codex-adreno-power-audit/) (20 patches) | Record privacy audit scope and history cleanup [skip ci] |
| `codex/audio-speaker-0212` | `b3f26c8c42` | 2026-11-06 | [codex-audio-speaker-0212/](codex-audio-speaker-0212/) (13 patches) | xnu 0212: optional AW882xx speaker init sequence (sweetamp=2) |
| `codex/audio-tone-0014` | `eded05043e` | 2026-11-06 | [codex-audio-tone-0014/](codex-audio-tone-0014/) (1 patches) | wland 0014: AFE DTMF tone checkpoint on full wland stack |
| `codex/cellular` | `84f2a32fdf` | 2026-11-05 | fully in main | NOTES: user priorities, Safari media root cause (0207), 0206 SMMU |
| `codex/iserv-diagnostics` | `d9e3acea62` | 2026-11-05 | [codex-iserv-diagnostics/](codex-iserv-diagnostics/) (11 patches) | xnu: trace redacted ADI file and signing call metadata |
| `codex/media-0208` | `81762fb29c` | 2026-11-05 | [codex-media-0208/](codex-media-0208/) (7 patches) | xnu 0208: trace media surface mappings and memory-fault arguments |
| `codex/media-0211` | `3df0e9955a` | 2026-11-06 | [codex-media-0211/](codex-media-0211/) (12 patches) | xnu: fix iOS16 IOSurface planar row stride result offset |
| `codex/media-0212` | `89b4f2126e` | 2026-11-06 | [codex-media-0212/](codex-media-0212/) (17 patches) | native-metal: import NV12 IOSurface planes with checked shared backing |
| `codex/media-0213` | `e0574facd4` | 2026-11-06 | [codex-media-0213/](codex-media-0213/) (21 patches) | Fix combined YUV generated C string escapes |
| `codex/metal-native-a` | `e1b2f91924` | 2026-11-06 | [codex-metal-native-a/](codex-metal-native-a/) (13 patches) | notes: metal native profile a for eight-image batch |
| `codex/metal-native-b` | `4a86707cc0` | 2026-11-06 | [codex-metal-native-b/](codex-metal-native-b/) (13 patches) | notes: metal native profile b for eight-image batch |
| `codex/metal-native-c` | `b2af57173e` | 2026-11-06 | [codex-metal-native-c/](codex-metal-native-c/) (13 patches) | notes: metal native profile c for eight-image batch |
| `codex/metal-native-d` | `8fdd8cef2c` | 2026-11-06 | [codex-metal-native-d/](codex-metal-native-d/) (13 patches) | notes: metal native profile d for eight-image batch |
| `codex/native-adreno-metal` | `67448d2151` | 2026-11-03 | [codex-native-adreno-metal/](codex-native-adreno-metal/) (2 patches) | Record privacy audit scope and history cleanup [skip ci] |
| `codex/native-driver` | `679edba615` | 2026-11-03 | [codex-native-driver/](codex-native-driver/) (13 patches) | Record privacy audit scope and history cleanup [skip ci] |
| `codex/sensors-0213` | `21a8875c5c` | 2026-11-06 | [codex-sensors-0213/](codex-sensors-0213/) (13 patches) | xnu 0213: import sunfish sensor HID services |
| `codex/sensors-camera-0214` | `eb1b4201e2` | 2026-11-06 | [codex-sensors-camera-0214/](codex-sensors-camera-0214/) (14 patches) | xnu 0214: read-only sweet camera state probe on sensor image |
| `codex/speaker-0014` | `67738afb4f` | 2026-11-06 | fully in main | Match Sweet stock ADM source port and device-open token |
| `codex/speaker-0214` | `37557358df` | 2026-11-06 | fully in main | Add supervised Sweet speaker amplifier controller and bounded PCM tone path |
| `codex/speaker-0215` | `b5fa0df5d6` | 2026-11-06 | fully in main | Decode 16-bit ADM COPP ID without reserved response bits |
| `codex/speaker-bclk-0217` | `fbefa1c189` | 2026-11-06 | fully in main | Measure app-audio refill delays and DSP underflows |
| `codex/speaker-media-0216` | `4b56d3260e` | 2026-11-06 | fully in main | Allow bounded longer speaker listening test without changing amp gain |
| `codex/speaker-stream-0218` | `ac667a9d74` | 2026-11-07 | fully in main | Reduce GLINK receive syscalls and add bounded HAL level probe |
| `codex/speaker-stream-0219` | `c62f9d1600` | 2026-11-07 | fully in main | Avoid Darwin typedef collisions in PCM endpoint host harness |
| `codex/speaker-stream-0220` | `de951d6b12` | 2026-11-07 | fully in main | Distinguish deadline scheduling from delayed ordinary audio wakes |
| `codex/speaker-stream-0222` | `9415b5a8a2` | 2026-11-08 | [codex-speaker-stream-0222/](codex-speaker-stream-0222/) (2 patches) | 0223: fix tear bands: copy-speed statistic in panel rows, not presented |
| `codex/speaker-timer-0221` | `88d77238d1` | 2026-11-07 | fully in main | Record bounded coalescing-off helper build and staging |
| `codex/unixnu-ios16-sweet` | `2067f52a74` | 2026-11-03 | [codex-unixnu-ios16-sweet/](codex-unixnu-ios16-sweet/) (13 patches) | Record privacy audit scope and history cleanup [skip ci] |
