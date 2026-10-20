# GPU progress and architecture (2026-10-19)

This document is the resumable record of the Adreno 618 work on `sweet`
(Redmi Note 10 Pro, SM7150). It separates **verified observations** from
**implemented but unverified code** and from **plans**. Read it with
`NOTES.md` and `docs/GPU_BRINGUP.md`.

## Current implementation: patches 0089–0090

See [Adreno port implementation and log audit](adreno-port.md). CX power was
verified on hardware in 0088 and again in 0089. The 0089 hardware run
reached iOS, then stopped clock setup at the unnecessary CXO_AON gate
(`0x1004`, enable readback zero) after the three GCC branches passed.
Firmware upload and GMU/SMMU status reads were not reached. Patch 0090
removes this extra gate, which is absent from the SM7150 GMU DT clock list;
required CXO and GMU checks remain. Portable regression passed; 0090 has
passed CI `39349325350` and was temporarily booted; its hardware result is
pending. Patch 0089 adds the postmarketOS SM7150 GMU/bus
clock path and firmware staging while M3 reset stays asserted, with portable
failure checks. It still lacks firmware execution, SMMU mappings, rendering
submission and the iOS graphics interface. Older stage-5/GX descriptions below
are historical; GMU firmware must own GX startup.

## 120 Hz presentation experiment (patch 0085)

The user requested 120 Hz framebuffer presentation. `sweetfb_hz=120` now
selects a 120 Hz software timer and attempts presentation on every callback;
the earlier implementation treated every value >=60 as 60 Hz. Lower rates
remain selectable, with fractional pacing for values that do not divide 60.
Absolute deadlines exclude copy time from the nominal period; late callbacks
skip expired slots rather than creating a catch-up burst.

Every ten seconds the log reports `window_ms`, `callbacks`, `present_calls`
and `missed_deadlines`. `present_calls * 1000 / window_ms` measures calls to
the software presentation path, not distinct rendered frames or physical
scanout. Compare animation windows with the same workload at 30 and 120 Hz.

The captured live DT's Xiaomi K6 panel node lists dynamic modes 120 and
60 Hz, with a 120 Hz timing entry. This is panel capability/configuration
metadata, not a measurement of ABL's active scanout. This patch does not send
DSI mode commands, change GPU clocks, or supply Metal acceleration.

## 0085 hardware result: 120 Hz timer and ARC READ (2026-10-19)

The 120 Hz image was temporarily booted with
`shim/out/shim-boot-0085-arc-readback-120hz.img` and
`sweetfb_hz=120 sweetgpu=2`; the read-only pull is
`logs/gpu-arc-probe-agent.txt` (486,461 bytes, flush #432). iOS reached
SpringBoard with eight CPUs, touch and framebuffer updates; there was no
panic or display regression.

The 0084 READ path completed all three resources after its 60 s delay:

| Resource | Raw reply | Table index | Consumer level | Reading |
| --- | --- | --- | --- | --- |
| `mx.lvl` | `0x3` | 3 | 321 | high (NOM_L1 if selector) |
| `cx.lvl` | `0x5` | 5 | 257 | high (NOM if selector) |
| `gfx.lvl` | `0x0` | 0 | 1 | off/minimum or no vote |

The values are raw RPMH responses. Qualcomm's `rpmh_read()` documentation
says a read returns the current resource setting, and the values are within
the table bounds, but this board's reply encoding is not independently
proven. In particular, `gfx.lvl = 0` may be the ARC off selector or "no APPS
vote"; both mean the GPU rail is not at a usable level.

GPUCC still reported both domains collapsed and every sampled CBCR off.
`backboardd` still logged `IOAcceleratorES -> 0 found`. No GPU command,
firmware load or rendered pixel was produced.

The 120 Hz timer itself ran: about 1200 callbacks per 10 s. Across the
eight active windows, the software path made only 4-86 `present_calls` per
10 s (45-86 after the first partial window) and missed up to 374 deadlines.
This measures timer callbacks and calls into the CPU copy path, not rendered
frames or panel scanout. The physical panel mode remains whatever ABL
selected; 120 fps is not established.

## 0086 hardware result: raise-only `gfx.lvl` passed

Patch `0086-sweet-adreno-gfx-raise.patch` (commit `065acbd`) implemented the
next write gate. `sweetgpu=3` waits 60 s, re-reads `mx.lvl` and `cx.lvl`
without writing them, then calls `sweet_rpmh_arc_raise("gfx.lvl", 65)`.
That helper reads the current selector first and:

- refuses to write unless the reply is a valid table index;
- does nothing if the current consumer level is already >= 65 (LOW_SVS);
- otherwise writes the first table selector whose consumer level is >= 65
  (`gfx.lvl` selector 2, level 65 in the current table), then re-reads and
  requires the reply to equal the written selector;
- on mismatch or error, stops; there is no retry, no GPUCC access and no
  `mx.lvl`/`cx.lvl` write.

The single level write matches the upstream `qcom,rpmh-arc-regulator`
active request: `RPMH_REGULATOR_REG_ARC_PSEUDO_ENABLE` is a Linux-side
aggregation mask and is explicitly cleared from the RPMh command bits, so
the actual RPMh ARC command carries `ARC_LEVEL` (offset 0) only. The 0086
command is therefore the same active-vote shape the upstream driver would
send, not a partial register set.

CI run `39277061501` built and linked the kernel; the marker checks include
`SWEET GPU: ARC gfx raise v1 starting`. Kernel
`xnu/out-0086/kernel.development.sweet` (17,223,288 bytes, SHA-256
`f7f95c9f4ff87a0856e2b5149577cd974c1a292b2220ab524fed99503a31aa95`)
contains the new markers and no 0083 `SWEET RPMH: ARC vote` string.

The temporary image
`shim/out/shim-boot-0086-gfx-raise-120hz.img` (17,149,952 bytes, SHA-256
`536da97e93168dfb361b4ceb9bf1c576fe2066a2a99035174734c188f735d8bc`),
cmdline `... sweetfb_noconsole=1 sweetfb_hz=120 sweetgpu=3`, was booted on
2026-10-19 ~21:35 local. The read-only log pull is
`logs/gpu-gfx-raise.txt` (flush #27). The write gate passed:

| Step | Log value |
| --- | --- |
| `gfx.lvl` before | raw `0x0` (selector 0, consumer level 1) |
| write | selector 2, consumer level 65 |
| `gfx.lvl` after | raw `0x2` |
| verification | `ARC gfx.lvl raise verified selector=2 vlvl=65` |
| `mx.lvl` | raw `0x3` (level 321), unchanged |
| `cx.lvl` | raw `0x5` (level 257), unchanged |

There was no panic; SpringBoard and touch continued after the write, and
GPUCC still reported both domains collapsed. This proves the APPS ARC level
vote is stored and read back on this board. It does not prove the rail
voltage changed, does not enable GPUCC, and does not produce GPU
acceleration.

The next gate is 0087 (built and packaged; first stage-4 attempt inconclusive): power the GPUCC CX domain with
the downstream GDSC sequence, poll its status, and - only if the GPU AHB
branch reports a running clock - read a small table of RBBM/CP/VBIF
registers from the GPU aperture. `sweetgpu=5` adds the `gfx.lvl` raise and
the GX domain bring-up (BCR pulse, clamp release, GDSC poll). The earlier
plan called this read `RBBM_IDENTIFIER`; that register does not exist in the
public A6XX database (checked against the kernel's `a6xx.xml`), so the
identity reference stays the live-DT chip ID (`0x06010800`, Adreno 618) and
the aperture read is a liveness/version read (`RBBM_STATUS*`,
`VBIF_VERSION`). No GMU, SMMU or command submission in this gate.

## Current correction: patch 0084

The latest recovered boot log, `logs/gpu-opencode-0083.txt`, confirms the
0083 **read-only** probe ran successfully. GPUCC still reports CX/GX collapsed;
backboardd finds zero IOAcceleratorES providers. This build has no rendering
acceleration, so reaching the home screen does not predict a speed increase.

Observed ARC consumer-level tables (index order):

| Resource | Address | Levels |
| --- | --- | --- |
| mx.lvl | 0x30010 | 1, 17, 193, 321, 385 |
| cx.lvl | 0x30000 | 1, 17, 65, 129, 193, 257, 385 |
| gfx.lvl | 0x30050 | 1, 17, 65, 129, 193, 257, 321, 385, 417 |

**The old fixed-level write path is removed by 0084.** A DT minimum or
initial Linux regulator setting does not prove the bootloader's retained
APPS vote. Writing selector 2 could lower an existing higher vote on shared
CX/MX. Refusing selector zero does not address that problem.

With 0084, `sweetgpu=1` remains metadata/GPUCC inspection; **`sweetgpu=2`
now sends ARC READ commands**, not votes. The worker starts after 60 seconds
and requires successful touch-power setup. It uses the existing RPMh lock,
empty ACTIVE TCS selection, masked per-slot IRQ, bounded completion and
quarantine behavior. It reads raw CMD_RESP_DATA before releasing its slot.
A timeout stops the experiment; it does not reset or retry the controller.
The ARC parser also checks alignment, aux version and all padding, and uses
32-bit consumer levels to represent 65536 without overflow.

Protocol reference:
[Qualcomm's matching RPMh mailbox implementation](https://android.googlesource.com/kernel/msm/+/2055939b3577edbf42657871ad6bc147af037a82/drivers/mailbox/qcom-rpmh-mailbox.c)
defines MSGID WRITE at bit 16 and response data at offset 0x40;
[its RPMh API](https://android.googlesource.com/kernel/msm/+/2055939b3577edbf42657871ad6bc147af037a82/drivers/soc/qcom/rpmh.c)
uses one ACTIVE command for reads. These establish the transport, not the
meaning of ARC reply fields or proof of a retained APPS vote on this board.
Keep resource writes disabled until that meaning is established.

The older stage-3 instructions below describe 0083 history and must not be
used as the current procedure. The 0084/0085 `sweetgpu=2` readback gate
passed on 2026-10-19, and the 0086 raise-only `gfx.lvl` vote with post-write
read also passed on 2026-10-19 (above). The next gate is 0087: CX power,
then (with `sweetgpu=5`) the `gfx.lvl` raise and GX power, then a gated GPU
aperture register read. This remains a power-driver prerequisite, not GPU
acceleration; no clock-tree writes, GMU firmware, SMMU context or command
submission are part of it.

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

The DT includes consumer-level constraints starting at 17 (retention).
These constraints do not establish the bootloader vote or prove that a
LOW_SVS request would raise it.
`mx.lvl` also has an always-on (`_ao`) sibling; this stage votes only the
active resource.

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
  buffer) can be verified without any Metal ABI. It targets the full-frame CPU copy, whose share of frame time still needs
  measurement.
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

### Stage 2 - ARC table probe (complete)

- Code: patch 0083, `sweetgpu=1`.
- Behavior: read `mx.lvl`, `cx.lvl`, `gfx.lvl` from the RPMH command DB;
  validate aux length, alignment, monotonic levels and zero padding; print
  every level. No TCS is claimed and no RPMH command is written.
- Pass: all three tables print, `SWEET GPU: ARC tables read-only complete`,
  iOS keeps running. If a table is missing or malformed, the stage stops
  before any vote.
- Evidence: `logs/gpu-opencode-0083.txt`.
- Fail: missing table, malformed aux, non-monotonic levels, or a crash.

### Stage 2b - ARC readback (complete)

- Code: patch 0084, `sweetgpu=2`.
- Behavior: after 60 s, issue one ACTIVE READ per rail and log the raw
  response and the table-index interpretation. No resource vote is written.
- Pass: three completed reads, raw replies logged, touch/display intact.
- Evidence: `logs/gpu-arc-probe-agent.txt` (0085 image, 2026-10-19):
  `mx.lvl=0x3`, `cx.lvl=0x5`, `gfx.lvl=0x0`.
- Limit: the reply encoding is not proven; do not treat `0x0` as measured
  voltage or as proof that the bootloader vote was lost.

### Historical stage 3 - fixed ARC votes (removed by 0084)

- Code: patch 0083, `sweetgpu=2`.
- Behavior: after the read-only probe, vote `mx.lvl`, then `cx.lvl`, then
  `gfx.lvl` to the first level `>= 65` (LOW_SVS). Selector `0` (off) is
  refused. Each transaction uses a bounded 200 ms timeout and quarantines
  its TCS on an uncertain result.
- Pass: three `ACTIVE vote complete` lines, `ARC target vlvl=65 votes
  complete`, iOS keeps running, display still swaps.
- Important limit: RPMH completion proves the request was accepted, not that
  a rail reached a specific voltage. 0083 implemented no RPMH readback. Do not
  describe this as measured GPU power.
- Fail: timeout, missing completion, poisoned TCS, display regression, or
  any crash. Stop and recover the log; do not proceed to GPUCC writes.

### Stage 3 - raise-only gfx vote with readback (complete, hardware-verified)

- Why: `gfx.lvl` read back `0x0` (off/minimum or no vote), while `mx.lvl`
  and `cx.lvl` were already high. Do not rewrite the shared rails.
- Behavior: read `gfx.lvl`; if its current level is already `>= 65`, log and
  do nothing. Otherwise vote only the first selector with level `>= 65`
  (LOW_SVS), then issue a second READ and require the raw reply to match the
  written selector. A mismatch, timeout or poison condition ends the
  experiment with no retry and no GPUCC access.
- Pass: `gfx.lvl` readback changes to the expected selector, `mx.lvl` and
  `cx.lvl` are untouched, iOS keeps running and the display still swaps.
- Result (2026-10-19): passed. `gfx.lvl` went `0x0` -> `0x2` (level 65),
  `mx.lvl=0x3` and `cx.lvl=0x5` unchanged, no panic
  (`logs/gpu-gfx-raise.txt`).
- Fail: readback mismatch, timeout, display regression, or crash. Do not
  proceed to stage 4; recover the log.

### Stage 4 - GPUCC CX power (passed on hardware 2026-10-19)

- Prerequisite: stage 3 passes (done) and the inherited/shared ARC votes are
  understood.
- Patch 0087, `sweetgpu=4`: mirror the downstream `qcom,gdsc` enable for
  `gpu_cx_gdsc` (refuse HW-controlled GDSCR, clear `SW_COLLAPSE`, poll
  `GPU_CC_CX_HW_CTRL` bit 31 for 500 us). No parent-rail, PLL, RCG or branch
  clock write.
- Patch 0087, `sweetgpu=5`: first re-issue the raise-only `gfx.lvl` vote
  (LOW_SVS), then mirror `gdsc_enable` for `gpu_gx_gdsc`: pulse
  `GX_BCR` block reset for 1 us, clear the `GX_DOMAIN` clamp, clear
  `SW_COLLAPSE`, poll `GX_GDSCR` bit 31 for 500 us.
- After power, re-read the `AHB` CBCR. **Only if its clock-off bit has
  cleared** (the branch reports a running clock) the code reads ten A6XX
  registers from `0x05000000`: `RBBM_GPR0_CNTL`, `RBBM_INT_0_STATUS`,
  `RBBM_STATUS`/`1`/`2`/`3`, `CP_RB_BASE`, `CP_RB_CNTL`, `CP_HW_FAULT` and
  `VBIF_VERSION`. If the clock is still gated, the reads are skipped and
  logged; there is no blind access to a halted bus.
- The old name `RBBM_IDENTIFIER` was wrong: the public A6XX register
  database has no chip-ID register. The live-DT chip ID `0x06010800`
  (Adreno 618) remains the identity reference; this stage proves the
  domain powers and (when the AHB clock runs) the aperture responds.
- Pass: CX (and for `sweetgpu=5`, GX) status bit 31 sets, no panic, display
  and touch survive, and any aperture values are stable/plausible with
  `RBBM_STATUS3` SMMU-stalled clear and `CP_HW_FAULT` zero.
- Fail: GDSC poll timeout, `0xFFFFFFFF` aperture reads, SMMU-stalled bit,
  bus hang, panel loss or panic. Stop; do not add clock-tree writes.
- **Result 2026-10-19: CX pass.** Patch 0088 replaced the 0087 CX path with
  `research/gpu/0088-work/bsd/dev/sweet_adreno.{c,h}` and disabled the
  incomplete GX sequence and core-aperture reads. Image
  `shim/out/shim-boot-0088-a618-cx-120hz.img` (`sweetgpu=4`, 120 Hz,
  `sweetfb_noconsole=1`) booted on the phone; touch worked (GT9896,
  pid 0x39383936) and iOS reached SpringBoard. At 60 s the core logged
  `SWEET ADRENO: CX result errno=0 control=0xf8228000 status=0x80001fe0
  waited_us=1` (status bit 31 set); no panic, display and touch survived.
  Log `logs/gpu-0088-a618-cx.txt` (flush #25, SHA-256
  `d2d9582d458b68a118db580a31e085f06acf17be7fa4826d47ed3e9c33e77946`).
  `backboardd` still reports `IOAcceleratorES -> 0 found`. This proves the
  CX GDSC can be powered from XNU; it does not prove GPU clocks, GX, GMU,
  SMMU, command submission or rendering.
- History: the 0087 `sweetgpu=4` attempt (2026-10-19 ~23:07) was
  inconclusive - the 45 s touch probe read an all-zero Goodix page and
  stopped, and the phone was rebooted before the 60 s write, so the CX gate
  was not exercised (`logs/gpu-cx-power.txt`, ignored). The 0088 boot
  repeated the test with working touch and passed. `sweetgpu=5` (GX) is not
  approved and remains disabled in 0088.

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

## Test procedure for the current milestone (0088; 0087 history below)

Build and boot through CI as usual; do not flash a partition. Patch 0088 is
built and verified: CI run `39299055941` is green on `7321f15`, kernel
SHA-256 `0d1750b798bb569ed6d7071b764c84782092052c2df77579f47d59fed8066439`.
The stage-4 image is `shim/out/shim-boot-0088-a618-cx-120hz.img`, SHA-256
`9d495db439da798f2013cbc79b0d4da7a354aebf27b3f0b5735b0d2251fdedce`.
`sweetgpu=4` powers CX through the portable A618 core; it never reads the
GPU aperture, and `sweetgpu=5` (GX) is refused:

```sh
fastboot boot shim/out/shim-boot-0088-a618-cx-120hz.img

# after iOS runs >= 60 s, recover the persistent log via OrangeFox
bash tools/sweet-log-pull.sh logs/gpu-0088-a618-cx.txt
rg 'SWEET ADRENO:|SWEET GPU:|SWEET RPMH:.*ARC|SWEET FB: timing' logs/gpu-0088-a618-cx.txt
```

Expected markers for `sweetgpu=4`:

```
SWEET GPU: GPUCC audit v2 at 0x5090000; stage=4, no GMU access
SWEET GPU: ARC tables read-only complete
SWEET ADRENO: CX controller v2 scheduled in 60 s; fresh rail checks required
SWEET ADRENO: CX controller v2 starting; no GPU core access
SWEET ADRENO: CX result errno=0 control=0xf8228000 status=0x80001fe0 waited_us=1
SWEET ADRENO: CX controller finished errno=0; rendering unavailable
```

A non-zero `errno` logs `CX preconditions failed ...; no GPUCC write`. The
result above is the observed 2026-10-19 pass (`status` bit 31 set, no panic,
touch and display survived, `IOAcceleratorES -> 0 found` unchanged).

Historical 0087 markers (CX power v1, superseded by the 0088 core):

```
SWEET GPU: GPUCC CX power v1 scheduled in 60 s; GX untouched
SWEET GPU: GPUCC CX power v1 starting; GX untouched
SWEET GPU: CX power before GDSCR=0x222001 hw_ctrl=0x5ff4 clk_dis_wait=2
SWEET GPU: CX power on after ... us; GDSCR=... HW_CTRL=0x...
SWEET GPU: CBCR AHB after power @0x1078=... enable=1 clk_off=...
SWEET GPU: GPU aperture reads complete        (only if clk_off cleared)
SWEET GPU: GPUCC CX power v1 complete; CX left on, GX untouched
SWEET FB: timing ... target=120 Hz
```

If `clk_off` for AHB stays set, the log contains
`SWEET GPU: AHB clock still off; GPU aperture reads skipped`. That is not a
failure of the GDSC sequence; it tells the next patch to enable the GCC GPU
GPLL0 parent and the GPUCC AHB/CXO branches before any aperture read.

After `sweetgpu=4` passes, the same kernel boots as `sweetgpu=5` (raise
`gfx.lvl`, then CX and GX). Expected GX markers:

```
SWEET GPU: GPUCC CX+GX power v1 scheduled in 60 s; gfx.lvl then CX then GX
SWEET GPU: GPUCC CX+GX power v1 starting; gfx.lvl first
SWEET RPMH: ARC gfx.lvl raise verified selector=2 vlvl=65
SWEET GPU: GX power before GDSCR=0x222001 DOMAIN=0x1 BCR=0x0
SWEET GPU: GX power on after ... us; GDSCR=0x... DOMAIN=0x... BCR=0x...
SWEET GPU: GPUCC GX power v1 complete; domains left on, no GMU/SMMU access
```

Stop conditions: `CX power timeout`, `GX power timeout`, `... stopped
errno=...`, `0xFFFFFFFF` aperture values, or the SMMU-stalled bit set. Keep
the log, do not proceed to clock-tree writes, and do not re-run blindly.

### 0086 procedure (history; the gate passed on 2026-10-19)

The 0086 procedure below is retained for reruns.

Build and boot through CI as usual; do not flash a partition. The 0086 image
bundles the 0084 READ path, the 0085 120 Hz scheduler and the raise-only
`gfx.lvl` write:

```sh
fastboot boot shim/out/shim-boot-0086-gfx-raise-120hz.img

# after iOS runs >= 60 s, recover the persistent log via OrangeFox
bash tools/sweet-log-pull.sh logs/gpu-gfx-raise.txt
rg 'SWEET GPU:|SWEET RPMH:.*ARC|SWEET FB: timing' logs/gpu-gfx-raise.txt
```

Expected markers for `sweetgpu=3`:

```
SWEET GPU: ARC tables read-only complete
SWEET GPU: ARC gfx raise v1 scheduled in 60 s; gfx only
SWEET GPU: ARC gfx raise v1 starting; mx/cx read-only
SWEET RPMH: ... mx.lvl ... ACTIVE read complete raw=...
SWEET RPMH: ... cx.lvl ... ACTIVE read complete raw=...
SWEET RPMH: ARC gfx.lvl raise before=... table-count=... target-vlvl=65
SWEET RPMH: ARC gfx.lvl raise selector=2 vlvl=65 from=... vlvl=...
SWEET RPMH: ARC gfx.lvl raise after=0x2 expected=2
SWEET RPMH: ARC gfx.lvl raise verified selector=2 vlvl=65
SWEET GPU: ARC gfx raise v1 complete; GPUCC power/clock writes remain disabled
SWEET FB: timing ... target=120 Hz
```

If the current gfx reply is already >= 65, the log instead contains
`already at/above vlvl=...; no write`. If the reply is not a table index,
the selector is missing, or the post-write readback differs, the code prints
a refusal/mismatch and stops without GPUCC access. Keep the log and do not
add GPUCC writes after a mismatch.

The older 0085 120 Hz image and its `sweetgpu=2` READ path remain the
fallback:

```sh
fastboot boot shim/out/shim-boot-0085-arc-readback-120hz.img

# after iOS runs >= 60 s, recover the persistent log via OrangeFox
bash tools/sweet-log-pull.sh logs/gpu-arc-probe.txt
rg 'SWEET GPU:|SWEET RPMH:.*ARC|SWEET FB: timing' logs/gpu-arc-probe.txt
```

Expected markers for `sweetgpu=2`:

```
SWEET GPU: ARC tables read-only complete
SWEET GPU: ARC readback v1 scheduled in 60 s; no resource votes
SWEET GPU: ARC readback v1 starting; no resource votes
SWEET RPMH: ... ACTIVE read complete raw=...
SWEET GPU: ARC readback v1 complete; power/clock writes remain disabled
SWEET FB: timing ... target=120 Hz
```

Do not boot the 0083 `sweetgpu=2` fixed-vote image. The 0086 raise-only
`gfx.lvl` vote has been run and passed; do not re-run it without a reason.
The 0087 CX/GX power patch is implemented (CI run `39289968547` green on
`f6f8624`; both images packaged). Stage 4 was approved and attempted once;
the result is inconclusive because the SweetTouch probe failed before the
60 s CX write. The `sweetgpu=4` retry and the `sweetgpu=5` boot each need
explicit approval, and 4 comes before 5. If a
run regresses the display or crashes, keep the previous image and the log;
do not add clock-tree or GMU writes.

## Safety and recovery

- Use temporary `fastboot boot` images only. No partition, bootloader,
  recovery, modem, persist or backup changes for GPU probes.
- Keep `AppleCLCD` and `sweetgpu` disabled by default.
- One uncertain RPMH transaction poisons the TCS for the rest of the boot;
  the code never retries or resets it.
- Never take an ARC rail to selector `0`; the code refuses it.
- Do not access the GPU core or GMU until its gate is approved and run
  (`sweetgpu=4`/`5` are the only GPU-aperture stages, and they read only
  when the AHB branch reports a running clock).
- Preserve the Android installation and the existing partition backups.

## Artifacts and references

- Authoritative audit log: `logs/gpu-audit-v2.txt` (ignored locally).
- Baseline log: `logs/gpu-baseline.txt`.
- ARC table log: `logs/gpu-opencode-0083.txt`.
- ARC readback / 120 Hz log: `logs/gpu-arc-probe-agent.txt` (2026-10-19).
- Live DT extract: `research/gpu/live-gpu-tree.json`, `research/gpu/*.c`.
- Patch series: `xnu/patches/0081` (probe), `0082` (audit v2), `0083`
  (ARC table probe), `0084` (ARC readback), `0085` (120 Hz scheduler),
  `0086` (raise-only `gfx.lvl`), `0087` (CX/GX power).
- Kernel call site: `bsd/dev/sweet_ufs.c` calls `sweet_gpu_probe()`.
- RPMH client: `bsd/dev/sweet_rpmh.c`; GPUCC probe: `bsd/dev/sweet_gpu.c`.
- CI: `.github/workflows/xnu.yml` rejects skipped project patches and
  requires the GPU/ARC markers in the built SWEET kernel.

## Open blockers

1. Stages 2, 2b and 3 are complete on hardware. Stage 4 (patch 0087) is
   written and CI-pending; its `sweetgpu=4` and `sweetgpu=5` boots need
   explicit approval, 4 before 5.
2. The dyld cache has not been disassembled; `IOGPU`/`IOAccelerator` symbol
   names are recovered, but numeric selectors and structures are unknown.
3. The shader strategy for Metal-on-Adreno is unresolved; no translation
   layer in this repository claims to solve it.
4. Full cache extraction is blocked by local disk space; only targeted
   extraction or an external workspace should be used.
5. GMU/SMMU/firmware/command submission are unimplemented. If the AHB clock
   stays gated after CX power, a follow-up patch must enable the GCC GPU
   GPLL0 parent and the GPUCC AHB/CXO branches before any aperture read.
