# Adreno 618 GPU bring-up

`sweet` uses the Qualcomm SM7150 Adreno 618. The GPU register window is
`0x05000000` (256 KiB), its CX memory window is `0x0509e000`, and its debug
window is `0x05061000`. The hardware also depends on the GMU, GPU clock and
power controller, Adreno SMMU context banks, and a zap shader firmware region.

This repository does not yet have an iOS GPU driver. The existing `AppleCLCD`
driver is a CPU compositor that copies IOSurface contents into the bootloader
framebuffer. It must remain the fallback until command submission is proven
safe.

## Why this is not a small kernel patch

The open XNU sources do not include Apple's `IOGPU`/`IOAccelerator` family or
the iPhone X AGX user-space driver. iOS applications use Metal/OpenGL through
those private interfaces, while Adreno needs a different command-stream,
firmware, memory-management, fault-recovery, and shader stack. A driver which
only maps the MMIO block would not accelerate CoreAnimation and could leave the
GPU wedged or the panel black.

## Bring-up order

1. Add an `Adreno618` IOKit nub from `/arm-io` and a boot-arg-gated, read-only
   probe. It must report the chip ID and GMU status without enabling clocks,
   changing registers, or loading firmware.
2. Port the SM7150 GPU power sequence from the matching Linux DRM/MSM driver:
   GPUCC clocks, CX/GX GDSCs, RPMh votes and reset handling. Verify that power
   can be toggled and recovered while `AppleCLCD` continues presenting frames.
3. Bring up the Adreno SMMU in an isolated context bank, with guarded DMA
   buffers below 4 GiB and explicit cache maintenance. Exercise only a
   scratch-buffer copy and validate faults through the persistent log.
4. Load a legally obtained, device-matching zap shader/GMU firmware and submit
   a minimal fenced command stream. Add timeout recovery before exposing any
   user client.
5. Implement the required `IOGPU`/`IOAccelerator` compatibility surface and a
   matching user-space translation layer. Only after it renders an IOSurface
   correctly should CoreAnimation switch from software rendering.

## Phase 1: opt-in GPUCC status probe

Patch `0081-sweet-adreno-readonly-gpucc.patch` adds a read-only probe for the
SM6150 GPU clock controller at `0x05090000`. It reads the GX/CX clock branch
control registers identified by the upstream SM6150 GPUCC driver. It does not
map the Adreno core aperture, change power or clocks, reset hardware, load
firmware, or submit commands. It is disabled by default; pass `sweetgpu=1` to
opt in. The output is intended to establish the ABL-inherited clock state
before touching GPU registers.

The original CI run on `8cfc234` reported success but silently skipped patch
0081 because its contexts predated the RPMh/SPMI and USB changes. The packaged
kernel contained no `SWEET GPU:` strings; the user's saved eight-core boot log
also contains no probe output. Patch 0081 now applies after patches 0001–0080.
The workflow fails if any project patch is rejected and checks the GPU version
marker in the final SWEET kernel.

Patch `0082-sweet-adreno-power-audit.patch` extends `sweetgpu=1` with the
`GPUCC audit v2` marker, CX/GX GDSCR values, CX hardware power status, GX domain
clamp/reset bits, block reset, and ten clock-branch snapshots. It only reads
within the existing GPUCC mapping. CX power status comes from `0x05091540`,
referenced by the live DT's `hw-ctrl-addr`; GX status is reported raw with bit31
labeled explicitly, because the DT also contains `qcom,poll-cfg-gdscr`. Neither
these bits nor the clock-enable bits authorize core/GMU access.

The audit still needs a hardware boot and a recovered persistent log. It does
not initialize power, submit GPU work, accelerate animations, or enable blur.

## Required evidence before powering or accessing the Adreno core

- The stock `sweet` base DTB and DTBO (or the post-overlay live FDT), plus the
  exact Android firmware build used on the phone.
- A hardware log from the opt-in GPUCC probe and the corresponding SM7150
  Linux GPUCC/GMU/SMMU device-tree data.
- The firmware redistribution status and a recoverable test procedure using
  `fastboot boot`; no boot partition is flashed.

## Next hardware step

Temporary-boot the audit image with `sweetgpu=1`, let iOS run for at least
30 seconds, then return to OrangeFox and collect the persistent log:

```sh
cd ~/ios-sweet
bash tools/sweet-log-pull.sh logs/gpu-audit-v2.txt
rg 'SWEET GPU:' logs/gpu-audit-v2.txt
```

Use those readings to establish the inherited CX/GX state before adding a
separately gated core/GMU probe or a bounded power transition. Keep `sweetgpu`
disabled by default. The existing iOS userdata image can be reused.

## Hardware references

The captured live FDT identifies chip ID `0x06010800`, GPUCC at
`0x05090000`/`0x9000`, GMU at `0x0506a000`/`0x31000`, and KGSL SMMU at
`0x05040000`/`0x10000`. It confirms CX GDSCR `0x0509106c`, GX GDSCR
`0x0509100c`, GX block reset `0x05091008`, GX domain `0x05091508`, and
CX hardware status `0x05091540`.

Primary references, with revisions pinned for reproducibility:

- [Qualcomm GPUCC register definitions](https://android.googlesource.com/kernel/msm/+/2055939b3577edbf42657871ad6bc147af037a82/drivers/clk/qcom/gpucc-sdmmagpie.c)
- [SM6150 GDSC definitions](https://github.com/LineageOS/android_kernel_xiaomi_sm6150/blob/e03e5ba22586be70673afb5d3c3f70510524e658/arch/arm64/boot/dts/qcom/sm6150-gdsc.dtsi)
- [Downstream GDSC status interpretation](https://github.com/LineageOS/android_kernel_xiaomi_sm6150/blob/e03e5ba22586be70673afb5d3c3f70510524e658/drivers/clk/qcom/gdsc-regulator.c)
- [Adreno 618 firmware selection](https://github.com/LineageOS/android_kernel_xiaomi_sm6150/blob/e03e5ba22586be70673afb5d3c3f70510524e658/drivers/gpu/msm/adreno-gpulist.h):
  `a630_sqe.fw`, `a618_gmu.bin`, and `a615_zap`. Obtain matching firmware
  from this device before implementing uploads; filenames alone are not
  proof of a compatible firmware version.
