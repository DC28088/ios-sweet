/* SPDX-License-Identifier: MIT */
/*
 * APR (Qualcomm Asynchronous Packet Router) packets for the sweet ADSP (SM7150 LPASS, Elite firmware):
 * AVCS readiness, AFE port start/stop/config and the CVD voice sequence that connects the modem's
 * CS voice ("default modem voice") to AFE ports. Pure byte building and parsing, no I/O: the
 * caller (wland, which owns the ADSP GLINK edge) writes each packet to GLINK channel
 * "apr_audio_svc" and feeds every received packet back.
 *
 * Opcodes and layouts follow the msm-4.14 techpack/audio headers (include/ipc/apr.h,
 * include/dsp/apr_audio-v2.h, include/dsp/q6voice.h, dsp/q6voice.c voc_start_voice_call order).
 * All fields are little-endian; packets are packed (no padding).
 */
#ifndef SWEET_APR_H
#define SWEET_APR_H

#include <stddef.h>
#include <stdint.h>

/* header (struct apr_hdr, 20 bytes) */
#define APR_HDR_SIZE            20u
#define APR_MSG_TYPE_EVENT      0u
#define APR_MSG_TYPE_CMD_RSP    1u
#define APR_MSG_TYPE_SEQ_CMD    2u
#define APR_HDR_FIELD(type)     ((uint16_t)((((type) & 3u) << 8) | ((APR_HDR_SIZE / 4u) << 4)))

#define APR_DOMAIN_MODEM        3u
#define APR_DOMAIN_ADSP         4u
#define APR_DOMAIN_APPS         5u

#define APR_SVC_ADSP_CORE       0x03u
#define APR_SVC_AFE             0x04u
#define APR_SVC_ADSP_MVM        0x09u
#define APR_SVC_ADSP_CVS        0x0Au
#define APR_SVC_ADSP_CVP        0x0Bu

#define APR_BASIC_RSP_RESULT    0x000110E8u

/* core */
#define AVCS_CMD_ADSP_EVENT_GET_STATE     0x0001290Cu
#define AVCS_CMDRSP_ADSP_EVENT_GET_STATE  0x0001290Du

/* AFE */
#define AFE_PORT_CMD_DEVICE_START         0x000100E5u
#define AFE_PORT_CMD_DEVICE_STOP          0x000100E6u
#define AFE_PORT_CMD_SET_PARAM_V2         0x000100EFu
#define AFE_SVC_CMD_SET_PARAM             0x000100F3u
#define AFE_MODULE_AUDIO_DEV_INTERFACE    0x0001020Cu
#define AFE_PARAM_ID_I2S_CONFIG           0x0001020Du
#define AFE_MODULE_CLOCK_SET              0x0001028Fu
#define AFE_PARAM_ID_CLOCK_SET            0x00010290u
#define AFE_PORT_ID_PRIMARY_MI2S_RX       0x1000u
#define AFE_PORT_ID_PRIMARY_MI2S_TX       0x1001u
#define AFE_PORT_ID_RX_CODEC_DMA_RX_0     0xB030u
#define AFE_PORT_ID_TX_CODEC_DMA_TX_3     0xB037u
#define AFE_PORT_I2S_SD0                  1u
#define AFE_PORT_I2S_SD1                  2u
#define AFE_PORT_I2S_MONO                 0u
#define AFE_PORT_I2S_STEREO               1u
#define Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT  0x100u
#define Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO 1u

/* CVD (MVM / CVS / CVP) */
#define VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION    0x000110FFu
#define VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL           0x00011327u
#define VSS_IMVM_CMD_ATTACH_VOCPROC                    0x0001123Eu
#define VSS_IMVM_CMD_DETACH_VOCPROC                    0x0001123Fu
#define VSS_IMVM_CMD_START_VOICE                       0x00011190u
#define VSS_IMVM_CMD_STOP_VOICE                        0x00011192u
#define VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION 0x00011140u
#define VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2 0x000112BFu
#define VSS_IVOCPROC_CMD_TOPOLOGY_COMMIT               0x00013198u
#define VSS_IVOCPROC_CMD_ENABLE                        0x000100C6u
#define VSS_IVOCPROC_CMD_DISABLE                       0x000110E1u
#define VSS_IVOLUME_CMD_MUTE_V2                        0x0001138Bu
#define VSS_IVOLUME_CMD_SET_STEP                       0x000112C2u
#define APRV2_IBASIC_CMD_DESTROY_SESSION               0x0001003Cu
#define VSS_IVOCPROC_TOPOLOGY_ID_NONE                  0x00010F70u
#define VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING        0x00010F7Cu
#define VSS_IVOCPROC_PORT_ID_NONE                      0xFFFFu
#define VSS_IVOCPROC_DIRECTION_RX_TX                   2u
#define VSS_IVOLUME_DIRECTION_TX                       0u
#define VSS_IVOLUME_DIRECTION_RX                       1u
#define SWEET_APR_SESSION_NAME_LEN                     20u
#define SWEET_APR_MODEM_VOICE_NAME                     "default modem voice"

struct sweet_apr_msg {
    uint16_t hdr_field, pkt_size;
    uint8_t src_svc, src_domain, dest_svc, dest_domain;
    uint16_t src_port, dest_port;
    uint32_t token, opcode;
    const uint8_t *payload;
    uint32_t payload_len;
};

/* Generic packet: returns the packet length, or 0 if it does not fit in cap. */
size_t sweet_apr_pack(uint8_t *out, size_t cap, uint8_t type, uint8_t dest_svc, uint16_t src_port, uint16_t dest_port,
                      uint32_t token, uint32_t opcode, const uint8_t *payload, uint16_t payload_len);
/* Parse a received packet; 0 on success, -1 if malformed (bad header length or size). */
int sweet_apr_parse(const uint8_t *in, size_t n, struct sweet_apr_msg *m);
/* For APR_BASIC_RSP_RESULT: the command opcode it answers and its status (0 = success). Returns -1 otherwise. */
int sweet_apr_basic_result(const struct sweet_apr_msg *m, uint32_t *opcode, uint32_t *status);

/* Individual commands (src_port = our client port, token echoed back by the DSP). */
size_t sweet_apr_avcs_get_state(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token);
size_t sweet_apr_afe_port(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, int start, uint16_t port_id);
size_t sweet_apr_afe_i2s_config(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, uint16_t port_id,
                                uint32_t sample_rate, uint16_t bit_width, uint16_t sd_lines, uint16_t stereo);
size_t sweet_apr_afe_clock(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, uint32_t clk_id,
                           uint32_t freq_hz, int enable);

/*
 * CS voice: voc_start_voice_call for a modem session (passive MVM/CVS named "default modem voice",
 * dual control, full CVP with both topologies NONE, enable, attach, unmute, start voice). The modem
 * owns the vocoder; the ADSP mixes it into AFE rx_port and takes uplink from tx_port.
 */
enum sweet_voice_step {
    SV_IDLE, SV_MVM_CREATE, SV_CVS_CREATE, SV_DUAL_CONTROL, SV_CVP_CREATE, SV_TOPOLOGY_COMMIT, SV_CVP_ENABLE,
    SV_ATTACH, SV_TX_UNMUTE, SV_START, SV_RUNNING,
    SV_STOP, SV_DETACH, SV_CVP_DISABLE, SV_CVP_DESTROY, SV_STOPPED,
    SV_FAILED
};
struct sweet_voice {
    enum sweet_voice_step step;
    uint16_t src_port;              /* our APR client port for the voice session */
    uint16_t tx_port, rx_port;      /* AFE ports, e.g. TX_CODEC_DMA_TX_3 and PRIMARY_MI2S_RX */
    uint16_t mvm_handle, cvs_handle, cvp_handle;
    uint32_t token;                 /* incremented per command */
    uint32_t last_opcode, last_status;
};
void sweet_voice_init(struct sweet_voice *v, uint16_t src_port, uint16_t tx_port, uint16_t rx_port);
/* Begin a call (from idle/stopped/failed); existing passive MVM/CVS handles are reused as in Linux. */
void sweet_voice_start(struct sweet_voice *v);
/* Next command to send for the current step (0 when nothing is to be sent: idle, running, done, failed). */
size_t sweet_voice_next(struct sweet_voice *v, uint8_t *out, size_t cap);
/* Feed a received packet. Returns 1 if it advanced the sequence, 0 if unrelated, -1 if the step failed. */
int sweet_voice_receive(struct sweet_voice *v, const uint8_t *in, size_t n);
/* Begin teardown from RUNNING (or any created state). */
void sweet_voice_stop(struct sweet_voice *v);
/* Volume step and mute while running (one-shot commands, not part of the sequence). */
size_t sweet_voice_rx_volume_step(const struct sweet_voice *v, uint8_t *out, size_t cap, uint32_t step);
size_t sweet_voice_tx_mute(const struct sweet_voice *v, uint8_t *out, size_t cap, int mute);
const char *sweet_voice_step_name(enum sweet_voice_step s);

#endif
