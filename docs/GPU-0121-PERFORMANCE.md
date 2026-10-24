# 0121 observations and next clock profile

Observed on sweet on 2026-10-24. The installed userspace remains the 0120 Mesa
GLES library. This is not a completed Metal port.

## Startup and wallpaper

The original wallpaper registration helper blocked during its first plugin
registration after registering PaperBoard. SpringBoard samples showed
CoreServices lock waits and synchronous XPC. Disabling that job alone restored
the home screen with the same 0121 kernel. Keep it disabled.

The PNG wallpaper assets and ExtensionKit override remain installed, but the
wallpaper is still absent. PosterBoard still traps during initial configuration.
Converting images did not solve provider discovery/registration. Automatic early
registration is not a validated repair.

## User report and live trace

The user reports relatively smooth recents scrolling, good blur, very sluggish
app open/close transitions and notification shade, and similar overall speed.

The 300-second capture `live-0121-no-registrar-usb.txt` received 270,619 bytes.
It contains over 6,000 real scoped GPU submissions, eight logged read-only
residency normalizations, and no captured BackBoard crash or renderer-submit
rejection. This exercises the 0121 correction; it does not establish long-term
stability. PosterBoard's separate crash remains.

Several active `glFinish` windows average about 50–140 ms, with peaks over one
second. Kernel submission calls are commonly about 2–5 ms but exclude the prior
fence wait in userspace. These samples mix different interactions and idle time;
they are not a controlled FPS comparison. Preserving real completion and cache
ownership is required before presenting or reusing buffers.

## 0122 change

The current HFI table has just off and 180,000 kHz entries, and CP DCVS selects
entry1. The pinned [SM7150 GPU OPP table](https://github.com/sm7150-mainline/linux/blob/fd78d17954a86ea6ceb0c535e618ea129e09f8dd/arch/arm64/boot/dts/qcom/sm7150.dtsi)
allows 267 MHz at LOW_SVS (raw64), with supported-hardware mask0x1f. The live
command DB already rounds the previous MIN_SVS raw48 request up to raw64;
0121 logged the existing vote as0x400202. Its MX dependency is already selected
by the same command-DB rounding path.

Patch `0120-sweet-low-svs-clock-profile.patch` selects 267,000 kHz only if the
existing raw voltage corner is at least64; otherwise it retains180,000. It does
not add a voltage vote or expose higher corners. Firmware acceptance of that
table is not a measured clock rate or proof of a speed improvement. Full dynamic
frequency scaling, thermal control and higher performance levels remain absent.

The patch also records real fence wait and post-completion cache retirement
separately, sampled at the first eight retirements and every120 afterward. It
retains completion waits, SMMU permissions, residency, and cache maintenance.

Source781b4ed; kernel CI40109685834, with tests disabled. Local HFI compilation
and submit-core syntax compilation passed. Hardware results remain pending.
