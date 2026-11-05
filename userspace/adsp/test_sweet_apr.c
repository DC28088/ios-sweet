/* SPDX-License-Identifier: MIT */
/* Host model test: cc -std=c99 -Wall -Wextra -Werror -o /tmp/t userspace/adsp/test_sweet_apr.c userspace/adsp/sweet_apr.c && /tmp/t */
#include "sweet_apr.h"
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static uint16_t g16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t g32(const uint8_t *p) { return (uint32_t)g16(p) | (uint32_t)g16(p + 2) << 16; }

/* A fake ADSP: answers every command with APR_BASIC_RSP_RESULT from a fixed session handle per service. */
static size_t dsp_answer(const uint8_t *cmd, size_t n, uint8_t *out, size_t cap, uint32_t status)
{
    struct sweet_apr_msg m;
    if (sweet_apr_parse(cmd, n, &m)) return 0;
    uint16_t handle = m.dest_port ? m.dest_port : (uint16_t)(m.dest_svc == APR_SVC_ADSP_MVM ? 0x101 :
                                                             m.dest_svc == APR_SVC_ADSP_CVS ? 0x202 : 0x303);
    uint8_t p[8];
    memcpy(p, cmd + 16, 4);                 /* the opcode answered */
    p[4] = (uint8_t)status; p[5] = p[6] = p[7] = 0;
    size_t r = sweet_apr_pack(out, cap, APR_MSG_TYPE_CMD_RSP, m.dest_svc, handle, m.src_port, m.token,
                              APR_BASIC_RSP_RESULT, p, sizeof p);
    /* a response comes from the DSP: swap the domains as the ADSP would */
    out[5] = APR_DOMAIN_ADSP; out[9] = APR_DOMAIN_APPS;
    return r;
}

int main(void)
{
    uint8_t b[256], r[64];
    size_t n;

    /* header field: SEQ_CMD, 5 words, version 0 = 0x0250, as APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(20), 0) */
    n = sweet_apr_avcs_get_state(b, sizeof b, 7, 9);
    CHECK(n == 20 && g16(b) == 0x0250 && g16(b + 2) == 20 && b[4] == 3 && b[5] == 5 && b[8] == 3 && b[9] == 4);
    CHECK(g16(b + 6) == 7 && g32(b + 12) == 9 && g32(b + 16) == AVCS_CMD_ADSP_EVENT_GET_STATE);

    n = sweet_apr_afe_port(b, sizeof b, 1, 2, 1, AFE_PORT_ID_PRIMARY_MI2S_RX);   /* sizeof(afe_port_cmd_device_start) = 24 */
    CHECK(n == 24 && b[8] == APR_SVC_AFE && g32(b + 16) == AFE_PORT_CMD_DEVICE_START && g16(b + 20) == 0x1000);

    /* hdr 20 + port/size 4 + mem_hdr 12 + param_hdr_v1 12 + afe_param_id_i2s_cfg 20 = 68 */
    n = sweet_apr_afe_i2s_config(b, sizeof b, 1, 3, AFE_PORT_ID_PRIMARY_MI2S_RX, 48000, 16, AFE_PORT_I2S_SD0, AFE_PORT_I2S_STEREO);
    CHECK(n == 68 && g16(b + 22) == 32 && g32(b + 36) == AFE_MODULE_AUDIO_DEV_INTERFACE && g32(b + 40) == AFE_PARAM_ID_I2S_CONFIG);
    CHECK(g16(b + 44) == 20 && g32(b + 48) == 1 && g16(b + 52) == 16 && g16(b + 54) == 1 && g16(b + 56) == 1 && g32(b + 60) == 48000);

    /* hdr 20 + payload_size 4 + mem_hdr 12 + param_hdr_v1 12 + afe_clk_set 20 = 68; 1.536 MHz = 48k x 16 bit x 2 */
    n = sweet_apr_afe_clock(b, sizeof b, 1, 4, Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT, 1536000, 1);
    CHECK(n == 68 && g32(b + 16) == AFE_SVC_CMD_SET_PARAM && g32(b + 20) == 32 && g32(b + 36) == AFE_MODULE_CLOCK_SET);
    CHECK(g32(b + 52) == 0x100 && g32(b + 56) == 1536000 && g16(b + 60) == 1 && g32(b + 64) == 1);

    /* v3 headers: 16-byte param_hdr_v3, 4 bytes longer, and the AFE_PORT_CMD_SET_PARAM_V3 / AFE_SVC_CMD_SET_PARAM_V2 opcodes */
    n = sweet_apr_afe_i2s_config_v3(b, sizeof b, 1, 5, AFE_PORT_ID_PRIMARY_MI2S_RX, 48000, 16, AFE_PORT_I2S_SD0,
                                    AFE_PORT_I2S_STEREO);
    CHECK(n == 72 && g16(b + 22) == 36 && g32(b + 36) == AFE_MODULE_AUDIO_DEV_INTERFACE && g16(b + 40) == 0 &&
          g16(b + 42) == 0 && g32(b + 44) == AFE_PARAM_ID_I2S_CONFIG && g32(b + 48) == 20);
    CHECK(g32(b + 52) == 1 && g16(b + 56) == 16 && g16(b + 58) == 1 && g16(b + 60) == 1 && g32(b + 64) == 48000);
    n = sweet_apr_afe_clock_v2(b, sizeof b, 1, 6, Q6AFE_LPASS_CLK_ID_PRI_MI2S_IBIT, 1536000, 1);
    CHECK(n == 72 && g32(b + 16) == AFE_SVC_CMD_SET_PARAM_V2 && g32(b + 20) == 36 && g32(b + 36) == AFE_MODULE_CLOCK_SET &&
          g32(b + 44) == AFE_PARAM_ID_CLOCK_SET && g32(b + 48) == 20);
    CHECK(g32(b + 52) == 1 && g32(b + 56) == 0x100 && g32(b + 60) == 1536000 && g16(b + 64) == 1 && g32(b + 68) == 1);

    /* parser rejects truncated/oversized packets */
    struct sweet_apr_msg m;
    CHECK(sweet_apr_parse(b, 19, &m) == -1);
    CHECK(sweet_apr_parse(b, 60, &m) == -1);
    CHECK(sweet_apr_parse(b, n, &m) == 0 && m.payload_len == 52);   /* the last packet above is the 72-byte clock v2 */

    /* the voice sequence against the fake DSP */
    struct sweet_voice v;
    sweet_voice_init(&v, 0x0005, AFE_PORT_ID_TX_CODEC_DMA_TX_3, AFE_PORT_ID_PRIMARY_MI2S_RX);
    CHECK(sweet_voice_next(&v, b, sizeof b) == 0);   /* idle: nothing */
    sweet_voice_start(&v);
    static const uint32_t want[] = { VSS_IMVM_CMD_CREATE_PASSIVE_CONTROL_SESSION, VSS_ISTREAM_CMD_CREATE_PASSIVE_CONTROL_SESSION,
        VSS_IMVM_CMD_SET_POLICY_DUAL_CONTROL, VSS_IVOCPROC_CMD_CREATE_FULL_CONTROL_SESSION_V2, VSS_IVOCPROC_CMD_TOPOLOGY_COMMIT,
        VSS_IVOCPROC_CMD_ENABLE, VSS_IMVM_CMD_ATTACH_VOCPROC, VSS_IVOLUME_CMD_MUTE_V2, VSS_IMVM_CMD_START_VOICE };
    static const size_t want_len[] = { 40, 40, 21, 64, 20, 20, 22, 26, 20 };
    for (unsigned i = 0; i < sizeof want / sizeof want[0]; i++) {
        n = sweet_voice_next(&v, b, sizeof b);
        CHECK(n == want_len[i] && g32(b + 16) == want[i]);
        if (i == 0) CHECK(!strcmp((const char *)b + 20, "default modem voice") && g16(b + 10) == 0 && b[8] == APR_SVC_ADSP_MVM);
        if (i == 3) CHECK(g16(b + 20) == 2 && g16(b + 22) == 0xB037 && g32(b + 24) == VSS_IVOCPROC_TOPOLOGY_ID_NONE &&
                          g16(b + 28) == 0x1000 && g16(b + 42) == 0xFFFF);
        if (i == 6) CHECK(g16(b + 10) == 0x101 && g16(b + 20) == 0x303);   /* to the MVM, carrying the CVP handle */
        if (i == 7) CHECK(g16(b + 10) == 0x202 && b[8] == APR_SVC_ADSP_CVS);
        /* an unrelated packet (wrong token) does not advance */
        size_t a = dsp_answer(b, n, r, sizeof r, 0);
        r[12] ^= 0x55;
        CHECK(sweet_voice_receive(&v, r, a) == 0);
        r[12] ^= 0x55;
        CHECK(sweet_voice_receive(&v, r, a) == 1);
    }
    CHECK(v.step == SV_RUNNING && v.mvm_handle == 0x101 && v.cvs_handle == 0x202 && v.cvp_handle == 0x303);
    CHECK(sweet_voice_next(&v, b, sizeof b) == 0);

    n = sweet_voice_rx_volume_step(&v, b, sizeof b, 4);
    CHECK(n == 28 && g16(b + 10) == 0x202 && g16(b + 20) == 1 && g32(b + 22) == 4);

    /* teardown keeps the passive sessions; the next call starts at dual control */
    sweet_voice_stop(&v);
    static const uint32_t down[] = { VSS_IMVM_CMD_STOP_VOICE, VSS_IMVM_CMD_DETACH_VOCPROC, VSS_IVOCPROC_CMD_DISABLE,
        APRV2_IBASIC_CMD_DESTROY_SESSION };
    for (unsigned i = 0; i < 4; i++) {
        n = sweet_voice_next(&v, b, sizeof b);
        CHECK(n && g32(b + 16) == down[i]);
        size_t a = dsp_answer(b, n, r, sizeof r, 0);
        CHECK(sweet_voice_receive(&v, r, a) == 1);
    }
    CHECK(v.step == SV_STOPPED && v.cvp_handle == 0 && v.mvm_handle == 0x101);
    sweet_voice_start(&v);
    CHECK(v.step == SV_DUAL_CONTROL);

    /* a failing command stops the sequence; topology commit failure is tolerated */
    sweet_voice_init(&v, 5, 0xB037, 0x1000);
    sweet_voice_start(&v);
    n = sweet_voice_next(&v, b, sizeof b);
    n = dsp_answer(b, n, r, sizeof r, 0x10);
    CHECK(sweet_voice_receive(&v, r, n) == -1 && v.step == SV_FAILED);
    v.step = SV_TOPOLOGY_COMMIT; v.cvp_handle = 0x303;
    n = sweet_voice_next(&v, b, sizeof b);
    n = dsp_answer(b, n, r, sizeof r, 0x7);
    CHECK(sweet_voice_receive(&v, r, n) == 1 && v.step == SV_CVP_ENABLE);

    printf("%s (%d failures)\n", failures ? "FAILED" : "all APR model checks passed", failures);
    return failures != 0;
}
