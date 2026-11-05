/* SPDX-License-Identifier: MIT */
/* APR packet building/parsing and the CS voice sequence for the sweet ADSP. See sweet_apr.h. */
#include "sweet_apr.h"
#include <string.h>

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { put16(p, (uint16_t)v); put16(p + 2, (uint16_t)(v >> 16)); }
static void put_param_hdr_v3(uint8_t *p, uint32_t module, uint32_t param, uint32_t size)
{
    put32(p, module);
    put16(p + 4, 0);         /* instance_id */
    put16(p + 6, 0);         /* reserved */
    put32(p + 8, param);
    put32(p + 12, size);
}
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)get16(p) | ((uint32_t)get16(p + 2) << 16); }

size_t sweet_apr_pack(uint8_t *out, size_t cap, uint8_t type, uint8_t dest_svc, uint16_t src_port, uint16_t dest_port,
                      uint32_t token, uint32_t opcode, const uint8_t *payload, uint16_t payload_len)
{
    const size_t n = APR_HDR_SIZE + payload_len;
    if (!out || n > cap || n > 0xffff) return 0;
    put16(out + 0, APR_HDR_FIELD(type));
    put16(out + 2, (uint16_t)n);
    out[4] = dest_svc;           /* src_svc: Linux apr_send_pkt uses the destination service id for both */
    out[5] = APR_DOMAIN_APPS;
    put16(out + 6, src_port);
    out[8] = dest_svc;
    out[9] = APR_DOMAIN_ADSP;
    put16(out + 10, dest_port);
    put32(out + 12, token);
    put32(out + 16, opcode);
    if (payload_len) memcpy(out + APR_HDR_SIZE, payload, payload_len);
    return n;
}

int sweet_apr_parse(const uint8_t *in, size_t n, struct sweet_apr_msg *m)
{
    if (!in || !m || n < APR_HDR_SIZE) return -1;
    m->hdr_field = get16(in);
    m->pkt_size = get16(in + 2);
    const uint32_t hlen = ((m->hdr_field >> 4) & 0xfu) * 4u;
    if (hlen < APR_HDR_SIZE || m->pkt_size < hlen || m->pkt_size > n) return -1;
    m->src_svc = in[4];
    m->src_domain = in[5];
    m->src_port = get16(in + 6);
    m->dest_svc = in[8];
    m->dest_domain = in[9];
    m->dest_port = get16(in + 10);
    m->token = get32(in + 12);
    m->opcode = get32(in + 16);
    m->payload = in + hlen;
    m->payload_len = m->pkt_size - hlen;
    return 0;
}

int sweet_apr_basic_result(const struct sweet_apr_msg *m, uint32_t *opcode, uint32_t *status)
{
    if (!m || m->opcode != APR_BASIC_RSP_RESULT || m->payload_len < 8) return -1;
    if (opcode) *opcode = get32(m->payload);
    if (status) *status = get32(m->payload + 4);
    return 0;
}

size_t sweet_apr_avcs_get_state(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token)
{
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_ADSP_CORE, src_port, 0, token,
                          AVCS_CMD_ADSP_EVENT_GET_STATE, NULL, 0);
}

/* struct afe_port_cmd_device_start/stop: u16 port_id, u16 reserved */
size_t sweet_apr_afe_port(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, int start, uint16_t port_id)
{
    uint8_t p[4] = { 0 };
    put16(p, port_id);
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_AFE, src_port, 0, token,
                          start ? AFE_PORT_CMD_DEVICE_START : AFE_PORT_CMD_DEVICE_STOP, p, sizeof p);
}

/*
 * AFE_PORT_CMD_SET_PARAM_V2, in band: u16 port_id, u16 payload_size, mem_mapping_hdr (3 x u32 = 0),
 * then param_hdr_v1 (u32 module, u32 param, u16 size, u16 reserved) + struct afe_param_id_i2s_cfg
 * (u32 minor 1, u16 bit_width, u16 channel_mode, u16 mono_stereo, u16 ws_src 1, u32 rate, u16 format 0, u16 0).
 */
size_t sweet_apr_afe_i2s_config(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, uint16_t port_id,
                                uint32_t sample_rate, uint16_t bit_width, uint16_t sd_lines, uint16_t stereo)
{
    uint8_t p[4 + 12 + 12 + 20] = { 0 };
    uint8_t *ph = p + 16, *cfg = p + 28;
    put16(p, port_id);
    put16(p + 2, 12 + 20);
    put32(ph, AFE_MODULE_AUDIO_DEV_INTERFACE);
    put32(ph + 4, AFE_PARAM_ID_I2S_CONFIG);
    put16(ph + 8, 20);
    put32(cfg, 1);
    put16(cfg + 4, bit_width);
    put16(cfg + 6, sd_lines);
    put16(cfg + 8, stereo);
    put16(cfg + 10, 1);      /* AFE_PORT_CONFIG_I2S_WS_SRC_INTERNAL: the LPASS is the I2S master */
    put32(cfg + 12, sample_rate);
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_AFE, src_port, 0, token, AFE_PORT_CMD_SET_PARAM_V2,
                          p, sizeof p);
}

/*
 * AFE_SVC_CMD_SET_PARAM (v1), in band: u32 payload_size, mem_mapping_hdr (12 bytes of 0), param_hdr_v1,
 * struct afe_clk_set (u32 minor 1, u32 clk_id, u32 freq, u16 attri, u16 root 0, u32 enable).
 */
size_t sweet_apr_afe_clock(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, uint32_t clk_id,
                           uint32_t freq_hz, int enable)
{
    uint8_t p[4 + 12 + 12 + 20] = { 0 };
    uint8_t *ph = p + 16, *c = p + 28;
    put32(p, 12 + 20);
    put32(ph, AFE_MODULE_CLOCK_SET);
    put32(ph + 4, AFE_PARAM_ID_CLOCK_SET);
    put16(ph + 8, 20);
    put32(c, 1);
    put32(c + 4, clk_id);
    put32(c + 8, freq_hz);
    put16(c + 12, Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO);
    put32(c + 16, enable ? 1u : 0u);
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_AFE, src_port, 0, token, AFE_SVC_CMD_SET_PARAM,
                          p, sizeof p);
}

/*
 * The same two parameters with a param_hdr_v3 (module, instance_id 0, param, u32 size). Linux sends these
 * (AFE_PORT_CMD_SET_PARAM_V3 / AFE_SVC_CMD_SET_PARAM_V2) when the ADSP supports instance IDs; the header
 * is 4 bytes longer and the in-band payload_size counts only the packed header+data.
 */
size_t sweet_apr_afe_i2s_config_v3(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, uint16_t port_id,
                                   uint32_t sample_rate, uint16_t bit_width, uint16_t sd_lines, uint16_t stereo)
{
    uint8_t p[4 + 12 + 16 + 20] = { 0 };
    uint8_t *ph = p + 16, *cfg = p + 32;
    put16(p, port_id);
    put16(p + 2, 16 + 20);
    put_param_hdr_v3(ph, AFE_MODULE_AUDIO_DEV_INTERFACE, AFE_PARAM_ID_I2S_CONFIG, 20);
    put32(cfg, 1);
    put16(cfg + 4, bit_width);
    put16(cfg + 6, sd_lines);
    put16(cfg + 8, stereo);
    put16(cfg + 10, 1);
    put32(cfg + 12, sample_rate);
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_AFE, src_port, 0, token, AFE_PORT_CMD_SET_PARAM_V3,
                          p, sizeof p);
}

size_t sweet_apr_afe_clock_v2(uint8_t *out, size_t cap, uint16_t src_port, uint32_t token, uint32_t clk_id,
                              uint32_t freq_hz, int enable)
{
    uint8_t p[4 + 12 + 16 + 20] = { 0 };
    uint8_t *ph = p + 16, *c = p + 32;
    put32(p, 16 + 20);
    put_param_hdr_v3(ph, AFE_MODULE_CLOCK_SET, AFE_PARAM_ID_CLOCK_SET, 20);
    put32(c, 1);
    put32(c + 4, clk_id);
    put32(c + 8, freq_hz);
    put16(c + 12, Q6AFE_LPASS_CLK_ATTRIBUTE_COUPLE_NO);
    put32(c + 16, enable ? 1u : 0u);
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_AFE, src_port, 0, token, AFE_SVC_CMD_SET_PARAM_V2,
                          p, sizeof p);
}

/* ---- the CS voice sequence ---- */

void sweet_voice_init(struct sweet_voice *v, uint16_t src_port, uint16_t tx_port, uint16_t rx_port)
{
    memset(v, 0, sizeof *v);
    v->step = SV_IDLE;
    v->src_port = src_port;
    v->tx_port = tx_port;
    v->rx_port = rx_port;
}

void sweet_voice_start(struct sweet_voice *v)
{
    if (v->step != SV_IDLE && v->step != SV_STOPPED && v->step != SV_FAILED) return;
    /* As voice_create_mvm_cvs_session: passive sessions are created once and reused by later calls. */
    v->step = !v->mvm_handle ? SV_MVM_CREATE : !v->cvs_handle ? SV_CVS_CREATE : SV_DUAL_CONTROL;
}

void sweet_voice_stop(struct sweet_voice *v)
{
    if (v->step == SV_RUNNING || v->step == SV_START) v->step = SV_STOP;
    else if (v->cvp_handle && v->step > SV_CVP_CREATE && v->step < SV_STOP) v->step = SV_CVP_DISABLE;
    else if (v->step < SV_STOP) v->step = SV_STOPPED;
}

static size_t name_payload(uint8_t *name)
{
    memset(name, 0, SWEET_APR_SESSION_NAME_LEN);
    memcpy(name, SWEET_APR_MODEM_VOICE_NAME, sizeof SWEET_APR_MODEM_VOICE_NAME);   /* 20 bytes with the NUL */
    return SWEET_APR_SESSION_NAME_LEN;
}

size_t sweet_voice_next(struct sweet_voice *v, uint8_t *out, size_t cap)
{
    uint8_t p[64] = { 0 };
    size_t n = 0;
    uint8_t svc = APR_SVC_ADSP_MVM;
    uint16_t dest = v->mvm_handle;
    uint32_t op;
    switch (v->step) {
    case SV_MVM_CREATE:
        op = VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION; n = name_payload(p); dest = 0; break;
    case SV_CVS_CREATE:
        op = VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION; n = name_payload(p); svc = APR_SVC_ADSP_CVS; dest = 0; break;
    case SV_DUAL_CONTROL:
        op = VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL; p[0] = 1; n = 1; break;   /* bool enable_flag */
    case SV_CVP_CREATE: {
        /* struct vss_ivocproc_cmd_create_full_control_session_v2_t: u16 direction, u16 tx_port, u32 tx_topology,
         * u16 rx_port, u32 rx_topology, u32 profile, u32 vocproc_mode, u16 ec_ref_port, char name[20] */
        op = VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2; svc = APR_SVC_ADSP_CVP; dest = 0;
        put16(p + 0, VSS_IVOCPROC_DIRECTION_RX_TX);
        put16(p + 2, v->tx_port);
        put32(p + 4, VSS_IVOCPROC_TOPOLOGY_ID_NONE);
        put16(p + 8, v->rx_port);
        put32(p + 10, VSS_IVOCPROC_TOPOLOGY_ID_NONE);
        put32(p + 14, 0);
        put32(p + 18, VSS_IVOCPROC_VOCPROC_MODE_EC_INT_MIXING);
        put16(p + 22, VSS_IVOCPROC_PORT_ID_NONE);
        name_payload(p + 24);
        n = 24 + SWEET_APR_SESSION_NAME_LEN;
        break;
    }
    case SV_TOPOLOGY_COMMIT: op = VSS_IVOCPROC_CMD_TOPOLOGY_COMMIT; svc = APR_SVC_ADSP_CVP; dest = v->cvp_handle; break;
    case SV_CVP_ENABLE: op = VSS_IVOCPROC_CMD_ENABLE; svc = APR_SVC_ADSP_CVP; dest = v->cvp_handle; break;
    case SV_ATTACH: op = VSS_IMVM_CMD_ATTACH_VOCPROC; put16(p, v->cvp_handle); n = 2; break;
    case SV_TX_UNMUTE:
        op = VSS_IVOLUME_CMD_MUTE_V2; svc = APR_SVC_ADSP_CVS; dest = v->cvs_handle;
        put16(p, VSS_IVOLUME_DIRECTION_TX); put16(p + 2, 0); put16(p + 4, 0); n = 6; break;
    case SV_START: op = VSS_IMVM_CMD_START_VOICE; break;
    case SV_STOP: op = VSS_IMVM_CMD_STOP_VOICE; break;
    case SV_DETACH: op = VSS_IMVM_CMD_DETACH_VOCPROC; put16(p, v->cvp_handle); n = 2; break;
    case SV_CVP_DISABLE: op = VSS_IVOCPROC_CMD_DISABLE; svc = APR_SVC_ADSP_CVP; dest = v->cvp_handle; break;
    case SV_CVP_DESTROY: op = APRV2_IBASIC_CMD_DESTROY_SESSION; svc = APR_SVC_ADSP_CVP; dest = v->cvp_handle; break;
    default: return 0;
    }
    v->last_opcode = op;
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, svc, v->src_port, dest, ++v->token, op, p, (uint16_t)n);
}

static enum sweet_voice_step after(enum sweet_voice_step s)
{
    switch (s) {
    case SV_MVM_CREATE: return SV_CVS_CREATE;
    case SV_CVS_CREATE: return SV_DUAL_CONTROL;
    case SV_DUAL_CONTROL: return SV_CVP_CREATE;
    case SV_CVP_CREATE: return SV_TOPOLOGY_COMMIT;
    case SV_TOPOLOGY_COMMIT: return SV_CVP_ENABLE;
    case SV_CVP_ENABLE: return SV_ATTACH;
    case SV_ATTACH: return SV_TX_UNMUTE;
    case SV_TX_UNMUTE: return SV_START;
    case SV_START: return SV_RUNNING;
    case SV_STOP: return SV_DETACH;
    case SV_DETACH: return SV_CVP_DISABLE;
    case SV_CVP_DISABLE: return SV_CVP_DESTROY;
    case SV_CVP_DESTROY: return SV_STOPPED;
    default: return s;
    }
}

int sweet_voice_receive(struct sweet_voice *v, const uint8_t *in, size_t n)
{
    struct sweet_apr_msg m;
    uint32_t op, status;
    if (sweet_apr_parse(in, n, &m) || m.dest_port != v->src_port || sweet_apr_basic_result(&m, &op, &status)) return 0;
    if (op != v->last_opcode || m.token != v->token) return 0;
    v->last_status = status;
    /* A newer CVD may not know TOPOLOGY_COMMIT for TOPOLOGY_ID_NONE; Linux only sends it for CVD >= 2.3. Skip it. */
    if (status && v->step != SV_TOPOLOGY_COMMIT) {
        v->step = SV_FAILED;
        return -1;
    }
    if (v->step == SV_MVM_CREATE) v->mvm_handle = m.src_port;      /* handles arrive as the response's src_port */
    else if (v->step == SV_CVS_CREATE) v->cvs_handle = m.src_port;
    else if (v->step == SV_CVP_CREATE) v->cvp_handle = m.src_port;
    else if (v->step == SV_CVP_DESTROY) v->cvp_handle = 0;
    v->step = after(v->step);
    return 1;
}

/* struct vss_ivolume_cmd_set_step_t: u16 direction, u32 value, u16 ramp_ms */
size_t sweet_voice_rx_volume_step(const struct sweet_voice *v, uint8_t *out, size_t cap, uint32_t step)
{
    uint8_t p[8] = { 0 };
    put16(p, VSS_IVOLUME_DIRECTION_RX);
    put32(p + 2, step);
    put16(p + 6, 20);
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_ADSP_CVS, v->src_port, v->cvs_handle, 0,
                          VSS_IVOLUME_CMD_SET_STEP, p, sizeof p);
}

size_t sweet_voice_tx_mute(const struct sweet_voice *v, uint8_t *out, size_t cap, int mute)
{
    uint8_t p[6] = { 0 };
    put16(p, VSS_IVOLUME_DIRECTION_TX);
    put16(p + 2, mute ? 1 : 0);
    return sweet_apr_pack(out, cap, APR_MSG_TYPE_SEQ_CMD, APR_SVC_ADSP_CVS, v->src_port, v->cvs_handle, 0,
                          VSS_IVOLUME_CMD_MUTE_V2, p, sizeof p);
}

const char *sweet_voice_step_name(enum sweet_voice_step s)
{
    static const char *const names[] = { "idle", "mvm-create", "cvs-create", "dual-control", "cvp-create",
        "topology-commit", "cvp-enable", "attach", "tx-unmute", "start", "running", "stop", "detach", "cvp-disable",
        "cvp-destroy", "stopped", "failed" };
    return (unsigned)s < sizeof names / sizeof names[0] ? names[s] : "?";
}
