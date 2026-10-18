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

## Required evidence before phase 1

- A stock `sweet` DTB and the exact Android firmware build used on the phone.
- A hardware log with the stock bootloader's GPU clock/power state, plus the
  corresponding SM7150 Linux GPUCC/GMU/SMMU device-tree data.
- The firmware redistribution status and a recoverable test procedure using
  `fastboot boot`; no boot partition is flashed.

The next implementation change should be the read-only probe in a new XNU
patch, tested first with `sweetgpu=1`. Do not enable it by default.
