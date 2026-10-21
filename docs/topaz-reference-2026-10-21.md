# Topaz reference refresh, 2026-10-21

Pinned the current `topaz` branch at
`89451b811a4e6cc5cdcf734c45887163d92c599e` after fetching it again. The clean local
reference checkout is detached at this exact revision. Topaz is
SM6225/Adreno610, while sweet is SM7150/Adreno618.

## Public implementation and reported hardware results

The [current README](https://github.com/MakrSas/ios-topaz/blob/89451b811a4e6cc5cdcf734c45887163d92c599e/README.md)
reports WPA2 association, saved passwords, reconnection and HTTPS browsing.
The public 0113–0130 patches implement modem/PAS boot, GLINK/QRTR, firmware
host services, WLFW, copy engines, HTC/WMI/HTT and station/WPA2 operations.
These are useful protocol references; these hardware results are the author's
reports, not results measured on sweet.

The README also reports GPU-rendered lock/home/Settings screens using private
HackiOS patches and Mesa freedreno behind CoreAnimation's OpenGL ES renderer.
Those accelerator patches are explicitly excluded until their source is public.
Metal is a later roadmap item. The current public branch still renders with CPU.
There is no public complete Topaz GPU/Metal driver here to transplant.

## Useful graphics work

Public patches0131–0139 implement IOSurface values/bulk attachments, wrapping
client memory and release handling. 0139 publishes client-address surfaces so
that their IDs do not all remain zero. These operations should be compared
against sweet before integrating a renderer; ownership and map lifetime matter.
Patch0138 only publishes an empty IOAcceleratorES to avoid a thirty-second
lookup timeout. It supplies no GPU rendering and must not replace sweet's
provider, which is tied to an actual VM, ring, transport and Metal plugin.

## Sweet Wi-Fi mapping

Sweet's saved FDT identifies ICNSS at0x18800000, an8MiB window; WLAN IOVA
0xa0000000/0x10000000, IPA0xb0000000/0x10000, stream576(0x240), mask1,
MSA reserved region0x97d00000/0x180000 (advertised WLAN MSA1MiB). Topaz0123
uses WLAN0x0c800000, CE+0x240000, SMMU0x0c600000, stream0x1a0 and a different
reserved pool. Its hard-coded maps must be replaced from sweet's tree and
firmware contract. Modem/PAS ID, firmware partition/layout, SMEM/SMP2P, power
votes, SMMU routing, CE/shadow layout and per-device calibration require a sweet
implementation before enabling radio DMA. No Wi-Fi hardware is initialized by
this reference refresh; no firmware/calibration/device dumps are committed.

## Immediate sweet GPU correction

0112's live USB log shows the real CP fence completing and the GMEM validation
passing, then RBBM_NC_MODE_CNTL0x534 returning0xdeafbead. The pinned downstream
SM6150 a6xx_start accesses this register only when macrotiling-channels==8.
Sweet's FDT has no eight-channel override; its SM7150 profile uses macrotiling
mode0. 0113 leaves this optional register at reset instead of writing/reading it.
All remaining required readbacks also explicitly reject0xdeafbead so that small
bit masks cannot accidentally accept an inaccessible register. This remains a
startup correction, with iOS rendering unverified. Source identity is recorded
in native-adreno-0113-sources.json. No ABI or installed-root change is needed.

## Concrete IOSurface comparison

Reconstructed both complete SweetSurface.cpp files from their patch series in
`/tmp/surface-audit-{sweet,topaz}` (temporary inspection files; no source tests).
Sweet's current code is only0046 plus0068. It allocates a new buffer on create,
ignores IOSurfaceAddress, has no wrapping methods6/7 and treats value/bulk calls
as successful no-ops. Topaz adds real methods6/7,9/10/11,27; state for
accelerator binding12, purgeability20 and matrix5; and separate detach/release
view handling. These are concrete missing APIs, although their effect on the
installed NativeMetal path still requires a compositor trace. Port the logical
operations with retained descriptors and16KiB range accounting; do not copy
Topaz0092/0093's display-specific contiguous-pool addresses or scanout hooks.
The existing renderer's original BO/task-descriptor ownership must be retained.

The public WLAN radio API has scan/heard/PMK/join/transmit/ready hooks. A search
of this pinned public tree did not locate an IO80211/AppleBCMWLAN compatibility
class or a consumer of the ready hook. Therefore the README's complete Settings
integration cannot be independently established from the public radio patches
alone. Account for the iOS-facing interface as well as the hardware port; do not
promise that transplanting the radio API produces working sweet Settings Wi-Fi.
