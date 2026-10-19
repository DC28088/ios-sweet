# GPU progress and architecture (2026-10-19)

This document is the resumable record of the Adreno 618 work on `sweet`
(Redmi Note 10 Pro, SM7150). It separates **verified observations** from
**implemented but unverified code** and from **plans**. Read it with
`NOTES.md` and `docs/GPU_BRINGUP.md`.

## Current truth

| Question | Answer |
| --- | --- |
| Does iOS boot and draw? | Yes. Eight CPUs, touch, framebuffer swaps and the home screen are hardware-verified. |
| Is the Adreno GPU used for any of it? | **No.** `AppleCLCD` copies IOSurface pixels to the ABL framebuffer on the CPU. |
| Does iOS see a GPU? | **No.** `backboardd` repeatedly matches `IOProviderClass=AGXAccelerator` and `IOAcceleratorES` and gets `none` / `0 found`. |
| Is there a Metal driver? | **No.** `IOGPU`, `IOAccelerator` and `AGXMetalA11` exist in the guest dyld cache, but no kernel service publishes the expected provider class. |
| Is GPU acceleration claimed? | **No.** Nothing in this repository has executed a GPU command or rendered a pixel on the Adreno. |

The software path is the fallback and must keep working through every stage:
`IOSurface` (kernel `IOCoreSurfaceRoot`) -> `backboardd` -> `AppleCLCD` ->
`0x9c000000` panel buffer. Any GPU experiment must be boot-arg gated and must
leave this path intact.

## Hardware facts recovered from the live device tree

The live FDT was captured from the phone (local, ignored `backup/`). The
following nodes and properties are observed, not inferred from a similar SoC.

| Item | Value |
| --- | --- |
| GPU | `qcom,kgsl-3d0@5000000`, chip ID `0x06010800` (Adreno 618) |
| GPU registers | `0x05000000`, size `0x40000` |
| CX debug/misc | `0x05061000`/`0x800`, `0x0509e000`/`0x1000` |
| GPUCC | `qcom,gpucc-sdmmagpie` at `0x05090000`, size `0x9000` |
| GMU | `qcom,gmu@506a000` at `0x0506a000`, size `0x31000` |
| KGSL SMMU | `qcom,smmu-v2` at `0x05040000`, size `0x10000` |
| Zap firmware | `qcom,kgsl-hyp`, `qcom,firmware-name = "a615_zap"` |
| GMU firmware | `a618_gmu.bin` |
| GPUCC supplies | `vdd_cx-supply`, `vdd_mx-supply`, `vdd_gfx-supply` |
| GPUCC GDSCs | CX GDSCR `0x106c`, GX GDSCR `0x100c`, GX BCR `0x1008` |
| GX domain control | `0x1508` (clamp/reset bits), CX HW status `0x1540` |

The GPU OPP table covers 180, 267, 355, 430, 565, 650, 750 and 780 MHz.
Qualcomm's `gpucc-sdmmagpie.c` maps those rates to GPU corners:

| Rate | Corner | Consumer level |
| --- | --- | --- |
| 180 MHz | MIN_SVS | 49 |
| 267 MHz | LOW_SVS (SVS2) | 65 |
| 355 MHz | SVS | 129 |
| 430 MHz | SVS_L1 | 193 |
| 565 MHz | NOM | 257 |
| 650 MHz | NOM_L1 | 321 |
| 750 MHz | TURBO | 385 |
| 780 MHz | TURBO_L1 | 417 |

The first conservative GPU power step votes **LOW_SVS (65)**, which is also
the level the downstream GDSC driver uses for its parent supply before
enabling a domain.

## Power topology and why the first audit looked "dead"

The live DT routes GPU power through three ARC level regulators:

- `mx.lvl` (`pm6150_s3_level`) - MX rail
- `cx.lvl` (`pm6150l_s2_level`, MX-parented) - CX rail
- `gfx.lvl` (`pm6150_s2_level`) - GPU GX rail

These are **performance-level votes**, not millivolt VRM commands. The
existing touch driver's `sweet_rpmh_vreg_enable()` writes PM6150L L11C VRM
registers (voltage/mode/enable) and cannot be reused unchanged.

The 2026-10-19 audit (`logs/gpu-audit-v2.txt`, source `d37d1ca`, CI
`39228692751`) shows the state inherited from ABL:

| Register | Value | Interpretation |
| --- | --- | --- |
| `CX_GDSCR` | `0x00222001` | software-collapse set, not powered by HLOS |
| `CX_HW_CTRL` | `0x00005ff4` | power-on status clear |
| `GX_GDSCR` | `0x00222001` | software-collapse set |
| `GX_DOMAIN` | `0x00000001` | GX clamped |
| `GX_BCR` | `0x00000000` | block reset not asserted |

All ten sampled CBCRs report `CLK_OFF`. `AHB`, `CXO` and `GX_CXO` have their
enable bit set while still reporting `CLK_OFF`, so an enable bit alone does
not prove a running clock. In this state the core and GMU must not be
accessed. ARC rails and GPUCC power/clock/reset are prerequisites, in that
order.

## Guest iOS graphics interface - what is actually known

1. `backboardd` opens `IOMobileFramebuffer`/`AppleCLCD`, creates IOSurfaces
   and swaps them. This works and is the verified software path.
2. `backboardd` probes for an accelerator service:
   `IOProviderClass=AGXAccelerator`, `IOProviderClass=IOAcceleratorES` and
   the legacy variant. The captured logs show `none` / `0 found`.
3. The root filesystem's `IOGPU.framework` and `IOAccelerator.framework`
   directories contain only `Info.plist`/signature stubs. Their binaries are
   in the split dyld shared cache:
   `/System/Library/PrivateFrameworks/IOGPU.framework/IOGPU` and
   `/System/Library/PrivateFrameworks/IOAccelerator.framework/IOAccelerator`.
4. The same cache contains `Metal.framework`, `QuartzCore.framework` and the
   iPhone X GPU bundle `AGXMetalA11.bundle/AGXMetalA11`, plus
   `AGXCompilerCore`/`AGXCompilerConnection`.
5. The cache is split into 40+ files (about 7.8 GB total, plus a 592 MB
   `.symbols` file). The symbol file was scanned read-only in place, without
   extracting the cache. It confirms the user-space chain and names the
   entry points:
   - `Metal.framework` -> `_MTLIOAccelDevice initWithAcceleratorPort:`
   - `IOGPU.framework` -> `_IOGPUDeviceCreateWithAPIProperty`,
     `_IOGPUCommandQueueCreateWithQoS`, `_IOGPUResourceGetTypeID`,
     `_IOGPUNotificationQueueCreate`
   - `IOGPUMetalDevice initWithAcceleratorPort:options:`
   - `IOGPUDevice`, `IOGPUCommandQueue` and `IOGPUResource` class/ID symbols
   - `AGXA11Family...`, `AGXMetalA11`, `AGXCompilerCore`,
     `AGXCompilerConnection-S2A8`
   The numeric `IOConnect` selectors and the AGX command-buffer layout are
   not in the symbol names; they require disassembly of the dylibs. The cache
   has **not** been disassembled, so those ABI details remain unknown.

Consequence: Metal does not talk to a hardware driver directly. It opens an
accelerator port from an IOKit family service (`IOGPUDevice`/`IOGPUCommandQueue`
/`IOGPUResource`), and the user-space AGX bundle builds AGX command buffers.
A kernel provider must implement that port and resource/queue ABI; publishing
a service named `AGXAccelerator` without those user clients is not enough.

## Architecture options (verified direction, not assumed)

| Option | Verdict |
| --- | --- |
| MoltenVK | **Wrong direction.** It translates Vulkan to Metal. iOS apps use Metal; there is no Vulkan API for it to intercept on iOS, and Apple's Metal is unavailable on Adreno. |
| Freedreno / Turnip | Linux DRM/MSM user-space drivers (Gallium and Vulkan). They contain useful Adreno command-stream, register and shader-compiler knowledge, but assume the Linux `msm`/`kgsl` UAPI and do not implement XNU's `IOGPU`/`IOAccelerator` ABI. Not drop-in. |
| Asahi / AGX | Apple GPU driver for Apple Silicon. Wrong hardware. |
| Minimal `AGXAccelerator` provider that only advertises a GPU | Will not render. Without the private user-client ABI and a working command path, `backboardd` cannot use it. |
| Kernel-side Adreno blit/copy used by `AppleCLCD` | **Smallest path that puts real Adreno work on screen without the guest Metal ABI.** It accelerates the existing compositor (copy/scale/format conversion), not app shaders. Still needs GMU, SMMU and command submission first. |
| Metal-on-Adreno for app content | Largest path. Requires the `IOGPU`/`IOAccelerator` ABI **and** a story for shaders compiled for Apple's AGX ISA. If iOS 16 app metallibs contain precompiled AGX machine code, an instruction translator or an API-level recompiler is required. This is unresolved and must not be promised. |

The practical conclusion is to separate two goals:

- **Composition acceleration** (GPU copies/scales IOSurfaces into the panel
  buffer) can be verified without any Metal ABI. It targets the measured
  bottleneck: a full 10 MB frame copy on the CPU.
- **Metal/CoreAnimation acceleration** requires the guest ABI and shader
  strategy. It comes later and only after composition proves the command
  path.

## Staged plan and pass/fail gates

Every stage is opt-in, logs a unique marker, and leaves `sweetgpu` unset by
default. A stage is complete only when its gate passes **on the phone**.

### Stage 0 - CPU baseline (complete)

- Pass: iOS boots, eight CPUs, touch, home screen, framebuffer swaps.
- Evidence: `logs/gpu-baseline.txt`, earlier boot logs.

### Stage 1 - read-only GPUCC audit (complete)

- Code: patch 0081/0082, boot arg `sweetgpu=1`.
- Pass: `SWEET GPU: GPUCC audit v2`, GDSCR/CBCR snapshot in the persistent
  log, no crash or display regression.
- Evidence: `logs/gpu-audit-v2.txt`.

### Stage 2 - ARC table probe (implemented, hardware test pending)

- Code: patch 0083, `sweetgpu=1`.
- Behavior: read `mx.lvl`, `cx.lvl`, `gfx.lvl` from the RPMH command DB;
  validate aux length, alignment, monotonic levels and zero padding; print
  every level. No TCS is claimed and no RPMH command is written.
- Pass: all three tables print, `SWEET GPU: ARC tables read-only complete`,
  iOS keeps running. If a table is missing or malformed, the stage stops
  before any vote.
- Fail: missing table, malformed aux, non-monotonic levels, or a crash.

### Stage 3 - ARC rail votes (implemented, hardware test pending)

- Code: patch 0083, `sweetgpu=2`.
- Behavior: after the read-only probe, vote `mx.lvl`, then `cx.lvl`, then
  `gfx.lvl` to the first level `>= 65` (LOW_SVS). Selector `0` (off) is
  refused. Each transaction uses a bounded 200 ms timeout and quarantines
  its TCS on an uncertain result.
- Pass: three `ACTIVE vote complete` lines, `ARC target vlvl=65 votes
  complete`, iOS keeps running, display still swaps.
- Important limit: RPMH completion proves the request was accepted, not that
  a rail reached a specific voltage. There is no RPMH readback. Do not
  describe this as measured GPU power.
- Fail: timeout, missing completion, poisoned TCS, display regression, or
  any crash. Stop and recover the log; do not proceed to GPUCC writes.

### Stage 4 - GPUCC CX power, clocks, GX release, identity read

- Prerequisite: stage 3 passes and the inherited/shared ARC votes are
  understood.
- Implement in a new patch: CX GDSC enable/poll, GPUCC branch clocks needed
  for register access, GX GDSC enable, clamp/reset release, then read
  `RBBM_IDENTIFIER`/revision from the GPU aperture **without** GMU boot or
  command submission.
- Pass: identity matches Adreno 618, no GPU fault, no display regression.
- Fail: GDSC poll timeout, bus hang, SMMU fault, panel loss.

### Stage 5 - GMU firmware boot

- Load the device-matching `a618_gmu.bin` (captured locally, ignored), boot
  the GMU, wait for its handshake, then stop.
- Pass: GMU boot status and firmware version readable; no command submitted.
- Fail: firmware mismatch, GMU timeout, secure/protected failure.

### Stage 6 - SMMU context and one scratch command

- Bring up one isolated KGSL SMMU context bank with guarded buffers below
  4 GiB, explicit cache maintenance, and timeout recovery.
- Submit a minimal fenced command stream that writes a known pattern to a
  scratch buffer, then verify the pattern on the CPU.
- Pass: fence completes, pattern matches, no translation fault.
- Fail: any fault, timeout, or corrupted memory. Stop; do not expose a user
  client.

### Stage 7 - GPU composition into the panel path

- Implement a minimal Adreno blit/copy or clear into a test IOSurface, then
  compare pixels and measure CPU time saved versus the `AppleCLCD` memcpy.
- Pass: pixel-exact or intentionally converted output, repeatable over many
  frames, lower CPU time, no visual corruption. Only then may `AppleCLCD`
  optionally use the GPU path; keep a boot-arg fallback.
- This is the first point where "GPU acceleration" may be claimed for
  composition. It is **not** Metal acceleration.

### Stage 8 - guest `IOGPU`/`IOAccelerator` service and Metal

- Gate A: recover the user-client ABI (selectors, structures, shared-memory
  and notification conventions) from the dyld cache or a compatible source.
- Gate B: resolve the shader question (precompiled AGX ISA versus something
  recompilable). No API-level translation can be promised before this is
  answered.
- Gate C: `backboardd` opens the service and presents a GPU-rendered
  IOSurface; compare against the software frame.
- Only after gate C should CoreAnimation switch renderers.

## Test procedure for the current milestone (stage 2/3)

Build and boot through CI as usual; do not flash a partition. Then:

```sh
# stage 2: read-only ARC tables, no votes
fastboot boot shim/out/shim-boot-0083-arc-probe.img

# after iOS runs >= 30 s, recover the persistent log via OrangeFox
bash tools/sweet-log-pull.sh logs/gpu-arc-probe.txt
rg 'SWEET GPU:|SWEET RPMH:.*ARC' logs/gpu-arc-probe.txt
```

Expected markers for `sweetgpu=1`:

```
SWEET RPMH: ARC mx.lvl read-only table: ...
SWEET RPMH: ARC cx.lvl read-only table: ...
SWEET RPMH: ARC gfx.lvl read-only table: ...
SWEET GPU: ARC tables read-only complete
SWEET GPU: read-only audit complete; use sweetgpu=2 to vote ARC rails
```

Then repeat with `sweetgpu=2` and require the three vote-complete lines.
If either run regresses the display or crashes, keep the previous image and
the log; do not add GPUCC writes.

## Safety and recovery

- Use temporary `fastboot boot` images only. No partition, bootloader,
  recovery, modem, persist or backup changes for GPU probes.
- Keep `AppleCLCD` and `sweetgpu` disabled by default.
- One uncertain RPMH transaction poisons the TCS for the rest of the boot;
  the code never retries or resets it.
- Never take an ARC rail to selector `0`; the code refuses it.
- Do not access the GPU core or GMU until stage 3 passes.
- Preserve the Android installation and the existing partition backups.

## Artifacts and references

- Authoritative audit log: `logs/gpu-audit-v2.txt` (ignored locally).
- Baseline log: `logs/gpu-baseline.txt`.
- Live DT extract: `research/gpu/live-gpu-tree.json`, `research/gpu/*.c`.
- Patch series: `xnu/patches/0081` (probe), `0082` (audit v2), `0083`
  (ARC probe/vote).
- Kernel call site: `bsd/dev/sweet_ufs.c` calls `sweet_gpu_probe()`.
- RPMH client: `bsd/dev/sweet_rpmh.c`; GPUCC probe: `bsd/dev/sweet_gpu.c`.
- CI: `.github/workflows/xnu.yml` rejects skipped project patches and
  requires the GPU/ARC markers in the built SWEET kernel.

## Open blockers

1. Stage 2/3 require an approved temporary boot and a recovered log.
2. The dyld cache has not been disassembled; `IOGPU`/`IOAccelerator` symbol
   names are recovered, but numeric selectors and structures are unknown.
3. The shader strategy for Metal-on-Adreno is unresolved; no translation
   layer in this repository claims to solve it.
4. Full cache extraction is blocked by local disk space; only targeted
   extraction or an external workspace should be used.
5. GMU/SMMU/firmware/command submission are unimplemented.
