# Public iOS ports relevant to sweet

Reviewed 2026-10-24. Reported hardware results below come from the projects;
they were not independently reproduced here. Code presence, a working UI and
complete Metal API coverage are different claims.

## Most useful references

| Project | Hardware / OS | Published graphics path | Networking status and relevance |
| --- | --- | --- | --- |
| [ios-flame](https://github.com/NWylynko/ios-flame) | Pixel 4, SD855 / A640, iOS 16 | SweetMetal2 → metal2vulkan → Turnip; project reports UI/app rendering and about 20–26 fps on home swipes | WCN3990 Wi-Fi through `wland`, Settings and DHCP; Bluetooth listed unfinished. Closest published combination for our OS, GPU generation and radio. |
| [HackiOS](https://github.com/oosexgang/hackios-xnu-kernel) | Redmi Note 12 Pro Speed / redwood, A642L, iOS 16 | Published README identifies the working UI path as Mesa GLES; app Metal remains in progress | Working WCN6750/ath11k Wi-Fi; Bluetooth unfinished. Its radio and EFI/APFS architecture differ from sweet. |
| [ios-topaz](https://github.com/MakrSas/ios-topaz/tree/topaz) | Redmi Note 12 4G, A610, iOS 16 | Published branch documents CPU rendering; GPU work on the phone is reported but absent from this branch | Published modem/WLAN transport, Settings and WPA2. Useful protocol/reference code; board and radio setup differ. |
| [Chlorophyll](https://github.com/ry-diffusion/Chlorophyll) / [Bergamota](https://github.com/ry-diffusion/Bergamota) | Poco F1, A630, iOS 18 | Published AIR → NIR → ir3 and Metal API over Gallium | Useful alternative shader/runtime implementation. Bergamota's inspected README still lists GPU/networking as unfinished; do not infer complete hardware support from Chlorophyll's description. iOS 18/arm64e integration needs adaptation. |
| [ios-a52s](https://github.com/createrman-system/ios-a52s) | README identifies Galaxy A71 / SM7150 despite repository name | README explicitly describes CPU rendering and no GPU driver | No published working Wi-Fi/Bluetooth in the inspected README. Same-SoC reference, not a finished acceleration donor. |
| [Toygoon/ios-sweet](https://github.com/Toygoon/ios-sweet) | README describes POCO X3 NFC / SM7150 | CPU rendering, no GPU driver | README lists Wi-Fi/Bluetooth unfinished. |

## Pixel 4 source inspection

Pinned checkout: `745c4341af86089efb0df851fa8a27fb8face624` under the ignored
`research/ios-flame-reference` directory. Useful files:

- `phone/metal2/NativeBridgeMetalFrontend.mm`, `SweetMetal2.mm`, `bridge/`:
  iOS 16 frontend and runtime integration.
- `tools/build-turnip-ios.sh`, `tools/build-m2v-bridge.sh`: on-device compiler
  stack and Darwin KMD build configuration.
- `xnu/patches/0115-flame-gpu-v2-client-turnip.patch`: NativeAdreno ABI transport.
- `xnu/patches/0119-flame-gpu-async-present.patch`: presentation scheduling.
- `phone/wifi/`: WCN3990 radio, GLINK, QMI/WLFW, ath transport and BSD link bridge.

Sweet already exposes NativeAdreno ABI1. Required adaptation includes A618
device information, firmware, GMEM/layout properties, memory ownership and
submission residency. Our legacy submit still maintains the whole VM; a Metal
port must carry complete residency through Turnip's Darwin backend rather than
discarding the scoped path. A640 register values and firmware cannot be copied
to A618. Pixel 4's panel/presentation setup also differs.

The WCN3990 protocol code is a stronger Wi-Fi starting point than WCN6750
ath11k. Sweet still needs its own rails/clocks, SMEM layout, modem/authentication,
firmware and board data, interrupts, and kernel network interface integration.

## Bluetooth cross-check

- [ios_sagit Bluetooth documentation](https://github.com/MC-XiaoXiao/ios_sagit/blob/898998cf1b74ea103e0c16d6dc0016822d8d644b/docs/bluetooth/README.md)
  describes WCN3990 UART, IBS and firmware loading plus an Apple serial/control
  socket bridge. It explicitly says hardware was not connected/tested and
  bluetoothd's WCN3990 vendor/capability integration is incomplete. This is
  relevant code, not evidence of working scanning/pairing/audio.
- `arabaci0711-ux/ios-vince` main tree
  `5abe1899ebc59e80e3f0ecd99d9a11b3bd7070de` contains no path matching the supplied
  Bluetooth patch/hook names. The user's separate summary remains a reference;
  the matching implementation has not been located there.
- Bergamota's inspected `bt18` tree
  `15561f8bd2529d5a666b6b8851f2e194ed2adccd` likewise did not expose Bluetooth
  implementation paths in the tree search; a branch name is not a hardware result.

Other pinned README revisions: HackiOS
`d82c64a3a31cd27a0a0afbf6e213e467e44b1b4c`, topaz
`e4772911e116ab4ab6c4ac1f38e6106fefe8b483`, Chlorophyll
`a95c328c2d073eddf7ca20302ef59751d8a53b30`, Bergamota develop
`fae4e73b6b071fe0234fe84e4aa026ef78a6c3e1`.

## Wallpaper repair in progress

Sailfish's [wallpaper conversion script](https://github.com/joranv1/ios-sailfish/blob/ce637521bacc06fab5cc29cf144febee2fca9ff8/tools/wallpaper-heic2png.sh)
converts HEIC layers to PNG because its port lacks HEVC image decoding. Its root
tweaks also enable extension registration; PosterBoard needs a registered
`com.apple.posterkit.provider`.

The original local sweet root contains 48 HEIC files, no PNG twins and 36 CAML
files. `tools/prepare-sweet-wallpapers.py` prepares a separate bundle, keeps
original HEICs, validates PNG chunk checksums/alpha and records source/destination
hashes. It rewrites only image `src` attributes. Example on a mounted local root:

```sh
python3 tools/prepare-sweet-wallpapers.py --root /path/to/mounted-ios-root --output /path/to/new-wallpaper-bundle
```

Prepared private image output: `research/wallpaper-0120/png-bundle`:
48 PNG files and 18 modified CAML files, 46,983,592 bytes, 16 unique HEIC decodes.
All payload hashes pass. This bundle is **not installed**. Source hashes must be
checked against the active phone root in recovery. Also inspect ExtensionKit
feature flags, migration state and LaunchServices registration before assuming
image conversion alone restores wallpaper. No whole-root reflash is required.

### Active-root findings and registration build

OrangeFox inspection on 2026-10-24 matched all 66 wallpaper source hashes against
that bundle. Both CollectionsPoster and LegacyPoster are present on disk. The
ExtensionKit override and migration sentinel are absent. Both LaunchServices
csstores lack their identifier strings; this is supporting evidence, not a
runtime registration count. Nine PosterBoard crash reports show SIGTRAP in
`_stateLock_setupSwitcherConfigurationForFirstTime+2040` during initial setup.

`userspace/wallpaper/register.m` supplies targeted registration under mobile's
LaunchServices context. It checks SPI selectors at runtime, registers installed
PosterKit bundles, and requires the CollectionsPoster/LegacyPoster LS records
plus a nonempty ExtensionFoundation query before reporting success. It uses no
instruction-offset calls, no database rebuild, and no full DataMigration run.
A 90-second alarm bounds the job; it does not KeepAlive/restart-loop. Diagnostics
persist in mobile's Library/Logs/sweet-wallpaper.log.

The helper build passed on source `ba14dd89ea9f40e6cd76e2fb0cef15aa3647f6ad`,
[CI 40101724073](https://github.com/DC28088/ios-sweet/actions/runs/40101724073),
including strict compilation and code-signature verification. Active-root staging
is under ignored `research/wallpaper-install-0121`, with a fresh block cache.
The 71-file payload includes the 66 PNG/CAML files, helper, provider list,
ExtensionKit override and launch job in both standalone and merged launchd plists.
Existing migration skip preferences and HEIC originals are retained. Installation
and visible wallpaper restoration remain pending.

Local overlay installation completed: both HFS+ checks were clean, all 71 payload
hashes matched after a read-only remount, and the filesystem was unmounted.
The staged delta saves every original/edited byte in 88 chunks (48.5 MiB).
A fresh recovery read matched all original chunks. Phone writes and the 0121
boot remain pending explicit approval; visual restoration is unverified.

The user approved installation and temporary0121 boot. All88 userdata chunks
were freshly compared before writing and verified by readback afterward.
Fastboot accepted0121; live USB confirms its boot tag, wallpaper helper launch
and initial real GPU command completion. The helper's registration result,
wallpaper visibility and performance comparison are still unverified.

### 0121 failed to reach the home screen

The user reports a persistent Apple logo. Recovery inspection recovered the
helper log: no providers, PaperBoard app registration succeeds, then the first
plugin registration does not return before logging. SpringBoard samples show
CoreServices lock waits/synchronous XPC; BackBoard's main thread is in its run
loop, with no captured GPU submission error. Automatic registration is therefore
being disabled for an isolated boot, while PNG layers and the ExtensionKit flag
are retained. This is a regression investigation, not successful wallpaper
restoration. The helper source now defaults to query-only and requires an explicit
`--register` argument for mutation; that mode is not validated for early boot.
