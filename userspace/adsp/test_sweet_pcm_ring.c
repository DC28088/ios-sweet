/* SPDX-License-Identifier: MIT */
#include "sweet_pcm_ring.h"
#include <assert.h>
#include <stdio.h>
static uint32_t rnd=1;
static uint32_t random32(void){rnd=rnd*1664525u+1013904223u;return rnd;}
int main(void)
{
 struct sweet_pcm_ring r={0};uint32_t model[65536],input[80000],actual[80000],count=0,seq=0;uint64_t dropped=0,written=0,read=0;
 for(unsigned k=0;k<3000;k++){
  unsigned n=random32()%80000;
  if((random32()>>16)&1){
   for(unsigned i=0;i<n;i++)input[i]=seq++;
   unsigned skip=n>65536?n-65536:0,keep=n-skip;dropped+=skip;written+=n;
   unsigned discard=count+keep>65536?count+keep-65536:0;
   memmove(model,model+discard,(count-discard)*4);count-=discard;dropped+=discard;
   memcpy(model+count,input+skip,keep*4);count+=keep;
   sweet_pcm_ring_push(&r,(const uint8_t *)input,n);
  }else{
   unsigned want=n<count?n:count;unsigned got=sweet_pcm_ring_pull(&r,(uint8_t *)actual,n);
   assert(got==want && !memcmp(actual,model,want*4));memmove(model,model+want,(count-want)*4);count-=want;read+=want;
  }
  assert(r.count==count && r.head<65536 && r.tail<65536 && r.dropped==dropped && r.written==written && r.read==read);
 }
 unsigned n=sweet_pcm_ring_pull(&r,(uint8_t *)actual,80000);assert(n==count && !memcmp(actual,model,n*4) && !r.count);
 puts("PASS: real PCM ring preserves frames/order across wrap, empty/full states, large writes and 3000 overruns/reads");
}
