# 0112: GMEM validation and USB reconnect

The saved 0111 console confirms the owned A618 context, SQE ME_INIT and real
CP fence completion (29 submitted/29 completed, value 0x618c0de). Startup
then rejects UCHE_GMEM_RANGE_MAX: write 0x17ffff, read 0x17f000. No rendering
provider is published on that boot, so the installed Metal bundle cannot open it.

The correction keeps Linux's inclusive GMEM end programming and validates all
upper 20 bits of this low address word. The lower 12 bits reading as zero are
an inference from this device's measured readback. Other register validation,
including the high address word, remains enabled. This is a startup correction;
compositor rendering and frame rate remain unverified.

USB reset also retained `configured=true`, skipping data endpoint setup on
subsequent SET_CONFIGURATION requests. The persistent log confirms configuration
messages without endpoint setup following each later bus reset. The correction
ends active data transfers using their resource IDs, drops pending software
frames, clears configuration and disables data endpoints. The permanent DMA
allocation stays retained. A failed stop or endpoint setup disables data transfer
for this boot. Network output is serialized with reconfiguration; unconfigured
RX completions are ignored. This follows the reset/EndTransfer lifecycle in
[pinned Linux DWC3 gadget.c](https://github.com/torvalds/linux/blob/fd78d17954a86ea6ceb0c535e618ea129e09f8dd/drivers/usb/dwc3/gadget.c).

After receiving a valid host frame in each configuration, the console sender
replays up to the last 1 MiB with a bounded 16-datagram budget per 100 ms. A send
error restores its cursor. The host helper sends a small UDP probe on the
dedicated phone subnet to initiate ARP. UDP is still best effort; this is a
console transport, without shell or debugger access. Reconnect and data transfer
must be confirmed on hardware.

Bounded kernel telemetry records compositor connections and ABI method results.
The next boot must show provider publication, a real backboardd connection and
successful submissions before acceleration can be claimed. The ABI is unchanged;
the installed 0110 Metal bundle remains paired. No root image rebuild is needed.

See native-adreno-0112-sources.json and NOTES.md for evidence and build status.
