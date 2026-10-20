# Adreno 618 port: implementation and evidence

## Status

Patch 0088 starts a separate `bsd/dev/sweet_adreno.c` hardware core and an
XNU adapter in `sweet_gpu.c`. **This is not a working graphics accelerator.**
No captured boot has submitted a GPU command. Patch 0094 now implements
GMU cold startup and HFI exchange, but neither has a recorded hardware result.
There is no Metal driver, IOAcceleratorES service, Mesa userspace backend,
GPU address-space allocator or SQE startup. The command transport has no
attached CP hardware backend yet.

The previous 0087 implementation was a power experiment, not a rendering
implementation. Rebuilding it could not enable blur or GPU compositing.

## postmarketOS SM7150 port (0089, 2026-10-20)

Reference kernel: [sm7150-mainline/linux at fd78d179](https://github.com/sm7150-mainline/linux/tree/fd78d17954a86ea6ceb0c535e618ea129e09f8dd),
from the SM7150 mainlining project used by postmarketOS. The
[pmaports kernel recipe](https://github.com/sm7150-mainline/pmaports/blob/7aaf86b4b194987aeedbad7e29f3de53c8c28ebd/device/community/linux-postmarketos-qcom-sm7150/APKBUILD)
identifies that kernel repository; the recipe snapshot is older than the
selected kernel revision. Neither is an iOS graphics implementation.
The per-file hashes and immutable source paths are in
[adreno-port-sources.json](adreno-port-sources.json).

A key source correction is that [GX enable is deliberately a no-op](https://github.com/sm7150-mainline/linux/blob/fd78d17954a86ea6ceb0c535e618ea129e09f8dd/drivers/clk/qcom/gpucc-sc7180.c).
[GMU startup](https://github.com/sm7150-mainline/linux/blob/fd78d17954a86ea6ceb0c535e618ea129e09f8dd/drivers/gpu/drm/msm/adreno/a6xx_gmu.c)
controls GX. The earlier manual GX GDSC/BCR sequence must remain disabled.
The following original XNU code implements the public hardware sequence;
it does not compile Linux DRM source into XNU.

Patch 0089 adds `bsd/dev/sweet_adreno_clock.c`:

- Requires the verified CX power state and fresh MX/CX resource checks.
- Validates GPLL0's inherited fixed 600 MHz rate, supports vote-FSM and already
  running direct-control modes, and changes no PLL rate registers. Linux waits
  for ACTIVE in vote-FSM mode; direct mode requires LOCK and RUN.
- Enables the GCC DDRSS GPU AXI, GPU MEMNOC and GPU SNOC clocks, and GPUCC CXO,
  AHB and CX SNOC clocks, preserving unrelated vote/register bits.
- Configures the GMU RCG for 200 MHz from the existing 300 MHz divided GPLL0
  output, waits for the UPDATE handshake and enables CX GMU. The root remains
  explicitly held on for this development stage. Running status is checked
  before any GMU register access; failures stop without accessing later blocks.
- Reads only CX-side GMU status and the SMMU ID registers; it does not read GX,
  CP or shader registers, route SMMU streams, or enable translation.
- Can stage the captured legacy 32 KiB `a618_gmu.bin` into ITCM. The M3 must
  already be held in reset; CRC, vector bounds and every uploaded word are
  checked. The loader does not release reset or execute firmware. This is
  upload verification, not a GMU startup result.

Boot arguments are `sweetgpu=4` for the verified CX path, `sweetgpu=5` for CX
plus GMU clocks/status, and `sweetgpu=6` for clocks plus firmware staging.
Stage 6 needs `/sweet-adreno` firmware properties in the local boot ADT:

```sh
python3 tools/sweet-adreno-firmware.py BASE.adt a618_gmu.bin OUTPUT.adt --sha256 EXPECTED_SHA256
```

The tool requires the expected SHA-256, checks the exact length and Cortex-M
vector table, and exclusively creates the output. It preserves the base tree's
existing nodes. Firmware bytes are local boot inputs and are not included in
Git or the CI kernel artifacts. CRC guards upload integrity; it is not a secure
boot authentication mechanism.

`python3 tools/check-adreno-core.py` extracts the actual driver source from the
patches and exercises valid clocks, shared-bit preservation, both PLL modes,
power/PLL preconditions, all-ones reads, stuck branches/RCG/UPDATE, invalid
firmware, CRC mismatch, reset ownership and partial readback failure. These
checks validate software failure handling; hardware results are recorded
separately in NOTES.md. XNU CI also runs these checks.

CI [39347214431](https://github.com/DC28088/ios-sweet/actions/runs/39347214431)
succeeded on `b1a4b3c`, including the portable checks, full XNU build and kernel
marker checks. The verified kernel SHA-256 is
`6d0f9903c9c0f97c1ecce4c56e0d24955900af21b1669b23e5edfbd08549fda8`.
The stage-6 local image reached iOS. The retained log shows a stop at the
extra CXO_AON gate, before SMMU-ID reads or upload; see the 0090 correction
below. No flash was used.

**Full GPU acceleration remains unimplemented.** Subsequent dependencies are
XNU DMA mappings and SMMU setup, GMU/HFI startup, ZAP and SQE initialization,
submission/fault recovery, the Mesa userspace backend, and iOS graphics API
integration. A successful clock/staging run cannot establish rendered frames.

## Implemented in 0088

- Separate hardware core with an XNU MMIO adapter. CX enable uses the live-DT
  status register at GPUCC + 0x1540 and clock-disable wait value 8, orders MMIO,
  delays before polling and after memory power-up, rejects all-ones reads and
  hardware control, and bounds polling to 500 iterations. It never resets an
  already powered CX domain or retries an uncertain power transition.
- Fresh serialized MX/CX ARC resource checks before a CX write. Off, invalid,
  busy, poisoned or unavailable resource responses stop the operation. This
  still relies on this board's experimentally used selector interpretation;
  resource settings are not physical voltage measurements. No shared rail votes
  are written. Parent power must remain established throughout GPU use.
- The incomplete GX sequence is disabled. It omitted the root clock and AON
  GMEM reset handling present in the downstream sequence. GX status semantics
  must be resolved for this exact downstream revision before enabling it.
- Removed all reads from the GPU core aperture: AHB running with CX on does not
  establish that the GX/CP register banks are accessible.
- Bounded A6xx type-4 register writes, type-7 memory writes and cache-flush fence
  packet builders, using Freedreno's public wire format. Rejected appends leave
  the command buffer unchanged. Addresses must be aligned and fit 49 bits;
  actual ownership/mapping validation belongs to the future XNU allocator.
- Ring transport with wraparound, one empty slot, capacity checks, invalid RPTR
  rejection, backpressure and permanent poisoning after uncertain publication.
  Callers must serialize submissions, use separate command staging storage,
  supply a DMA synchronization/doorbell backend, and establish CP/SMMU/power
  readiness first. It is kernel-only and is not a userspace submission API.

In 0088, `sweetgpu=4` invokes the new CX controller after 60 seconds and
`sweetgpu=5` refuses writes. 0089 supersedes stage 5 as described above. Stages 1–3 keep their existing diagnostic behavior. The 120 Hz
software framebuffer scheduler remains unchanged; physical DSI timing is not
configured by it.

## Log audit, 2026-10-19

All seven local text captures were inspected, including a fresh read from the
phone in OrangeFox, `logs/gpu-reaudit-20261019.txt`. Its SHA-256 is
`daa90d47582dcd4f8decb37ea8792e667bb596c758853ee70d812ca972ee1876`, identical
to the previous `gpu-cx-power.txt`: there is no newer retained boot log.

| Capture | Observed result |
| --- | --- |
| gpu-baseline | No IOAcceleratorES provider |
| gpu-audit-v2 | CX/GX collapsed, all ten GPUCC branches report CLK_OFF |
| gpu-opencode-0083 | Stage 1 metadata only; no power write or GPU command |
| gpu-arc-probe-agent | Stage 2 ARC reads complete; no power write; no accelerator |
| gpu-gfx-raise | Stage 3 gfx selector 0 → 2 readback; no GPUCC write |
| gpu-cx-power | Stage 4 scheduled, but its start marker never appears; touch discovery fails |
| gpu-reaudit-20261019 | Same retained stage-4 boot as previous row |
| gpu-0088-a618-cx | **Stage 4 CX pass**: errno=0, control=0xf8228000, status=0x80001fe0; touch GT9896 works; no accelerator |

Every capture contains an unsuccessful IOAcceleratorES lookup. The 0088
capture is the first with a successful GPUCC CX power operation (`SWEET
ADRENO: CX result errno=0 ... waited_us=1`, status bit 31 set), but it still
contains no GPU command completion or rendering. The earlier 0087 stage-4
boot ended before its 60-second power operation, so it provides no evidence
that CX enable succeeded or failed; the touch failure precedes GPU power
writes and must not be attributed to one.

The recovery Linux log was also pulled to
`logs/recovery-dmesg-20261019.txt`. It registers `gpu_cc-sdmmagpie`, a GMU
with separate user/kernel IOMMU groups, and the GPU SMMU at 0x05040000.
The SMMU explicitly reports **non-coherent table walks**, so the future XNU
page-table backend must perform cache maintenance; a CPU store barrier alone
is insufficient. `/sys/fs/pstore` and `/proc/last_kmsg` supplied no additional
retained log. These are recovery-Linux observations, not XNU GPU initialization.

Local firmware inspection: `a630_sqe.fw` starts with a four-byte container
word that Linux skips before upload. Its following version word is 0x016ee19a
and patch word is 0x01190001, satisfying v6.12's `a630_sqe.fw` version criteria.
`a618_gmu.bin` is 32768 bytes with a Cortex-M vector/code prefix; it must not
be blindly interpreted as the newer GMU block-header format. No firmware was
uploaded to the device by this work.

## Source references and reuse boundary

Use pinned, inspectable references rather than copying register guesses from
other phone models:

- [Linux v6.12 MSM a6xx_gpu.c](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c):
  command/fence ordering, firmware and GPU initialization dependencies.
- [Linux v6.12 adreno_gpu.h](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/adreno_gpu.h):
  packet field layout and parity reference.
- [Freedreno PM4 database carried by Linux v6.12](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/adreno_pm4.xml):
  opcode/event values and packet payload format (MIT). Reference SHA-256
  `ab27047cc9cde4bc5996d6eb528fbb2dbdfdd9dc4515f75bbbada43a23809536`.
- [Freedreno copyright](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/freedreno_copyright.xml):
  Rob Clark and Ilia Mirkin; notice retained below.
- [Linux v6.12 a6xx_gmu.c](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gmu.c):
  legacy GMU firmware versus block firmware, HFI and runtime power requirements.
- Local Qualcomm downstream reference `research/gpu/gdsc-regulator.c`, revision
  `2055939b3577edbf42657871ad6bc147af037a82`, and Lineage revision
  `e03e5ba22586be70673afb5d3c3f70510524e658`: GDSC sequence, delay and reset
  behavior; live DT remains authoritative for register locations/flags.
- [Mesa Freedreno documentation](https://docs.mesa3d.org/drivers/freedreno.html):
  Freedreno supplies OpenGL and Turnip supplies Vulkan. Neither implements
  Apple's Metal/IOAccelerator interfaces.

0088 is original code implementing these public hardware interfaces. It does
not copy the GPL Linux DRM implementation into XNU or claim to be a Turnip
port. Porting Mesa requires an actual userspace/backend implementation in
addition to this kernel foundation. Firmware blobs stay in ignored local
research storage, not in this patch or the kernel.

## Remaining work in dependency order

1. CX power is verified in 0088. Verify the 0089 GMU/bus clocks and reset-held
   firmware staging on hardware, then proceed through GMU-owned GX startup.
   Never proceed after an uncertain clock/power transition.
2. Implement XNU-owned DMA buffers and GPU/GMU SMMU mappings with faults, cache
   maintenance, teardown, and explicit IOVA ownership. Never use CPU physical
   addresses as GPU addresses without a mapped address space.
3. Load the board firmware, implement the appropriate GMU/HFI protocol and
   secure ZAP initialization, initialize SQE and the ring, and attach the ring
   callbacks. The first execution milestone is a GPU-written scratch value
   followed by a completed cache-flush fence, with fault/timeout handling.
4. Implement the Mesa backend and first offscreen rendered image. Include
   shader compilation, buffer lifetime, synchronization and format handling.
5. Implement the iOS graphics interface used by the compositor. Registering
   an IOAcceleratorES name alone is insufficient. Apple AGX shader binaries and
   command buffers cannot be passed to Adreno unchanged.
6. Only then measure compositor frame times and implement display-mode/vblank
   synchronization. No iPhone 8 performance or 120 fps claim is established.

## MIT notice for the new core and Freedreno format reference

Copyright (c) 2026 ios-sweet contributors.
Copyright (c) 2013 Rob Clark and Ilia Mirkin (Freedreno register database).

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## 0089 hardware result and 0090 clock correction (2026-10-20)

The temporary stage-6 boot reached iOS. The retained log verified CX power,
fixed GCC PLL checks and three GCC bus branches. It stopped at GPUCC
`0x1004` (CXO_AON): `errno=5`, value zero, completed branches three.
No firmware upload, SMMU ID read or firmware execution occurred.

The pinned SM7150 GMU device-tree node requests GMU, CXO, AXI and MEMNOC;
CXO_AON is not a dependency. Patch 0090 removes that extra gate instead of
pretending its enable succeeded. Required CXO still needs a running status,
and a regression checks both unused AON reading zero and required CXO
remaining off. Full acceleration remains unfinished.

## 0090 hardware result and 0091 complete clock-list audit

The 0090 temporary image reached iOS. CX, three GCC branches, required
GPUCC CXO and AHB passed. Setup stopped at extra GPUCC CX_SNOC_DVM
(`0x108c`, readback zero, `errno=5`, five completed branches).
Firmware upload and GMU/SMMU status reads were not reached.

Auditing both pinned SM7150 DT nodes gives six unique clock dependencies:

| Consumer | Clocks |
| --- | --- |
| GMU | GPUCC CX_GMU, GPUCC CXO, GCC DDRSS_GPU_AXI, GCC GPU_MEMNOC_GFX |
| SMMU | GPUCC AHB, GCC GPU_MEMNOC_GFX, GCC GPU_SNOC_DVM_GFX |

0091 removes the extra GPUCC CX_SNOC_DVM access. GCC SNOC stays enabled
and verified. Both removed gates are now covered by no-access regressions,
including their actual zero-valued hardware state; required CXO failure
still aborts. This does not establish firmware startup or rendering.

Source limitation: the pinned `gpucc-sc7180.c` fallback clock array omits
`GPU_CC_AHB_CLK`, although its binding defines it and the SM7150 SMMU DT
requests it. The XNU port therefore retains the downstream sdmmagpie AHB
register at `0x1078`, verified running in the 0090 hardware log. The
mainline DT clock list provides consumer dependencies; it does not prove
the fallback driver is complete for this board.

## 0091 verified hardware / 0092 DMA and firmware-queue backend

0091 verified all six clocks and the exact SMMU profile (IDR0 `0x48016605`,
IDR1 `0x20008005`, IDR2 `0x7511`). GMU reset stayed asserted and all 32768
firmware bytes passed write/readback. This proves upload, not execution.

0092 implements these concrete prerequisites:

- `SweetAdrenoDMA.cpp`: 80 KiB wired, physically contiguous, uncached
  IOBufferMemoryDescriptor; real physical address/extent checks, 36-bit PA
  bound, permanent ownership after allocation. No cached queue aliases.
- `sweet_adreno_smmu.c`: 4 KiB stage-1 tables inside a 16 KiB host page;
  Normal NC, outer-shareable R/W data with execution disabled; only the
  64 KiB GMU shared range mapped at `0x60000000`. Table memory is unmapped.
- Measured-profile checks; inactive CB1, unused exact-match stream slot;
  hardware ATOS must match all 16 physical pages and fault on a guard before
  publishing stream 5. No context 0, stream 4, broad stream mask or bypass.
- `sweet_adreno_hfi.c`: actual v1 table and queue layout, unpadded legacy
  packets, wrap/space/bounds checks, ordered publication on uncached RAM.
  It does not claim a firmware response from host-only queue initialization.
- `sweetgpu=7` runs allocation/mapping checks after successful clock/TCM
  staging, with reset and GX isolation checked again. Logs identify exact
  register/translation failures. Deadlines and retained allocations prevent
  retry storms or freeing memory with uncertain device ownership.

Uncached tables provide visibility for the noncoherent SMMU; barriers order
stores before TTBR use. Qualcomm's advertised 16 KiB SMMU table granule is
avoided, matching its mainline implementation quirk. The source's AArch64
TCR conversion uses EPD1/AS and no AArch32 EAE bit. Original implementation
references are pinned `arm-smmu.c/h`, `arm-smmu-qcom.c`, `io-pgtable-arm.c`
and `a6xx_hfi.c/h`; source-list and host C compilation passed.

M3 execution, PDC/RSC setup, complete HFI negotiation, GPU ring execution,
fault recovery and the iOS graphics interface remain required. These DMA and
queue components alone cannot increase the frame rate.

Architecture audit: [Arm SMMUv2 specification, IHI 0062D.c](https://documentation-service.arm.com/static/5f900d34f86e16515cdc08fb),
sections 16.5.29 and 16.5.35–36, defines some control readbacks as unspecified.
0092 writes the required AFE/TRE ones but verifies only defined controls;
CB1 must have translation disabled, with other reset bits allowed to vary.
Cache-inhibited RAM is cleared with aligned volatile stores rather than an
optimized zero routine, avoiding Device-memory DC ZVA/vector assumptions.


## 0093 panic repair and 0094 firmware protocol (2026-10-20)

The new OpenCode commit 9de2ec6 repairs pmap_sync_page_attributes_phys:
ppnum_t must be widened before shifting above 4 GB. XNU's ptoa macro does
that. The supplied photo shows phystokv_range rejecting a low physical address
while DRAM begins at 0x100000000, consistent with truncation. The stage-7 log
ends after SMMU identification; only the stage-6 control records completed TCM
staging. A repaired boot has not been observed.

Patch 0094 adds a serialized, polled HFI v1 command/ACK implementation and
SM7150/A618 cold startup. Sources are the pinned a6xx_gmu.c, a6xx_hfi.c/.h,
a6xx_gmu.xml, a6xx.xml and sm7150.dtsi listed in adreno-port-sources.json.
Register IDs in XML are DWORDs; the GMU aperture starts at GPU byte 0x6a000.
RSC uses GMU+0x23000; PDC regions are 0x0b290000 and 0x0b490000.
PDC wake CX data is **2 on A618**, not the generic A6xx value 3. The third
PDC command address 0x30090 is the A618-specific Linux value; its resource name
has not been confirmed and is not asserted to be xo.lvl.

HFI accepts only complete ACK v1 packets (id 126, type 2) whose echoed
request header matches id, size and sequence. All waits have finite deadlines;
firmware faults, malformed responses and timeout poison the session. DMA and
rails stay held after failure; resetting a possibly outstanding RPMh command
is not a recovery strategy. Host interrupts remain masked because no GMU IRQ
handler exists yet. Initialization exchanges INIT, supported version 1.10,
fixed-size performance and A618 bandwidth tables, then the legacy end-init
message (wire id 5, called TEST in Linux). Only two OPP entries are supplied:
off and 180 MHz MIN_SVS for GPU, off and 200 MHz MIN_SVS for GMU. Votes use raw
ARC levels, not regulator consumer levels (+1). MIN_SVS is raw level 48 in
the pinned RPMh binding; the measured GFX/CX tables round it up to 64, and MX
to 192. No overclocking is involved.

`sweetgpu=8` opts into this execution path after successful stage-7 mappings.
This source has **not been exercised on hardware**. Parent rail/bus votes must
remain held, and the A618 bandwidth message does not replace host interconnect
programming. A successful firmware handshake would not establish GPU rendering:
protected ZAP/SQE initialization, GPU stream mappings, CP execution/fences,
fault recovery, thermal policy and native iOS graphics ABI integration still
remain. Existing PM4 encoders and ring code alone cannot make SpringBoard use
Adreno. Turnip/Freedreno implement different userspace interfaces from Apple's
Metal/IOAccelerator drivers.

Per the user's request, this iteration compiles only. No portable regression
suite, emulator, temporary boot or flash is run. Workflow run_tests is a
separate boolean input, default true; this build dispatch selects false.
