# Return to the direct-shim port: sailfish GLES reference

## Decision (2026-10-22)

The user switched back to the original iOS16 direct-shim approach. Active
development is `codex/adreno-power-audit`, based on the original main history.
The actual branch name is `adreno`, not `ardreno`. Keep the UniXNU UEFI work
on `codex/unixnu-ios16-sweet`; do not merge its boot architecture into this path.

Reference: https://github.com/joranv1/ios-sailfish at
`ce637521bacc06fab5cc29cf144febee2fca9ff8`. The ignored local checkout is
`research/ios-sailfish`. Selected file hashes are recorded in
`userspace/gles/reference-inputs.json`. Source was inspected, not executed.

## What the reference actually provides

The README reports Adreno530 GPU rendering through Core Animation's GLES
renderer, Mesa Gallium freedreno, an EAGL bridge and a custom kernel driver.
It reports 15–35 rendered frames/s depending on the screen, display updates up
to60Hz, and no Metal. These are author reports, not measurements on sweet.

The implementation is public, including `tools/sweetgl/sweetgl.c`, `eagl.c`,
`sweet_drm.c`, `mesa.patch`, GLI dispatch generation and kernel BO/submission
patches. This supplies a concrete alternative to the current Metal frontend.
It does not require replacing XNU or the installed HFS+ root with UEFI/APFS.

## Port boundaries found in source

| Piece | sailfish source | Required work on sweet |
| --- | --- | --- |
| Render frontend | `tools/sweetgl/sweetgl.c`, `eagl.c`, generated GLI dispatch | Build GLES for iOS16; match our framework ABI; initialize A618 before selecting it |
| Kernel transport | `sweet_drm.c` matches `SweetGPU` and calls scalar selectors 10–16 | Adapt to existing `AGXAccelerator` type 0x4147, NativeAdreno structured ABI 1; selector numbers alone are not compatible |
| GPU facts | `get_param()` hardcodes GPU ID 530 | Read actual mounted A618 chip/GMEM/VA facts; no invented frequency/timestamp |
| BOs and fences | Pixel BO-new/close/import, submit, wait and cache operations | Preserve real CPU maps, task page pinning, VA lifetime, cache maintenance and GPU completion; refuse unsupported sync operations |
| IOSurface | Pixel 0152–0162 surface wrapping, client mapping and row handling | Compare with sweet 0046/0068; pin and retain the exact allocation; validate stride and bounds; import without copying or Pixel physical addresses |
| Presentation | Pixel 0164 GPU scaling and MDP double buffering | Retain sweet's panel/display pipeline; port A6xx scaling/resolve only after rendering and fence completion work |
| Load path | Pixel 0104/0139/0163 environment injection and dyld policy | Install the matching dylib and arrange backboardd loading; a boot image alone cannot supply the GLES frontend |
| Discovery/boot delay | Pixel 0166 IOKit quiet/discovery changes | Avoid duplicating sweet 0110's existing IOAcceleratorES class; separate discovery delays from real rendering |

The reference's kernel hardware path is A5xx on MSM8996: SMD/RPM votes,
A530 PM4/PFP microcode, that device's SMMU and MDP. Sweet is A618/SM7150:
RPMh/ARC, GMU/HFI, SQE, its own SMMU/streams and display. Do not import the
Pixel's power, MMIO, memory addresses, firmware or clock values.

`eagl.c` also patches absolute unslid addresses from the author's dsc-8
QuartzCore cache: shader table 0x1d5b44a10 and switch 0x18855a1e4, plus optional
instruction hooks. Our iPhoneX root uses a different device identity. Equal
iOS version does not prove equal cache layout. Check cache identity and every
target against the installed cache before enabling those fixes. The runtime
checks in the reference themselves dereference addresses, so they do not
establish that the addresses are valid in another cache.

The reference cross-file embeds the author's Mac/SDK paths. `mesa-setup.sh`
assumes Mesa already exists and does not pin its revision. Resolve an exact
compatible Mesa revision, replace paths with SDK discovery and record compiler
inputs before claiming a reproducible sweet build.

## Existing hardware evidence that still gates this port

Saved 0112 trace: CP completes 29/29, fence 0x618c0de, followed by renderer
profile failure on optional A618 register 0x534 returning 0xdeafbead. Sweet 0113
skips that unsupported access, but its latest saved run did not reach it.

Saved 0113 trace (`logs/live-0113-usb.txt`) instead shows:

1. mx.lvl READ returns selector 3, interpreted level 321.
2. The immediately following cx.lvl READ on TCS 0 times out, errno 60.
3. Driver quarantines the TCS and refuses GPUCC writes; no GMU/SQE startup
   or live renderer service follows in this boot.

This is an observed transaction/completion failure, not proof the rail is off.
The polling code masks the RSC source and requires either fresh IRQ status or
an observed execution transition to exclude stale CMD_STATUS. Both successful
and failed log snapshots show IRQ 0, idle 1, CMD_STATUS 0x10101. A fast completed
transaction missed between polls is one hypothesis; it is not established.
Compare the pinned downstream mailbox lifecycle and obtain fresh completion
evidence rather than accepting stale status or dropping the power gate.
Keep timeout quarantine and avoid resetting/reusing an uncertain transaction.

## Implementation order and acceptance evidence

1. Correct RPMh completion handling using sweet's controller semantics, then
   recover a trace proving CX/GMU/CP/profile/provider initialization.
2. Build pinned Mesa Gallium freedreno for iOS16/A6xx, with a DRM adapter over
   the existing NativeAdreno BO/map/import/submit/wait interface. The current
   Native ABI allows 16 submitted command buffers; validate Mesa submission
   batches and all referenced resource lifetimes rather than truncating them.
3. Port EAGL/GLI dispatch and IOSurface sharing, match our actual shared cache,
   and prepare a reversible backboardd load configuration. Keep existing root
   and Metal bundle until a concrete switch is ready for review.
4. Establish actual compositor GPU submissions and fence completion. Service
   publication, successful boot, a running timer or faster startup alone are
   not evidence of accelerated UI rendering.
5. Integrate presentation and compare frame times with the same UI workload.
   The requested 120 Hz scheduler remains a separate display goal; it does not
   establish 120 rendered frames/s or a physical mode switch.

This milestone changes development direction and records the source audit.
No new driver/dylib was compiled, no tests were run, and no device operation
or root change was performed. Full GPU acceleration remains unfinished.
