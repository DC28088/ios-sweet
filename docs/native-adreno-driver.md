# A native Metal driver for sweet (plan and first code)

Status: design, plus the device/PM4 foundation (`userspace/native/`). Written
2026-10-30 on branch `codex/native-driver`. This document is the map; NOTES.md
records what has actually run on the phone.

## 1. What "native" means in this repository

Today's stack is four layers:

    Metal SPI (backboardd, apps)
      -> SweetMetal2.bundle            Objective-C Metal objects (Sunfish)
      -> NativeMetalBridge             Rust, the nmb_* C ABI
      -> metal2vulkan                  Metal -> Vulkan translation
      -> Turnip                        Vulkan -> Adreno (ir3, PM4, GMEM)
      -> NativeAdreno user client      kernel: ring, VM, fences, GMEM, SMMU

A native driver keeps the top (a Metal implementation) and the bottom (the
kernel's GPU services) and replaces the two translation layers in between with
one device layer that speaks Adreno directly:

    Metal SPI
      -> Metal device (our object graph: device, queue, resources, passes)
      -> shader compiler (AIR -> NIR -> ir3 -> Adreno ISA)
      -> Adreno device (state, PM4, descriptors, GMEM, fences)
      -> NativeAdreno user client

The kernel side already exists and is unrelated to Vulkan: ABI 1 has BO
allocation, host-page import (IOSurfaces), mapping with client-chosen IOVAs,
PM4 submission (up to 16 indirect buffers), CP fences, timestamps and device
info. Every submission is wrapped by the kernel with the CP prologue and a
`CACHE_FLUSH_TS` + `WHERE_AM_I` fence, so a native client emits plain PM4.

## 2. Why this is worth doing, measured rather than assumed

* The GPU, not the CPU, caps the UI: 10 s windows of the 0171 profile show the
  GPU never idle with a fence out (CP 100%, VBIF 75-84%) and ~6.6 ms for one
  1125x2436 pass even with `TU_DEBUG=sysmem`.
* The render-pass audit (bridge 0202) found 87% of drawn pixels in
  1125x2436 Load/Store passes: ~46 full-screen passes per second, ~3 per frame.
* Cost is ~3.2 ms/Mpx, about ten times what an A618 should need. Shading is
  the dominant term (`draw_ib_gmem` 6.69 s of 6.94 s in the 0157 trace), and
  Metal's own half/fast-math semantics were being re-rounded per operation
  until bridge 0201.
* Two translation layers stand between Metal and the hardware. Metal ->
  Vulkan alone has to emulate Metal semantics with Vulkan objects; the audit
  shows it also cannot avoid full-surface Load/Store because the render area is
  the full extent.

So the lever is fewer, cheaper GPU operations per frame, and the shortest path
to changing them is to own the Metal-to-hardware layer.

## 3. Compiler: already proven on this phone's own shaders

HackiOS's AIR frontend (`hackios-sweet/tools/mesa/air`, MIT, built on Mesa
25.0.7) reads Apple's shader bitcode and produces NIR, the same IR ir3
compiles for the A618. It was built here on Linux with gcc and run against the
**phone's own** `QuartzCore.framework/default.metallib`
(sha256 `11c74cc42ce5137b73d44d427f75764f886656d4ddb374422d56b609ed2762d9`,
pulled read-only over SSH):

| | of 141 modules |
| --- | --- |
| container read, AIR parsed | 141 |
| translated to NIR (`nir_validate`) | 135 |
| compiled by ir3 to A618 machine code | 135 |
| failed | 6 |

The six failures are:

* `fixed_frag_lph_cph` and `fixed_frag_lph_cpf`: function-constant gated code
  containing a store of a pointer into memory. The frontend has no address for
  it. These are the "fixed function" colour pipelines.
* `tile_downsample_1/2/4/8`: kernels taking an `air.imageblock` argument,
  which the frontend does not model.

Everything else CoreAnimation needs is covered, including `std_vert0_lpf`,
`upsample_frag_lpf`, `downsample_4_frag_lpf`, `downsample_8_frag_lpf`,
`variable_blur_*`, the path shaders and the blend/copy shaders. All 135 that
translate also compile through ir3 to A618 machine code here (the same pass
order as `ir3_cmdline`'s AIR path, A618 gpu-id 618); for example
`downsample_4_frag_lpf` is 42 instructions / 72 dwords and
`variable_blur_frag_lpf` 382 / 608. This is compilation, not correctness: no
pixel has been compared against Apple's result yet, and binding/constant
layouts are the driver's job.

Reusing this compiler is the difference between a multi-month LLVM backend and
a build-integration job. The AIR frontend is a Mesa subdirectory and ir3 is
already in Mesa; both are produced by the same build that would produce the
Adreno Gallium driver.

## 4. Two ways to reach the hardware, and the recommendation

**Path A - own PM4 emitter.** Metal state objects write A6xx registers and PM4
packets ourselves: `CP_SET_DRAW_STATE`, `CP_DRAW_INDX_OFFSET`, descriptor
loads, GMEM binning, UBWC, blits. Maximum control, no Mesa in the picture, but
it re-derives years of freedreno work (tiling, UBWC, LRZ, GMEM resolve,
predication) and every correctness bug is ours. Realistically several months
before a correct home screen.

**Path B - Metal on Mesa's freedreno (recommended).** Keep Mesa's fd6 Gallium
driver as the Adreno device layer (state + PM4 + GMEM + UBWC, the same code the
GLES port already uses) and put a native Metal implementation above it. The
only new code is the Metal object graph and the seam to fd6; no Vulkan and no
Turnip. Shaders go AIR -> NIR -> Gallium -> ir3, inside one process.

Path B is still "native" in the sense that matters here: our own Metal
implementation, our own memory/synchronisation/IOSurface handoff, our own
shaders, talking to the Adreno. It is the architecture HackiOS's RedwoodMetal
already uses (`userland/src/redwoodmetal.m`, 6.3k lines, plus
`libredwoodair` - the same `rwa_*` AIR/Gallium API), which means the Metal side
is not speculative either; it is a port with a known shape. Path A remains
available later for hot paths (a specialized blur or composite pass) without
changing the layer above.

The plan below is Path B, with the Metal core written so the fd6 layer can be
swapped for direct PM4 later.

## 5. Stages and exit criteria

**N0 - device and PM4 foundation (this change).**
`userspace/native/`: a small C++ client for the NativeAdreno user client
(open, query, BO alloc/import/map/free, CPU map, submit, fence wait, timestamp)
and a checked PM4 builder. A probe compiled into the Metal bundle runs once in
backboardd, allocates a BO, has the GPU write a pattern into it with
`CP_MEM_WRITE`, waits on the real CP fence, reads the values back and logs them.
Exit: the probe's two words appear, fence completes, no GPU fault, the normal
stack still renders afterwards.

**N1 - AIR compiler as a library.**
Vendor the pinned AIR frontend and Mesa patches under `third_party/air`, add a
`libsweetair` target that exposes `sweet_air_compile(metallib, entry, stage,
chip_id) -> NIR/ISA + interface metadata`, and a host CLI. Exit: the 135/141
QuartzCore shaders translate and compile to ir3 disassembly on the build host;
the result matches HackiOS's own numbers on the same file.

**N2 - KMD shim and fd6 on sweet.**
A libdrm-msm-shaped shim over the NativeAdreno ABI (BO new/import/map/info,
CPU prepare/fini, submit, wait, timestamps, submitqueue), so Mesa's unmodified
`freedreno_drm.c` + fd6 run on this kernel; plus the Darwin build glue. Exit:
an offscreen 256x256 triangle rendered by fd6, read back and compared on the
phone, launched from backboardd (the only process the user client admits).

**N3 - Metal core for the compositor subset.**
`nmb_*` implementation or a new MTLDevice: buffers, IOSurface-backed textures
with the formats CA uses, render passes (load/store/clear, 1-2 attachments, no
depth first), pipeline state with the blend modes CA actually sets (the 55-entry
table HackiOS read out of QuartzCore), draws, fences, presentation. Exit:
backboardd renders the home screen through the native path with the same
correctness as today's stack.

**N4 - A/B and presentation.**
Run native and metal2vulkan bundles on the same scroll script
(`tools/` measurement used since 0159), compare presents/10 s, gate, passes and
Mpx. Exit: native is no worse in correctness and measurably cheaper, or the
reason is understood.

**N5 - The actual lever.**
Cheaper blur (fewer taps / lower resolution / a single-pass approximation),
fewer full-screen passes, UBWC or GMEM render targets where CA's linear
IOSurfaces allow it. This is where the 87% is.

## 6. Facts the design depends on

* **One connection, one process.** `AGXAccelerator::newUserClient` admits only
  a process named `backboardd`, open type `0x4147`, and holds a single
  connection slot (`xnu/patches/0110`). A native driver therefore lives inside
  backboardd, exactly like the bundle. The probe in N0 opens the slot, runs and
  closes it before Turnip opens it, which is why it cannot disturb the running
  stack.
* **The tunnel.** A sandboxed process reaches the ABI through selector `N`
  with a `{ 'NTNL', method }` header; an unrestricted one would open
  `NativeAdrenoGpuPublished`, which this kernel does not publish yet. The
  shim probes selectors 0..255 exactly as Turnip's Darwin KMD does.
* **ABI 1 is enough** for a driver: 1024 objects, 16 buffers per submit, 256 MiB
  per BO, client-chosen IOVAs inside `va_start..va_start+va_size`, imported host
  pages, write-back or non-coherent mappings, CP fences.
* **Only userland changes are live.** A bundle swap needs a backboardd restart,
  not a reboot (`tools/sweet-install-metal2.sh`). Kernel changes need a
  fastboot boot image.
* **No local clang.** Host work here can build C/C++ with gcc and Mesa's
  compiler with gcc; iOS binaries are built by the macOS CI (`sweet-metal2`,
  `macos-15`), which already has clang, bison, flex and ninja.

## 7. Risks and open questions

* **iOS 16 Metal SPI surface.** SweetMetal2 already satisfies what this
  userland asks of a device; the native core can reuse that object model or,
  like RedwoodMetal, inherit `_MTLDevice` directly. Measured on the device, not
  assumed.
* **AIR coverage gaps** (section 3). The two `fixed_frag` shaders matter for
  CoreAnimation's fixed-function colour path; the `imageblock` kernels are
  tile-downsample, which is exactly the blur we want to make cheaper. Both need
  frontend work, not a new backend.
* **Correctness of a new state layer.** Blend, half precision and IOSurface
  row layout have each cost this port a visible bug before. The A/B in N4 and
  the existing pass audit are the guard rails.
* **fd6 assumptions.** fd6 expects a DRM-like device with tiled resources,
  `FD_BO` flags, submitqueue semantics and per-fence tracking. The shim must
  reproduce them over ABI 1 without the kernel changes Turnip needed
  (`SubmitResources` is already in this ABI).
* **Concurrency.** One connection, one ring: the native driver must own the
  GPU while it is active. Coexistence with Turnip is not a design goal; the
  bundle chooses one path per process.

## 8. File map

    userspace/native/include/sweet_gpu.h     device client API (no Vulkan)
    userspace/native/src/sweet_gpu.cpp       IOKit <-> NativeAdreno ABI 1
    userspace/native/src/sweet_pm4.h         checked PM4 packet builder
    userspace/native/src/sweet_native_probe.cpp   N0 in-backboardd probe
    userspace/native/test/pm4_test.cpp       host test for the builder
    userspace/native/README.md               how to build and run each piece
    tools/build-sweet-metal2.py              compiles the probe into the bundle

Later stages add `third_party/air/` (pinned frontend + patches),
`userspace/native/kmd/` (the libdrm shim), `userspace/native/metal/` (the
device object graph) and `userspace/native/shaders/` (the AIR-to-pipe glue).
