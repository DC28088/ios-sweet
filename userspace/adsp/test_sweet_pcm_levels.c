/* SPDX-License-Identifier: MIT */
#include "sweet_pcm_levels.h"
#include <assert.h>
#include <stdio.h>
int main(void)
{
 struct sweet_pcm_levels s={0};
 const uint8_t extreme[]={0,128,255,127,0,0,255,255};
 sweet_pcm_levels_add(&s,extreme,sizeof extreme);
 assert(s.frames==2 && s.nonzero_samples==3 && s.peak==32768);
 assert(s.squares==2147418114ULL);
 const uint8_t zero[1920]={0};sweet_pcm_levels_add(&s,zero,sizeof zero);
 assert(s.frames==482 && s.nonzero_samples==3 && s.peak==32768);
 puts("PASS: PCM level extrema, sign, silence and accumulation");
}
