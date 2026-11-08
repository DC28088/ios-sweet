/* SPDX-License-Identifier: MIT */
#ifndef SWEET_PCM_FIFO_H
#define SWEET_PCM_FIFO_H
/* 0018: jitter FIFO between /dev/sweet-pcm (HAL I/O cycles on the system clock)
 * and the DSP's 10 ms periods (its own clock). Before 0018 each refill read
 * whatever the kernel ring held and zero-padded short reads (audible gaps), and
 * nothing bounded the backlog: a slow consumer left the 1.36 s kernel ring full,
 * which was the 1-2 s start/stop lag, and its drop-oldest overruns stuttered.
 * Here: a period is taken only from real samples after a short prebuffer; when
 * the source runs dry the FIFO rebuffers; once per window the minimum level
 * above prebuffer + two periods (latency the stream never used) is dropped.
 * Stereo s16 frames only; caller serializes. */
#include <stdint.h>
#include <string.h>
#define SWEET_PCM_FIFO_FRAMES 16384u
#define SWEET_PCM_FIFO_PERIOD 480u /* frames per DSP period (10 ms at 48 kHz) */
#define SWEET_PCM_FIFO_HIGH 6144u /* frames over target before a hard cut (128 ms) */
struct sweet_pcm_fifo {
 uint8_t data[SWEET_PCM_FIFO_FRAMES*4];
 uint32_t at,count,min,prev_min,max,prebuf,window_ms;
 unsigned playing;
 uint64_t window,dropped,trimmed,padded,starts,cuts;
};
static inline void sweet_pcm_fifo_init(struct sweet_pcm_fifo *f,uint32_t prebuf_frames,uint32_t window_ms)
{
 memset(f,0,sizeof *f);f->min=f->prev_min=UINT32_MAX;f->window_ms=window_ms?window_ms:1000;
 if(prebuf_frames<SWEET_PCM_FIFO_PERIOD)prebuf_frames=SWEET_PCM_FIFO_PERIOD;
 if(prebuf_frames>SWEET_PCM_FIFO_FRAMES/2)prebuf_frames=SWEET_PCM_FIFO_FRAMES/2;
 f->prebuf=prebuf_frames;
}
static inline void sweet_pcm_fifo_drop(struct sweet_pcm_fifo *f,uint32_t n)
{
 if(n>f->count)n=f->count;
 f->at=(f->at+n)%SWEET_PCM_FIFO_FRAMES;f->count-=n;
}
/* Whole frames only; on overflow the oldest frames go (counted in dropped). */
static inline void sweet_pcm_fifo_push(struct sweet_pcm_fifo *f,const uint8_t *p,uint32_t n)
{
 if(n>SWEET_PCM_FIFO_FRAMES){uint32_t skip=n-SWEET_PCM_FIFO_FRAMES;p+=(size_t)skip*4;f->dropped+=skip;n=SWEET_PCM_FIFO_FRAMES;}
 if(n>SWEET_PCM_FIFO_FRAMES-f->count){uint32_t d=n-(SWEET_PCM_FIFO_FRAMES-f->count);sweet_pcm_fifo_drop(f,d);f->dropped+=d;}
 uint32_t end=(f->at+f->count)%SWEET_PCM_FIFO_FRAMES;
 uint32_t first=n<SWEET_PCM_FIFO_FRAMES-end?n:SWEET_PCM_FIFO_FRAMES-end;
 memcpy(f->data+(size_t)end*4,p,(size_t)first*4);
 memcpy(f->data,p+(size_t)first*4,(size_t)(n-first)*4);
 f->count+=n;if(f->count>f->max)f->max=f->count;
}
/* Fill one period (always fully written: samples then silence).
 * Returns the frames of real samples in it (0..SWEET_PCM_FIFO_PERIOD). */
static inline uint32_t sweet_pcm_fifo_period(struct sweet_pcm_fifo *f,uint8_t *out,uint64_t now)
{
 const uint32_t want=SWEET_PCM_FIFO_PERIOD;
 memset(out,0,(size_t)want*4);
 if(!f->playing){
  if(f->count<f->prebuf)return 0;
  f->playing=1;++f->starts;f->min=f->prev_min=UINT32_MAX;f->window=now;
 }
 uint32_t n=f->count<want?f->count:want;
 uint32_t first=n<SWEET_PCM_FIFO_FRAMES-f->at?n:SWEET_PCM_FIFO_FRAMES-f->at;
 memcpy(out,f->data+(size_t)f->at*4,(size_t)first*4);
 memcpy(out+(size_t)first*4,f->data,(size_t)(n-first)*4);
 sweet_pcm_fifo_drop(f,n);
 if(n<want){f->padded+=want-n;f->playing=0;return n;} /* source stopped or late: rebuffer */
 if(f->count<f->min)f->min=f->count;
 /* Hard cap: a HAL burst far beyond any I/O cycle (seen at play/pause, up to
  * 340 ms) is cut back at once instead of after two windows. */
 if(f->count>f->prebuf+2*want+SWEET_PCM_FIFO_HIGH){uint32_t d=f->count-(f->prebuf+2*want);sweet_pcm_fifo_drop(f,d);f->trimmed+=d;f->cuts++;f->min=f->prev_min=UINT32_MAX;f->window=now;}
 if(now-f->window>=f->window_ms){
  /* Keep two periods above the prebuffer (a late sender wake drains several at
   * once) and trim only what two consecutive windows never used: HAL cycles and
   * 10 ms periods beat with a pattern that can be longer than one window. */
  const uint32_t keep=f->prebuf+2*want,low=f->min<f->prev_min?f->min:f->prev_min;
  if(low!=UINT32_MAX && low>keep){uint32_t d=low-keep;sweet_pcm_fifo_drop(f,d);f->trimmed+=d;f->min-=d;}
  f->prev_min=f->min;f->min=UINT32_MAX;f->window=now;
 }
 return n;
}
#endif
