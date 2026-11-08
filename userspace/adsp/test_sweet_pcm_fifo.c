/* SPDX-License-Identifier: MIT */
/* Simulates the HAL writing whole I/O cycles on the system clock while the DSP
 * takes 10 ms periods through a sender whose wakes are late, and checks that
 * the FIFO keeps sample order, bounds latency and pads only when starved. */
#include "sweet_pcm_fifo.h"
#include <assert.h>
#include <stdio.h>
static struct sweet_pcm_fifo f;
static uint32_t next_in,next_out;
static void hal(uint32_t frames)
{
 uint8_t b[4096*4];
 while(frames){
  uint32_t n=frames<4096?frames:4096;
  for(uint32_t i=0;i<n;i++){uint32_t v=next_in++;memcpy(b+4*i,&v,4);}
  sweet_pcm_fifo_push(&f,b,n);frames-=n;
 }
}
/* Returns real frames; checks they continue the sequence (allowing trims). */
static uint32_t dsp(uint64_t now)
{
 uint8_t b[SWEET_PCM_FIFO_PERIOD*4];uint32_t n=sweet_pcm_fifo_period(&f,b,now);
 for(uint32_t i=0;i<n;i++){uint32_t v;memcpy(&v,b+4*i,4);assert(v>=next_out);next_out=v+1;}
 for(uint32_t i=n;i<SWEET_PCM_FIFO_PERIOD;i++){uint32_t v;memcpy(&v,b+4*i,4);assert(!v);}
 return n;
}
/* One continuous timeline (1 ms ticks): the HAL delivers 'burst' frames each
 * time 48 frames/ms add up to one; the sender services the DSP every 10 ms but
 * on every 7th period wakes late_ms late, then catches up on the periods that
 * completed meanwhile (8 queued DSP slots). */
static uint64_t produced,due,wake;
static uint64_t run(uint32_t burst,uint64_t from,uint64_t ms,unsigned late_ms,uint32_t *max_latency)
{
 uint64_t pad0=f.padded;
 for(uint64_t t=from;t<from+ms;t++){
  while(produced+burst<=(t+1)*48){hal(burst);produced+=burst;}
  if(t>=wake){
   while(due<=t){dsp(due);due+=10;}
   wake=t+1;if((t/10)%7==0)wake=t+late_ms;
  }
  if(t>from && f.count>*max_latency)*max_latency=f.count;
 }
 return f.padded-pad0;
}
int main(void)
{
 static const uint32_t bursts[]={256,512,1024,2048,4096};
 for(unsigned b=0;b<sizeof bursts/sizeof bursts[0];b++)for(unsigned late=2;late<=33;late+=31){
  sweet_pcm_fifo_init(&f,1440,1000);next_in=1;next_out=0;produced=due=wake=0;uint32_t maxl=0,steady=0;
  hal(65536-1); /* 1.36 s stale backlog: the old full kernel ring */
  run(bursts[b],0,20000,late,&maxl);
  uint64_t gaps=run(bursts[b],20000,10000,late,&steady);
  printf("burst %4u late %2u ms: steady max %5u frames (%.1f ms), stale removed %llu, gaps in steady 10 s %llu\n",
   bursts[b],late,steady,steady/48.0,(unsigned long long)(f.trimmed+f.dropped),(unsigned long long)gaps);
  assert(f.trimmed+f.dropped>=55000);                    /* stale backlog removed (overflow + trim) */
  assert(steady<=1440+bursts[b]+6*SWEET_PCM_FIFO_PERIOD); /* bounded latency */
  assert(!gaps);                                          /* no gaps in steady playback */
  uint64_t cuts=f.cuts;run(bursts[b],30000,10000,late,&steady);assert(f.cuts==cuts); /* no hard cuts in steady play */
 }
 /* source stops: pads once and rebuffers; resumes after the prebuffer only */
 sweet_pcm_fifo_init(&f,1440,1000);next_in=1;next_out=0;uint8_t o[SWEET_PCM_FIFO_PERIOD*4];
 hal(1000);assert(!sweet_pcm_fifo_period(&f,o,0) && !f.playing);
 hal(440);assert(sweet_pcm_fifo_period(&f,o,10)==480 && f.playing);
 assert(sweet_pcm_fifo_period(&f,o,20)==480 && sweet_pcm_fifo_period(&f,o,30)==480);
 assert(sweet_pcm_fifo_period(&f,o,40)==0 && !f.playing && f.padded==480);
 /* a 340 ms HAL burst is cut back to target on the next period */
 sweet_pcm_fifo_init(&f,1440,1000);next_in=1;next_out=0;hal(2400);assert(sweet_pcm_fifo_period(&f,o,0)==480);
 hal(16000);assert(sweet_pcm_fifo_period(&f,o,10)==480 && f.cuts==1 && f.count==1440+960);
 /* overflow drops oldest, never splits frames */
 sweet_pcm_fifo_init(&f,480,1000);uint8_t big[4096*4]={0};
 for(unsigned i=0;i<5;i++)sweet_pcm_fifo_push(&f,big,4096);
 assert(f.count==SWEET_PCM_FIFO_FRAMES && f.dropped==5*4096-SWEET_PCM_FIFO_FRAMES);
 puts("PASS: PCM FIFO keeps order, trims stale backlog, bounds latency for 256..4096-frame HAL cycles, no steady gaps with 33 ms late wakes");
 return 0;
}
