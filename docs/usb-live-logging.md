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

The helper waits for host MAC `02:00:5e:77:ee:01`, assigns 192.168.7.1/24
using sudo, and listens on UDP5140. It requires iproute, NetworkManager's nmcli
(optional), and netcat. The phone uses 192.168.7.2. Interface configuration
may need adjustment if Fedora's firewall blocks incoming UDP5140; restrict
any exception to this USB interface. No ttyACM node is expected for CDC-ECM.

The old project notes record working USB enumeration and ping on0076. That
does not qualify USB operation with the current GPU kernel. Nothing was booted
or connected to USB while preparing this document.

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
