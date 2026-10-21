# Live USB logging on sweet

The supplied USB-SHIM-GUIDE.md describes a CDC-ACM serial implementation for
MSM8953. It supplies design notes, not the referenced implementation files.
Sweet already implements a DWC3 CDC-ECM Ethernet device in patches0072–0077.
Its controller base is 0x0a600000; do not copy another board's clock/PHY addresses.

## Capture the existing kernel stream

The XNU payload command line must contain `sweetusb=2`. Logging defaults on;
`sweetusblog=0` disables it. The connected-renderer payload explicitly enables
`sweetusblog=1`. These are XNU payload arguments, not Android boot header arguments.

Start this on Fedora before the planned iOS boot:

```sh
cd /home/dan/ios-sweet
mkdir -p logs
bash tools/sweet-usbnet.sh logs/live-renderer.txt
```

The helper binds UDP5140 before configuring the link. It waits for host MAC
`02:00:5e:77:ee:01` and uses a dedicated NetworkManager profile `sweet-usb-log`
to assign192.168.7.1/24, with no default route or DNS changes. It requires
Python3, iproute and nmcli. Host sudo is unnecessary when the desktop
NetworkManager policy permits connection changes (confirmed on this Fedora
host). The phone uses192.168.7.2. `--duration SECONDS` bounds a capture. Interface configuration
may need adjustment if Fedora's firewall blocks incoming UDP5140; restrict
any exception to this USB interface. No ttyACM node is expected for CDC-ECM.

On2026-10-21, kernel0110 enumerated as1209:0001, created USB Ethernet,
answered ping, and streamed live console messages into logs/live-0110-usb.txt.
Initially the host had only IPv6 and no UDP receiver. Setting its IPv4 address
and starting the receiver enabled live output without rebooting the phone.
The dedicated profile now autoconnects for this MAC. Kernel startup bytes sent
before host setup may already have been lost; use the persistent recovery log
for that boot. This is a kernel console transport, not an iOS ADB/debug shell.

Look for the real startup sequence in the saved file:

```sh
rg 'SWEET (ADRENO|NATIVE VM|RENDER)|fault|panic' logs/live-renderer.txt
```

`SWEET RENDER: NativeAdreno ABI + Metal discovery published on live A618 context`
means the provider was registered; it does not prove a Metal frame rendered.
The stream copies the kernel console ring every100ms with a64KiB initial
backlog. UDP can lose messages, especially before the host address and ARP are
ready. It does not automatically include backboardd NSLog output, and cannot
report a panic that prevents its kernel thread from running. Preserve recovery
log collection (`tools/sweet-log-pull.sh`) for those cases.

## Applying the guide

The prepare → arm EP0 → RUN_STOP ordering, physical DMA addresses, cache
maintenance, bounded event processing and reconnect handling are useful review
points for the existing sweet transport. A CDC-ACM port would require actual
implementation sources plus sweet-specific clock/PHY handling. It should be
built separately from the rendering change. The guide's serial line-coding,
DTR and tty permission instructions apply to that alternative serial function.
