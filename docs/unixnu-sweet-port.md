# UniXNU references and sweet port

Sources are pinned with per-file hashes in `unixnu-port-sources.json`.
The reference checkouts are ignored under `research/unixnu-reference/`.
MIT imports and licenses are in `third_party/unixnu/`; their kernel copies
are embedded in patch 0107, because the existing CI applies XNU patches.

## Implemented in 0107

Ported NativeAdreno's ring and AdrenoPM4 encoder. Added `adoptA6xx` to take
ownership of sweet's existing idle ring after its proven ME_INIT, signed ZAP,
secure-mode transition and memory fence. Adoption verifies register bases,
control, SQE state, WPTR, memory RPTR shadow and completed fence. It does not
rewrite ring bases, clear memory, halt SQE or reset the device.

The A618 path uses CACHE_FLUSH_TS (event 4) and CP_WHERE_AM_I with sweet's
separate RPTR-shadow and fence addresses. It preserves the current sequence
and WPTR. It refuses the A7xx framed-submission path and sequence overflow.
The adapter retains existing cache-inhibited backing, checks CP/GMU/SMMU
faults, retries dropped WPTR writes within a bounded deadline and serializes
its private kernel submission API. Timeout retains storage and disables the
adapter. It publishes no accelerator/userclient or unvalidated userspace PM4.
There is no render command producer yet. Adoption itself submits no commands.

The 0106 driver already executed ME_INIT and memory-fence PM4 on hardware.
That does not establish rendering. 0107 compilation and hardware status are
recorded separately in NOTES.md; imported code is not presumed qualified.

## Required adaptations before Metal can run

| Component | Reference requirement | Sweet work remaining |
| --- | --- | --- |
| GMU / CP | NativeAdreno targets A740/sheng, SM8550 | Keep proven A618/sweet startup; do not transplant A740 register lists |
| Memory / VM | NativeAdrenoQueryCpu4K explicitly requires 4 KiB CPU pages | Adapt BO allocation, pinning, mapping and cache attributes to 16 KiB host pages while retaining 4 KiB GPU tables |
| Submission | Current upstream ring emits A7xx BR/BV packets and EVENT_WRITE7 | A618 kernel ring port implemented; A6xx context/IB submission and validation remain |
| Driver ABI | NativeAdrenoABI, NativeAdrenoUserClient and native Darwin Turnip KMD | Implement real BOs/imports, queues, mapping, completion and resource lifetimes before publication |
| Metal frontend | NativeMetal + metal2vulkan runtime + patched Turnip + AIR compiler work | Build dependencies and port plugin discovery/private selectors for iPhone X iOS 16.7.16 |
| Deployment | UniXNU build script targets arm64-apple-ios17.0, MinimumOSVersion 17.0 | Build for iOS 16 and verify symbols/protocols; changing the deployment version alone is insufficient |
| Display | UniXNU scanout handoff / NativeMobileDisplay | Integrate with sweet's IOSurface and framebuffer ownership and synchronization |

The supplied NativeAdreno and NativeMetal repositories contain reusable
layers, but their current hardware/frontend qualification is for sheng.
No plugin is installed and the iOS root is unchanged. Do not claim acceleration
or enable a service merely by returning success from missing methods.

## Topaz WLAN reference

The pinned topaz branch includes patches through 0138, including modem host
services, WLFW, CE/WMI scan, HTT, station association and WPA2 handshake code
(0121–0130). Its README is older than those patches. Source presence alone
does not establish that sweet Wi-Fi or iOS Settings works.

Sweet's own live FDT declares `qcom,icnss` at 0x18800000, an 8 MiB register
window, WLAN MSA 1 MiB, SMMU IOVA aperture 0xa0000000/0x10000000 and IPA
window 0xb0000000/0x10000. Topaz patch0123 uses WLAN 0x0c800000,
CE +0x240000, shadow +0x32000, SMMU0x0c600000 and a reserved pool beginning
0xe0000000. These board addresses/reservations must not be copied to sweet.
Use the GLINK/QRTR/QMI/WMI/HTT protocol logic as reference after verifying
sweet's modem/PAS, SMEM/SMP2P, clocks, rails, IOMMU streams, firmware and
board calibration. No WLAN firmware was loaded and no radio was powered by
this port. Calibration and device dumps must remain excluded from Git.
