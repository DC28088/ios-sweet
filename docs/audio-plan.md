# Sweet call audio and general audio: plan (2026-11-04)

Status: research + first host-tested code. Nothing has run on the device yet. "Observed" means read
from a DTB, source file or local binary; "guess" means not yet verified on hardware.

## Hardware (observed)

| Part | Facts | Source |
| --- | --- | --- |
| ADSP (LPASS Q6) | TZ PAS id 1, firmware `adsp` (mdt + b00..b26), relocatable image (linked 0x93c00000, span 0x2600000) into `pil_adsp_region` 0x95700000 + 0x2600000; proxy supplies `vdd_lpi_cx`/`vdd_lpi_mx` (RPMh lcx/lmx), XO, `qcom,signal-aop` (QMP `load_state adsp`) | stock DTB `/soc/qcom,lpass@62400000` |
| ADSP SMP2P | SMEM items 443 (out) / 429 (in), SPI 172, APCS IPC bit 26; in-entry `slave-kernel` bits: 0 err-fatal, 1 err-ready, 2 proxy-unvote, 3 stop-ack | `/soc/qcom,smp2p-adsp` |
| ADSP GLINK | edge `lpass`, remote pid 2, SMEM transport, APCS IPC bit 24, SPI 170; channels `apr_audio_svc` (APR, 20 x 512 B intents), `IPCRTR` (QRTR), `fastrpcglink-apps-dsp`, `glink_ssr` | `/soc/qcom,glink/adsp` |
| APR | APR (not GPR); `qcom,msm-audio-apr` subsys `apr_adsp`; AP buffers for the ADSP via SMMU SID 0x1b21 mask 0xf (msm-audio-ion) | stock DTB |
| Codec | Bolero (LPASS RX macro 0x62ee0000, TX macro 0x62ec0000, VA macro 0x62f20000; WSA macro disabled) + WCD9375 (wcd937x) over SoundWire: TX master 0x62ed0000 (slave 0x01170223), RX master 0x62ef0000 (slave 0x01170224); WCD937x reset = LPI TLMM gpio24; micbias 2.75 V | Xiaomi `sweet-audio-overlay.dtsi`, `sm6150-audio-overlay.dtsi`, `sm6150-lpi.dtsi` (sweet-r-oss) |
| Mics | analog AMIC1..4 through WCD937x ADCs -> SoundWire -> TX macro decimators -> CDC DMA TX | sweet `qcom,audio-routing` |
| Speakers | two Awinic AW882xx (AW88263S) smart PAs on QUP SE4 I2C: TOP 0x34 (reset gpio64, irq gpio93, channel 0), BOT 0x35 (reset gpio72, irq gpio56, channel 1); fed by Primary MI2S (1 RX line SD0, 2 TX lines for I/V feedback); MI2S pins SCK gpio49, WS gpio50, DATA0 gpio52, DATA1 gpio51 | sweet overlay + `sweet-sdmmagpie-pinctrl.dtsi`; pmOS `sm7150-xiaomi-sweet.dts` agrees on the amps |
| Earpiece | guess: the TOP PA is the receiver in calls (stereo-speaker phone, no separate earpiece driver in the DT). Needs `mixer_paths*.xml` from the vendor partition | — |

## How iOS does call audio on a Qualcomm iPhone (observed in CommCenter's command drivers)

CommCenter's `QMICallAudioDriver` / `QMIAudioCommandDriver` / `QMIAudioRoutingCommandDriver` use the
Apple-private QMI service **0xe1** ("CallModule: Audio Service", client name "voice"):

| Message | ID | Notes |
| --- | --- | --- |
| GetVocoderType | 0x2f | queried when the driver is ready (sweet's bridge answers "not supported" today) |
| SetAudioPath | 0x20 | TLV 0x10 downlink path, 0x11 uplink path |
| SetVolume (source gain) | 0x23 | TLV 0x10 |
| SetTTY | 0x27 | TLV 0x10 |
| SetCtmMode | 0x13 | |
| StartTone / StopTone / BurstDtmf, AudioToneDone | — / ind 0x2c, 0x2e | QMICallAudioDriver |
| VocoderTypeInfo, AudioInterfaceStatus, AudioDistortion, AudioStatistics | indications | QMIAudioCommandDriver |

On a real Mav iPhone the modem owns the vocoder and the PCM goes to Apple's audio codec. On sweet the
modem's voice is wired to the ADSP's CVD instead, so **call audio does not need a PCM path through
iOS**. The AP only has to (1) answer service 0xe1 sensibly, and (2) start a CVD voice session on the ADSP
bound to the modem ("default modem voice") with AFE RX = Primary MI2S (amps) and AFE TX = the codec mic
port, when a call becomes active.

## CVD voice sequence (Linux msm-4.14 `voc_start_voice_call`, implemented in `userspace/adsp/sweet_apr.c`)

MVM create passive "default modem voice" -> CVS create passive (same name) -> MVM set policy dual
control -> CVP create full control v2 (RX_TX, tx/rx AFE ports, topologies NONE, EC internal mixing) ->
topology commit -> CVP enable -> MVM attach vocproc -> CVS TX unmute -> MVM start voice. Teardown: stop
voice -> detach -> CVP disable -> destroy CVP (the passive MVM/CVS sessions are reused). Handles come
back as the `src_port` of each `APR_BASIC_RSP_RESULT`. Host model:
`cc -std=c99 -Wall -Wextra -Werror -o /tmp/t userspace/adsp/test_sweet_apr.c userspace/adsp/sweet_apr.c && /tmp/t`.

## Milestones (each with its checkpoint)

1. **ADSP boots** (wland, mirroring its modem PAS boot; firmware to `/var/local/fw/adsp`): lcx/lmx top
   votes, QMP `load_state adsp on`, PAS 1 init_image / mem_setup(0x95700000, 0x2600000) / relocated
   segment copy / auth_and_reset. Checkpoint: SMP2P item 429 `slave-kernel` bit 1 (err-ready), no bit 0.
2. **APR channel**: second GLINK edge in wland (SMEM host 2, items 478/479/480, IPC bit 24), open
   `apr_audio_svc`, queue intents. Checkpoint: `AVCS_CMD_ADSP_EVENT_GET_STATE` answered with state 1.
3. **AFE + amps (downlink path)**: AW882xx init on SE4 I2C (kernel, next to the AW8624 haptic driver),
   MI2S pins, AFE clock PRI_MI2S_IBIT 1.536 MHz, I2S config, port start 0x1000. Checkpoint: AFE port
   start success and amp status registers report the clock (no fault).
4. **Call downlink**: kernel bridge answers QMI 0xe1 (vocoder AMR-WB/NB, path/volume/TTY success) and
   forwards call state (VOICE 0x2e indications), route and volume to wland; wland runs `sweet_voice`.
   Checkpoint: the far end is heard on the TOP PA during a call.
5. **Uplink (mic)**: LPASS macro clocks via AFE, LPI TLMM pins, WCD937x reset/supplies, SoundWire TX
   master enumeration, WCD937x micbias/ADC, TX macro decimator, CDC DMA TX_3 (0xB037) config
   (`AFE_PARAM_ID_CODEC_DMA_CONFIG`). Reference: mainline Linux `wcd937x.c`, `lpass-tx-macro.c`,
   `lpass-va-macro.c`, `soundwire/qcom.c`. Checkpoint: the far end hears this phone.
6. **iOS controls**: SetAudioPath -> receiver (TOP only) vs speaker (both PAs), SetVolume -> CVS volume
   step, mute -> CVS TX mute, tones/DTMF. Checkpoint: speaker button and volume keys work in a call.
7. **Media audio** (later): ASM playback from the CoreAudio HAL plug-in through ADSP shared memory
   (SMMU SID 0x1b21) to the same AFE port; microphone capture for apps.

## Riskiest unknowns

- Whether the ADSP audio PD waits for pd-mapper/service-registry over the ADSP's own QRTR (`IPCRTR`)
  before APR services answer (wland's QRTR/pd-mapper currently serve only the modem edge).
- AW882xx needs Awinic's per-profile parameters (`aw882xx_acf.bin`) for usable output; raw register
  defaults may be silent or unsafe for the speaker.
- Voice with topology NONE and no ACDB calibration: expected to pass audio without EC/NS (guess).
- Whether the modem's CS voice is routed to the ADSP without extra AP-side configuration (dual control
  policy suggests yes).
- wland's GLINK/SMP2P code is single-edge with static modem state: it needs a per-edge refactor.

## Needed from the device

- Approval to copy the ADSP firmware (already collected locally from the modem partition) to
  `/var/local/fw/adsp` on the phone.
- A read-only copy of the Android vendor audio configuration: `mixer_paths*.xml`,
  `audio_platform_info*.xml`, `acdbdata/*`, and the Awinic `aw882xx*` firmware/parameter files.
- Then: boots with the patched wland/kernel and a test call per milestone.
