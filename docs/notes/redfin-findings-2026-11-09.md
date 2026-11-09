# What ios-redfin (Pixel 5) has that applies to sweet (2026-11-09)

Source: https://github.com/DenialOfGravity/ios-redfin (MIT), cloned read-only to `research/redfin-20261109/` (git-ignored).
Branches read: `ws-n-ipa-gsi1` (cellular data, newest, 2026-11-09), `ws-n-celldata` (Settings switch), `ws-n-wds`,
`ws-n-ipa-hw`, `ws-n-ipa-qmi`, `ws-q-mictest`, `known-good-rk5a-kernel` (kernel patch notes 0900-1061).
Pixel 5 = SM7250 (lito): IPA **v4.7** / GSI v2.5. Sweet = SM7150: IPA **v4.2** (DT `qcom,ipa-hw-ver 16`, same as upstream
SC7180) / GSI v2.2. Register layouts differ (mainline `ipa_reg-v4.2.c` vs `-v4.7.c`, `gsi_reg.c` vs `gsi_reg-v4.5.c`).

## Mobile data (redfin: works since 2026-11-09 02:00; "KNOWN-GOOD" 10:05, user-confirmed)

Design: everything in userland `wland` (`phone/wifi/cell.c`, `ipa.c`, `ipagsi.c`, `ipaqmi.c`, `ipaprobe.c`, `ipnet.c`),
over `/dev/pmem`; iOS gets a plain `utun` published to configd by `netsvc` (not CommCenter's `pdp_ip`). A dylib
(`SweetCT`, `phone/fps/SweetCT.m`) makes Settings > Cellular Data's switch write `/var/tmp/sweetcell.celldata`.
Enable words live in `/var/local/cell.on`.

Bring-up order that works (each step was a separate failure on their phone):
1. IPA clock vote (BCM IP0), PAS 15 load of `ipa_fws` (GSI firmware), IPA SMMU streams 0x520/0x522 identity,
   IPA memory init (mem_part canaries), SMP2P "ipa" out-entry = 3 (valid + clock on).
2. AP IPA QMI service 0x31 inst 1 announced; answer INDICATION_REGISTER; **INIT_DRIVER** with the mem_part offsets.
3. **Before INIT_DRIVER** (`p2early=1`): HDR_INIT_LOCAL, IP_V4/V6_ROUTING_INIT, IP_V4/V6_FILTER_INIT as immediate
   commands. Their command channel 5 never fetched; the commands went through the TX channel 2 (`p2cmdch2`).
   Filter bitmap for endpoints 2/5/8 + zero-rule slots before INIT_DRIVER (`ipatbl=1`, mainline ipa_table_setup).
4. **FILTER_INSTALLED_NOTIF** after the modem's INSTALL_FILTER_RULE (`ipanotif=1`, as Android): without it the modem
   never moved its DL channels (WDS rxok 0).
5. Data port: DPM Open Port 0x20, WDA Set Data Format 0x20 (raw IP, QMAP), WDS Bind Mux Data Port 0xa2 (mux 1),
   WDS Start Network 0x20, Get Current Settings 0x2d -> utun address/gw/dns/mtu (`cellroute=1`: netsvc up/down).
6. **ROOT CAUSE of their last bug**: the mux id must sit where the modem's UL route rule reads it
   (metadata & 0x00ff0000 == 0x00010000): endpoint 2 HDR_EXT **big endian** (`p2le=0`). Little endian sent every
   packet to the modem's exception path.
7. GSI: two AP channels on two different event rings asserted the modem (`ipa_hal.c:7354`) in several layouts;
   their final setup is in `ipagsi.c` (read before porting the ring layout).
8. Idle: the modem services AP pipes only on its own timer once RRC is idle -> `wdaps=1` (WDA Set Powersave Mode
   0x2e) and a gateway echo every 3 s while iOS used the link in the last 120 s (`p2keep=3 p2keepidle=120`).
   ~150 ms RTT, 1.6-2.5 Mbit/s on 1 MB.
9. Never pmem-read the GSI inter-EE block 0x1e10000+ (hung their phone).

Sweet's own state (NOTES 2026-11-04): kernel IPA v4.2 path (xnu 0192-0194, `sweetipa=1`, off by default) reached
"setup complete" (GSI firmware, rings, endpoints, LAN RX, `pdp_ip0..3`); the modem announced IPA QMI 0x31 at 0:0x2e;
INIT_DRIVER (injected) **panicked our kernel**; 0194 (trace) never booted. The wland side of it was wland 0005
(IPA PAS + SMP2P "ipa"), which is not in the Wi-Fi boot daemon (wland 0239 = 0004 stack).

Port options: (A) redfin's userland design (utun + netsvc), re-done for IPA v4.2/GSI v2.2 register offsets;
(B) sweet's kernel path + redfin's protocol lessons 3, 4, 6, 8 (keeps CommCenter's pdp_ip and the Cellular UI).
Either way the first step is the same: get past INIT_DRIVER without a panic, with the table init done first.

## SMS (redfin: send from a test command only)

`cell.c` `sms=NUMBER`: one SMS-SUBMIT via WMS Raw Send 0x20 (7-bit, SIM SMSC; the SCA byte mattered); WMS Set Event
Report + Raw Read 0x22 log incoming SMS. **Sending from iOS Messages is not done there** (imagent gets no dylib).
Sweet's calls already go CommCenter -> baseband bridge (xnu 0184+) -> modem, so SMS should use the same bridge (WMS
QMI is service 0x05; xnu 0232 traced WMS/NAS headers for this). Redfin's WMS message layouts are the useful part.

## Microphone (redfin: works, but different hardware)

Redfin: ALC5513 (rt5514) on I2C + PRI TDM into ADSP AFE port 0x9001, ADM record COPP + ASM read session,
`/dev/redwood-mic` read by their audio plug-in (`sweet_mic.c`, kernel 0955, `mic=1`); `ws-q-mictest`: an AudioQueue
recorder with peak/RMS per second. Reusable for sweet: the ADSP record side (AFE TX + ADM record + ASM read; sweet has
the APR/AFE/ADM speaker side already), the `/dev/*-mic` ABI to iOS and the test tool.
Not reusable: sweet's mics are analog into the WCD9375 codec, read over SoundWire by the LPASS TX macro. The AP must
program the TX macro, the SoundWire master and the WCD937x (Linux: lpass-tx-macro, swr-mstr, wcd937x) - new work.

## Performance (kernel patch notes 0900-1061)

| redfin | what | sweet today |
|---|---|---|
| 0910 | no set/way D-cache flush on every idle WFI (idle CPU 133% -> 22%) | **flushed every idle**: ported as xnu 0240 |
| 0910 | AppleKeyStore lock state 0x6 + sls 3 (coreduetd polling storm) | 0x4/0: ported in 0240 (coreduetd is also disabled) |
| 0978 | CPU present maps the source surface copyback-cacheable + clean/invalidate (WC mapping: 41 ms -> 4.3 ms) | physical-address surfaces are mapped `kIOMapWriteCombineCache`; check which surfaces backboardd presents |
| 0996/1046 | direct scanout (the display reads the frame; no copy) with triple buffering | CPU copy every frame (3.5-8 ms) |
| 1040 | GPU fence waits on the GPU interrupt, not polling (backboardd wakeups 5000/s -> 200/s) | present gate polls with IODelay(50) |
| 0963/0970/1037 | CPU governor + touch/frame boost; 1036 GPU DCVS | CPUs pinned at max; GPU fixed level |
| 1039 | per-page GPU TLB invalidate instead of full flush | not checked |
| 0977/0984/0995/1049 | vsync pacing: tick held to SwapEnd, no timer slop on the tick, display-link phase | 0235 removed slop globally (no visible change) |
| 1041/0962 | CA display/render-server threads to idle big cores | 0167/0178 do similar |
