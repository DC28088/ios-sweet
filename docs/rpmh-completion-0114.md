# RPMh completion correction for image 0114

The saved 0113 run completed an mx.lvl read and timed out on the following
cx.lvl read. Both final snapshots show an idle TCS and completed command
status; neither reports a completion IRQ. This alone cannot distinguish a
completed fast request from stale status left by the preceding request.

The old driver masked its RSC completion source, then accepted either an IRQ
or an observed busy/status transition. Polling can miss a brief transition.
Linux instead keeps ACTIVE TCS completion sources enabled and uses completion
IRQ status to identify the finished transaction. Reference inspected:
`research/gpu/qcom-qcom-rpmh-mailbox.c`, `tcs_irq_handler()` and probe's
ACTIVE-TCS mask initialization. No Linux source was copied into the patch.

Patch 0114 separates completion latching from interrupt delivery:

1. Require the selected TCS to be free under the existing controller lock.
2. Read the GIC distributor and require the RPMh SPI to be disabled/inactive.
   The saved live FDT has distributor 0x17a00000 and apps_rsc interrupts
   `<0 5 0>`: SPI 5, GIC INTID 37. This code does not write the GIC.
3. Clear only our known-free RSC latch, enable its RSC source and verify the
   latch remains clear before triggering this request.
4. Require that source's fresh latch, idle TCS and completed command status.
   Remove the transient busy-edge fallback.
5. On proven completion, mask our source while cleaning up, acknowledge only
   its latch, and restore its inherited enable state. On timeout, mask that
   source and retain quarantine; do not reset, acknowledge or retry it.

The GIC guard refuses the operation if another owner enabled this interrupt.
This is a correction based on source comparison and a race hypothesis, not
hardware proof that the CX timeout is fixed. Expected new log marker:
`SWEET RPMH: poll completion v2 GIC INTID37`.

Image 0114 retains the existing A618 renderer/Metal client, GMU/SQE path,
120 Hz software scheduler and USB logging. Mesa's GLES frontend is a separate
userspace component and is not added by this kernel correction. No new root
or Metal bundle installation is needed to inspect its hardware result.
