# wland 0239: the Wi-Fi + cellular boot daemon

`/var/local/bin/wland` on the phone (launchd `org.ios-sweet.wland`, `wland auto` at boot) is built from this
patch stack, not from `userspace/wifi/patches/`:

- 0001-0004: the last stack with working Wi-Fi (board rails/MSA/BDF, kernel-programmed WLAN SMMU stream,
  cellular QMI client, reentrant QRTR send).
- 0005 fast scan (active wildcard probe, 40/110 ms dwell, scan requests answered from a list under 30 s old),
  0006 fresh list (rescan every 20 s while not joined and the display is lit),
  0007 once-per-boot guard keyed on `kern.bootsessionuuid` (the old boottime-seconds key repeated across boots).
- 0008 (= userspace/wifi/patches/0005, 2026-11-09): the IPA steps for the kernel's cellular data path (xnu 0192-0194):
  record 7 (votes, SMMU, ipa_config) -> PAS 15 `ipa_fws` -> record 8 (GSI/commands/LAN RX) -> SMP2P "ipa" -> record 9.
  Inactive unless the kernel has `sweetipa=1` ("ipa: no answer from the kernel for step 7", data path off).
- 0009 (iOS 26, 2026-11-19): doorbell repeat (APCS IPC bit 12 again every <= 5 ms for 3 s after each glink send;
  `WLAND_NORING=1` off) and a SERVREG_NOTIFIER client: 20 s after the modem's notifier appears, if no WLFW, ask the
  state of `msm/modem/wlan_pd` (REGISTER_LISTENER) and request RESTART_PD once (`/var/local/servreg.off`,
  `WLAND_NO_PDRESTART`, `WLAND_SERVREG_MS`).

- 0010 (iOS 26, 2026-11-23): the kernel bridge's QRTR client ports 0x4100..0x417f (128, was 64), together with xnu 2018
  (CBQ_MAX_CLIENTS 128): the Mav20 CommCenter filled 64 clients (QMI_ERR_CLIENT_IDS_EXHAUSTED -> NAS ATCS_TIMEOUT).

Build: workflow `sweet-wifi-0239` (`.github/workflows/wifi-0239.yml`, manual dispatch). Tested 2026-11-09: Wi-Fi
list instant after the radio warms up, SIM works (binary sha256 5eb59683...).

`userspace/wifi/patches/` (0001-0018 + sensors 0021/0022) builds the ADSP speaker + sensors daemon
(`wland-0225` path, `wland adsp`), whose `auto` mode is not used for Wi-Fi.
