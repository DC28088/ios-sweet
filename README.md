# ios-sweet

**Apple's iOS 16 running on a Redmi Note 10 Pro.**

This project ports Apple's open-source XNU kernel (xnu-8796, the iOS 16 generation) to the Xiaomi Redmi Note 10 Pro (`sweet`, Qualcomm SM7150 / Snapdragon 732G). On top of it runs the **unmodified iOS 16.7.16 (20H392) userland of the iPhone X**: launchd, the system daemons, backboardd, SpringBoard and the stock apps.

As of **2026-10-31** the phone boots to the **iOS home screen**. You can unlock it, open Settings, pull down Control Center and the notification center, and use the touchscreen and the hardware buttons. The UI is GPU-accelerated through a Metal → Vulkan (Turnip) bridge on the Adreno 618; a direct native Metal backend is in development.

> Experimental bring-up, not a usable phone OS. Many things are missing or stubbed (see below), and Apple's security model is mostly switched off. The iOS root lives in the `userdata` partition and replaces Android's user data. Installation and boot procedures are intentionally not published in this README.

## What works

| Area | State |
| --- | --- |
| Boot | Stock Xiaomi ABL → our shim → XNU 8796 (iOS 16 generation), then the iPhone X 16.7.16 userland |
| CPU | 6 Cortex-A55 cores via PSCI, 16K pages, GICv3 interrupts and arch timers; the 2 Cortex-A76 cores are not used yet |
| Memory | About 3.95 GiB of the upper DDR bank |
| Storage | Own UFS host driver; journaled HFS+ iOS root in `userdata`, read/write; journal replay and read-only fsck tooling |
| iOS userland | launchd, the system daemons, the dyld shared cache, backboardd and SpringBoard run from the real iPhone X root |
| Display | AppleCLCD + IOMobileFramebuffer; iPhone X 1125×2436 presented and scaled to the 1080×2400 panel; beam-raced scanout copy; 120 Hz panel timing measured |
| GUI | Lock screen, home screen, gestures, app switcher, Control Center, Notification Center, Settings |
| Touch | Goodix GT9896 over I²C (GENI), RPMh rail, HID digitizer events |
| Buttons | Power and volume up/down through the PMIC/SPMI as HID consumer keys; lock/wake works |
| Battery | PM6150 QG voltage/current, charge counting, stable percentage, plug/unplug detection and an SDAM seed; the PM6150 RTC sets the calendar |
| Wi-Fi | WCN3990: radio and firmware up, scans, joins WPA2, DHCP, web pages load |
| Bluetooth | WCN3990 UART transport and firmware download; pairs with a laptop and headphones (no headphone audio yet) |
| USB networking | SSH (dropbear, key-only) and a kernel file server over the USB Ethernet link |
| Haptics | AW8624 driver, `/dev/sweet-haptic`, chip ID and buzz commands report done; the feel test is not confirmed |
| Device identity | iPhone X (`iPhone10,3` / `D22AP`): no Home button, gesture UI |
| Activation | Native "Hactivation" path of mobileactivationd, chosen through device-tree security status; no Apple servers involved |
| GPU kernel driver | Adreno 618: rings, GMEM, fences, scoped submissions, BO pool, write-combined BOs, SMMU streams, 800 MHz, GPUCC readback and 10 s telemetry |
| GPU userspace | Metal → Vulkan (Turnip) bridge: wallpaper, UI and apps render on the GPU at roughly 12–30 fps depending on content |
| Native Metal (experimental) | Direct AIR → NIR → freedreno/ir3 backend, metallib and function-constant parsing, specialization, command batching, viewport/scissor/clear, MSM ABI1 transport and IOSurface import; offline QuartzCore corpus 451/451 |

## What does not work (yet)

- Audio output (speaker and headphones) and video playback.
- Cellular modem, Apple ID and the App Store.
- Cameras, sensors, GPS, SEP / Face ID.
- Sleep and power management.
- Wi-Fi/Bluetooth coexistence: scans can fail while Bluetooth is on.
- Haptics feel confirmation.
- Control Center glyph corruption. Some A8 mask textures use a layout Turnip refuses and are shadowed; the vibrant/memoryless attachment is a second suspect.
- Performance: the UI is GPU-bound at 12–30 fps. Full-screen 1125×2436 passes cost about 3.2 ms/Mpx; blur and compositing dominate.
- 120 Hz UI. The panel timing is 120 Hz; the UI is not.
- Boot time of about 4–5 minutes on the Apple logo.
- Security: permissive AMFI and sandbox, a stub AppleKeyStore and no data protection. Do not put personal data on the device.

## Graphics

The working stack is a kernel Adreno 618 driver plus a Metal → Vulkan (Turnip) userspace bridge. It renders the wallpaper, the SpringBoard UI and apps, with the known gaps listed above. The kernel driver owns the ring, GMEM, fences, the BO pool and the scanout copy; the userspace bridge translates Metal command buffers and shaders to Vulkan.

The native Metal backend is the current development track. It removes the Vulkan bridge and translates Metal AIR directly to NIR, then compiles with freedreno/ir3 for the A618. The compiler now handles QuartzCore's generic shaders, including the sampler pointer cells that previously failed, and the offline specialization comparison matches Apple's own pre-specialized modules for all 451 recorded pipelines. That is a compiler and resource-selection result, not a hardware or pixel result.

The latest hardware trial (artifact `e8c7631`, CI 41163489105) loaded the native plugin and submitted real draws, but the compositor stopped on a draw validation error:

```
the draw failed: fixed_frag_lph_cph reads buffer 0 and 2 buffers were bound
```

The shader declares a direct buffer that the application does not bind. Local inspection shows that QuartzCore entry 4 `fixed_frag_lph_cph` has zero live resource reads in both the generic and Apple's specialized NIR, despite its declared buffer arguments. The correction in progress validates and binds according to executable NIR instead of every AIR declaration: direct UBOs, SSBOs, textures, samplers and images with no live access are filtered, while binding numbers, NIR resource counts, argument-buffer layouts and every slot reached by a dynamic index are preserved. Genuinely required reads are not bypassed. The host comparison includes live buffer slots as well as texture/sampler selection: 451/451 generic and specialized pipelines match, with no translation failures. This is offline evidence only; the iOS build, hardware trial and pixel accuracy remain.

The active device is on the original working graphics bundle. The native frontend is inactive and remains unqualified.

## Roadmap

1. Finish and review the NIR-liveness resource filtering, build the iOS bundle, and run a reversible hardware trial only with user approval.
2. Prove the draw binding is correct, then check pixel accuracy against the working bridge before claiming a rendering result.
3. Measure the native path (per-pass GPU cost, presents, fence latency) before any performance claim.
4. Reduce full-screen pass cost: fewer and smaller blur/compositing passes, then evaluate 120 Hz and the remaining GPU clock headroom (about 825 MHz).
5. Video playback.
6. Audio (speaker/headphone routing, CoreAudio HAL) and end-to-end haptics.
7. Sleep/wake and power management.
8. Wi-Fi/Bluetooth coexistence, then modem/cellular and Apple ID.
9. Cameras and sensors.
10. Control Center glyph fix: A8 texture layout and the memoryless attachment clear.
11. Remaining native Metal API gaps: larger constants, texture views, blits/events/heaps/resolve, imageblock compute, argument-buffer edge cases and dynamic pointers.
12. Boot-time reduction.
13. Optional UEFI (HakiOS) migration. This is separate bring-up work and does not address the native compiler blocker.

## How it boots

```
ABL (stock Xiaomi bootloader, unlocked)
  -> temporary boot of our shim (nothing is written to the boot partitions)
     -> shim: own MMU setup, reads the live FDT, loads the payload from the ramdisk
        -> XNU Mach-O + generated Apple Device Tree (ADT) + boot-args
           -> XNU -> UFS driver -> HFS+ root in the userdata partition
              -> dyld + shared cache -> launchd -> daemons -> backboardd + SpringBoard
```

There is no serial port (the debug UART is wired as SPI on this board). The kernel draws its console into the framebuffer that ABL leaves at `0x9c000000`, and keeps a persistent 4 MiB log ring at the end of `userdata`. The log can be read from a recovery environment with `tools/sweet-log-pull.sh`, so you don't have to film the screen.

## Building the components

The build is split into three independent pieces: the kernel, the iOS root image, and the boot image. Device installation and boot are deliberately not covered here; the dated engineering log in [`NOTES.md`](NOTES.md) records the development device's history.

### Kernel

The kernel builds on GitHub's macOS runners with [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build). The workflow fetches xnu-8796, applies `xnu/patches/` in order, and also stages the pinned open-source libpthread, HFS and BearSSL sources it needs. Fork the repo (public repos get free macOS minutes) and run:

```sh
gh workflow run xnu.yml -R YOU/ios-sweet
gh run download RUN_ID -R YOU/ios-sweet -n xnu-kernel -D xnu/out
```

To check a patch locally before spending a CI run, `tools/xnu-cc.py` replays the compile commands of a previous CI log for single files.

### iOS root image

Download the iPhone X (iPhone10,3) iOS 16.7.16 (20H392) IPSW from Apple. Then, on Linux, with `apfs-fuse`, `hfsprogs` and sudo.

The APFS reader must support LZBITMAP compression. Older packages based on the 2020 source mount the images but report I/O errors when reading compressed files. On Fedora, build a local reader with:

```sh
sudo dnf install cmake gcc-c++ fuse3-devel zlib-ng-compat-devel bzip2-devel
bash tools/build-apfs-reader.sh
export APFS_FUSE="$PWD/research/apfs-fuse-current/build/apfs-fuse"
```

`mkiosroot.sh` checks that compressed plists can be read before creating its output.

```sh
# OS, SystemOS and AppOS disk images from the IPSW (see BuildManifest.plist for their names)
tools/mkiosroot.sh OS.dmg SystemOS.dmg AppOS.dmg ios16-root.img 16G
sudo fsck.hfsplus -fy ios16-root.img
sudo mount -t hfsplus -o loop,force ios16-root.img /mnt/iosroot
sudo python3 tools/sweet-root-tweaks.py /mnt/iosroot
sudo umount /mnt/iosroot
img2simg ios16-root.img ios16-root.simg
```

`mkiosroot.sh` copies the OS volume and both cryptexes into one case-sensitive journaled HFS+ volume. It adds the dyld cache links, the plain system keybag and the log directories the daemons expect. `sweet-root-tweaks.py` applies the bring-up changes: it skips Setup, seeds the activation state, removes the baseband daemon, disables the watchdogs that kill slow apps and the crash-looping media paths, and so on. The script's header explains each change.

The device tree builder copies `/product` from the real iPhone X device tree, which MobileGestalt needs. Extract it once, without downloading the whole IPSW:

```sh
python3 tools/remotezip.py IPSW_URL DeviceTree.d22ap.im4p research/ipsw-x/dt
ipsw img4 im4p extract research/ipsw-x/dt/Firmware/all_flash/DeviceTree.d22ap.im4p -o research/ipsw-x/d22ap.adt
```

### Boot image

Needs `aarch64-linux-gnu-gcc` and `mkbootimg`. The shim needs the stock DTB; unpack your phone's stock `boot.img` with `tools/unpack_bootv2.py`.

```sh
python3 tools/mkadt.py shim/out/sweet.adt --highmem
python3 tools/mkpayload.py xnu/out/kernel.development.sweet shim/out/sweet.adt shim/out/payload.bin \
    --cmdline 'debug=0xc serial=3 cpus=6 keepsyms=1 -enable_kprintf_spam -noprogress rd=ufsroot0 unentitled_ios_sim_launch=1 ignition_level=1 wdt_test=no_user_checkin ufsrw=1 rootrw=1 launchd_unsecure_cache=1 wdt=-1 sweetfb_noconsole=1 sweetfb_hz=30'
shim/build.sh ../shim/out/payload.bin      # -> shim/out/shim-boot.img
```

What the less obvious boot-args do:

| Boot-arg | Effect |
| --- | --- |
| `rd=ufsroot0 ufsrw=1 rootrw=1` | Root on the `userdata` partition, writable |
| `cpus=6` | Start the six A55 cores; `cpus=1` is the safe fallback |
| `launchd_unsecure_cache=1` | launchd accepts its job cache without AMFI |
| `wdt=-1 wdt_test=no_user_checkin` | No hardware watchdog |
| `sweetfb_noconsole=1` | Stop drawing kernel text over the GUI. It still goes to the persistent log; panics are drawn |
| `sweetfb_hz=30` | Panel refresh rate of the CPU copy |
| `sweettouch=0`, `sweetkeys=0` | Disable the touch or button driver |

## QEMU

`tools/qemu-sweet.py` boots the same kernel on QEMU `virt` with a test RAM-disk root. It is useful for kernel-only changes; see `NOTES.md`.

## Repository layout

| Path | What |
| --- | --- |
| `shim/` | Bare-metal boot shim (C + asm) packed as an Android boot.img v2 |
| `xnu/patches/` | The XNU patch series (applied in order by CI), plus staging scripts for libpthread, HFS and BearSSL |
| `userspace/` | iOS userspace pieces: Metal2 bridge, audio HAL, Wi-Fi/Bluetooth helpers, SSH and tools |
| `userspace/native-metal/` | Direct AIR/NIR/freedreno native Metal backend, frontend and host tests |
| `.github/workflows/xnu.yml` | Kernel CI build |
| `.github/workflows/native-metal.yml` | Native Metal backend/frontend CI build |
| `tools/mkadt.py` | Generates the Apple Device Tree from sweet's MMIO layout and the iPhone X identity |
| `tools/mkpayload.py` | Packs kernel + ADT + boot-args into the shim payload |
| `tools/mkiosroot.sh` | Builds the HFS+ iOS root from the IPSW images |
| `tools/sweet-root-tweaks.py` | Bring-up changes to the root (Setup skip, activation, daemons) |
| `tools/sweet-log-pull.sh`, `sweet-log.py` | Read the persistent kernel log from recovery |
| `tools/sweet-crash.py` | Symbolicate console crash reports |
| `tools/xnu-cc.py` | Local single-file compile check from a CI log |
| `tools/qemu-sweet.py` | QEMU test boots with framebuffer text decoding |
| `tools/adt.py`, `remotezip.py`, `unpack_bootv2.py`, `sparsediff.py` | Helpers |
| `NOTES.md` | Detailed engineering log: every boot, crash and decision, plus hardware facts |

Not in git: partition backups, live device trees, recovery images, Apple firmware and root images, and downloaded or generated build trees. They contain device serials and calibration data, or Apple's copyrighted files.

## Kernel patches

| # | What |
| --- | --- |
| 0001–0009 | Toolchain compatibility, SWEET machine config, non-PAC arm64, stubs for Apple-internal pieces |
| 0010–0018 | Early framebuffer console, physmap fix, PRNG without corecrypto, device-tree platform expert, timer PPI, boot CPU driver |
| 0019–0020 | Apple open-source libpthread kernel provider; BearSSL AES |
| 0021–0027 | RAM-disk root, mockfs, SHA-256, minimal AMFI, Apple open-source HFS+, MD5 |
| 0028–0034 | UFS host driver and root device, read/write root, DMA above 4 GiB |
| 0035–0045 | Entitlements, bindfs, AppleKeyStore stub, IOWatchdog, console crash reports |
| 0046 | IOSurface (IOCoreSurfaceRoot) and IOMobileFramebuffer (AppleCLCD) services |
| 0047–0055 | Diagnostics: persistent log, /dev/null tee, IOKit trace, heap strings and user stack sampler on crash; permissive Sandbox policy; HFS rename-swap; AppleKeyStore selectors |
| 0056–0057 | Throttled panel copy; no external identity resolver |
| 0058 | HID event services. Apple's IOHIDFamily-1915.140.3 user client and event queue, matching the iOS 16.7 userland |
| 0059 | iPhone X identity (`hw.product` / `hw.target` from the device tree) |
| 0060 | Power and volume keys over SPMI; touch over GENI I²C |
| 0061 | Present the display as 1125×2436 |
| 0062 | RPMh regulator vote for the touch rail; battery power source |
| 0063 | GENI FIFO packing fix and the Goodix GT9896 protocol |
| 0064 | Grant PRIV_WORK_INTERVAL (UIKit's CoreAnimation work interval) |
| 0065 | SMP: secondary A55 cores via PSCI CPU_ON |
| 0066–0067 | Fault-region crash info; console mute |
| 0068 | IOSurface lookup semantics for already-mapped surfaces |
| 0069 | Batched persistent-log flushes |
| 0070–0080 | USB device/ECM/live log, 8-core SMP, display power, network trace |
| 0081–0106 | Adreno 618 bring-up: GPUCC, GMU, SMMU, CP/SQE, ZAP and startup diagnostics |
| 0107–0137 | Native Adreno ring/VM/submit, GMEM, macrotile, Metal2 plugin, compositor, clocks, write-combined BOs |
| 0138–0155 | Wi-Fi (PMEM, WLAN SMMU, WCN3990), Bluetooth UART, beam-raced copy, BO pool |
| 0156–0167 | Battery charge counting, AW8624 haptics, display scaling, PMIC RTC, A76 preference |
| 0168–0173 | USB file server, DDR vote, GPU profiling experiments, native Metal plugin selection |

## Key hardware facts

- RAM: payloads use the upper bank, `0x100000000` + `0xfcc00000` (about 3.95 GiB), above Qualcomm's protected carve-outs.
- GICv3 with security extensions: the timer goes through Group 1 IRQ (virtual timer INTID 19), not FIQ as on Apple silicon.
- PSCI through `smc`, 19.2 MHz timer, ARMv8.2 without pointer authentication. MPIDR uses Aff1 for the core number.
- Panel: 1080×2400 DSC video mode, continuous splash from ABL, no refresh kick needed.
- GPU: Adreno 618 (A618), 800 MHz, GMEM, own ring/fence/submit driver.
- Touch: Goodix GT9896 at I²C `0x5d` on QUP SE7 (`0xa84000`), powered by PM6150L LDO11 through RPMh.
- Buttons: power and volume down on the PM6150 PON block, volume up on PM6150L GPIO2 (SPMI SID 4).

More in [`NOTES.md`](NOTES.md).

## Credits

Built on Apple's open-source releases (xnu, IOHIDFamily, libpthread, hfs), [BearSSL](https://bearssl.org), [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build), [ipsw](https://github.com/blacktop/ipsw), Qualcomm/Xiaomi's published Linux kernel sources for the hardware details, and [OrangeFox Recovery](https://orangefox.download). iOS itself is Apple's and is not distributed here.
