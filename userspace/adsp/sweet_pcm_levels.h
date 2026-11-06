/* SPDX-License-Identifier: MIT */
#ifndef SWEET_PCM_LEVELS_H
#define SWEET_PCM_LEVELS_H
#include <stdint.h>
#include <stddef.h>
/* Aggregate measurements only; no sample storage or audio recording. */
struct sweet_pcm_levels { uint64_t frames, nonzero_samples, squares; unsigned peak; };
static inline void sweet_pcm_levels_add(struct sweet_pcm_levels *s,const uint8_t *b,size_t n)
{
 s->frames+=n/4;
 for(size_t i=0;i+1<n;i+=2){
  unsigned raw=(unsigned)b[i]|(unsigned)b[i+1]<<8;
  int value=raw>=32768u?(int)raw-65536:(int)raw;
  unsigned magnitude=(unsigned)(value<0?-value:value);
  if(magnitude>s->peak)s->peak=magnitude;
  s->nonzero_samples+=magnitude!=0;
  s->squares+=(uint64_t)magnitude*magnitude;
 }
}
#endif
