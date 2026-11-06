/* SPDX-License-Identifier: MIT */
#ifndef SWEET_PCM_H
#define SWEET_PCM_H
#include "sweet_apr.h"
/* Bounded, silent playback probe. No amplifier, GPIO or firmware operations.
 * Caller owns DMA until done, !failed and !mapped; an uncertain timeout retains it. */
#define SWEET_PCM_PERIOD 1920u /* 480 stereo s16 frames = 10 ms at 48 kHz */
#define SWEET_PCM_STRIDE 4096u
#define SWEET_PCM_MAP_BYTES 32768u /* known-working reference PCM map window */
#define SWEET_PCM_SLOTS 4u
#define SWEET_PCM_LIMIT 100u
struct sweet_pcm {
    unsigned step, waiting, mapped, copp_open, stream_open, failed, done;
    unsigned sent, completed, inflight;
    uint32_t token, pending_op, handle, copp, status;
    uint64_t dsp, deadline;
    unsigned busy,adm_default;
    uint32_t write_token[SWEET_PCM_SLOTS];
};
void sweet_pcm_init(struct sweet_pcm *s,uint64_t dsp);
/* next command, after start or a matching response; 0 if waiting/done. */
size_t sweet_pcm_next(struct sweet_pcm *s,uint8_t *out,size_t cap,uint64_t now);
/* 1 accepted, 0 unrelated, -1 malformed/matching failure (teardown scheduled). */
int sweet_pcm_receive(struct sweet_pcm *s,const uint8_t *packet,size_t n);
void sweet_pcm_abort(struct sweet_pcm *s,uint32_t status);
void sweet_pcm_poll(struct sweet_pcm *s,uint64_t now);
#endif
