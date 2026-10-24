# Bluetooth reference for Sweet

## Evidence and applicability (2026-10-24)

The user supplied `BLUETOOTH-SUMMARY.md`, describing an iOS 16.7 Bluetooth port
for vince. It reports pairing and daemon operation, with Bluetooth audio,
low energy and reboot persistence still unfinished. The supplied document is
a report, not the source patch or hook library. Those have been requested.

Sweet's saved live device tree identifies `/soc/bt_wcn3990`, compatible
`qca,wcn3990`, with separate IO/core/PA/crystal power supplies. A separate
`qcom,btfmslim_slave` node exists for the WCN3990 audio interface. This does not
establish that either path is currently operational in iOS.

Linux handles WCN3990 Bluetooth with its Qualcomm HCI UART driver, which
includes serial-device registration and the WCN3990 power data. See the
[pinned Linux driver](https://linux.googlesource.com/linux/kernel/git/torvalds/linux/+/482deed9dfa065cf3f68372dadac857541c7d504/drivers/bluetooth/hci_qca.c).
The Qualcomm protocol uses H4 with in-band sleep, as described in
[Linux Kconfig](https://github.com/torvalds/linux/blob/master/drivers/bluetooth/Kconfig).
The exact Sweet UART, pinmux, clocks and firmware selection still need to be
resolved from the board files and stock configuration.

## Potentially reusable iOS work

- The `AppleBTHci` service/user-client contract and packet framing.
- The scoped `bluetoothd` injection policy and BlueTool bootstrap hook.
- The stock daemon's vendor-command expectations and privilege requirements.

## Sweet work still required

- Establish the actual UART route, regulators, wake signals and firmware/NVM.
- Implement Qualcomm H4/in-band-sleep transport and firmware initialization.
- Connect the real HCI endpoint to the iOS service contract.
- Verify real controller replies before enabling the stock daemon.

The vince Pronto boot-shim, WCNSS control, calibration fragments and SMD
channels cannot be assumed to apply to Sweet's WCN3990. No firmware was
started and no Bluetooth memory/partition writes were performed from this
summary. GPU work continues independently.
