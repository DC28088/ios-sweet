# A618 native buffer, VM and submission core (0108)

## Scope and status

This ports reusable UniXNU kernel resource code to sweet. It is **not a full
Metal driver or a UI lag fix**. The new memory, VM and indirect-buffer manager
are compiled into XNU; they are not attached to the active boot context and
are not exposed to applications. The existing A618 CP startup and ring owner
remain in control. No root image or plugin is changed.

## Hardware/source comparison

Pinned reference inputs:

- NativeAdreno `805c7e4bfb7451eb6923f4a14dea8c657b9be7d2` (A740).
- ArmSmmu4K `4d0d6598b1a36aeec3de01091893fa6660e49388`.
- SM7150 mainline Linux `fd78d17954a86ea6ceb0c535e618ea129e09f8dd`;
  local `research/gpu/pmos-pinned/manifest.json` records board DTS and inputs.
  Its `a6xx_gpu.c` and `adreno_pm4.xml` define the A6xx packet sequence.

Sweet's device tree supplies A618 GPU/GMU resources, clocks, power domains,
stream routing and operating points. It does not supply an iOS Metal frontend.
The driver retains sweet's already working GMU, signed ZAP and cold SQE startup.
A740 power/register programming is not transplanted.

## Ported code

- NativeAdrenoMemory owns 16 KiB CPU allocations while exposing only the logical
  4 KiB GPU extent. Allocation budgets include host padding. Pool suballocations
  consume complete host pages, so unrelated buffers cannot share a pmap cache
  attribute. Separate owned spans cover padding; GPU spans do not.
- CPU granule checks accept consistent 4 KiB or 16 KiB hardware/kernel state.
  Cache attributes use `PAGE_SHIFT` page numbers and whole `PAGE_SIZE` pages.
  GPU table leaves, physical walks and cache-maintenance subranges stay 4 KiB.
- Task imports retain their prepared descriptor and never change CPU page cache
  attributes. Physical maintenance rejects ranges outside the known DRAM aperture
  before calling `phystokv`; absence of an alias is not proof of coherence.
- NativeAdrenoVm and ArmSmmu4K supply audited mappings, stream/context observation,
  native invalidation and poisoned-resource retention. The generic context writer
  is **not** applied to the active A618 CB0 or GMU CB1.
- NativeAdrenoSubmit validates handle/range/IB alignment, retains resources and
  emits real indirect buffers with fence-based retirement. It remains kernel core.
- Framed A6xx ring submissions now use CACHE_INVALIDATE and CCU depth/color
  invalidation, with the existing CACHE_FLUSH_TS / WHERE_AM_I fence path. A
  requested context entry initializes performance SRAM under protected-mode control
  and polls its status. No A7xx BR/BV thread packets or EVENT_WRITE7 bits are used.
  This does not switch TTBRs or establish process isolation.

MIT license files accompany the imports. `native-adreno-0108-sources.json` records
both original and adapted hashes. The patch embeds the same files into XNU.

## Build evidence

Local GCC C11 compilation passed for the portable SMMU units. The ARM-only direct
MMIO source passed C syntax compilation with the AArch64 target macro; it was not
assembled for x86. The adapted ring passed G++ C++14 compilation with warnings as
errors (upstream formatting warning suppressed). No tests or device actions ran.
Full kernel CI status is recorded in NOTES.md.

## Remaining integration

1. Allocate and install a dynamic BO table arena **before** CP release; retain
   the bootstrap firmware/ring/fence mappings and preserve GMU CB1. Never clear
   or replace active tables after the existing CP handoff.
2. Bind the VM and submit core under one resource gate, actual fault handling,
   quiescence and completion rules. Qualify the A618 context image rather than
   enabling the A740 generic fault-interrupt profile without a handler.
3. Publish the real NativeAdreno userclient only after resource lifecycle and
   isolation are implemented; connect patched Darwin Turnip.
4. Build the AIR translation/compiler and NativeMetal frontend for this iOS16
   runtime. Replace iPadOS17 raw display probes and qualify private class/selector
   layouts. Finish IOSurface render-target and scanout fence ownership.

Compilation does not establish rendering or performance. Booting this kernel
alone cannot give the currently software-rendered iOS UI GPU acceleration.
