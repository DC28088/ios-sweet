/* SPDX-License-Identifier: MIT */
#ifndef SWEET_PCM_RING_H
#define SWEET_PCM_RING_H
#ifdef KERNEL
#include <sys/types.h>
#include <sys/systm.h>
#else
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#endif
#define SWEET_PCM_RING_FRAMES 65536u
#define SWEET_PCM_FRAME_BYTES 4u
/* Caller serializes access. Drop oldest complete frames on overrun; neither
 * producer nor consumer waits. Only raw stereo signed16/48kHz frames. */
struct sweet_pcm_ring {
 uint8_t data[SWEET_PCM_RING_FRAMES*SWEET_PCM_FRAME_BYTES];
 uint32_t head,tail,count;
 uint64_t written,read,dropped;
};
static inline void sweet_pcm_ring_push(struct sweet_pcm_ring *r,const uint8_t *p,uint32_t n)
{
 r->written+=n;
 if(n>SWEET_PCM_RING_FRAMES){uint32_t skip=n-SWEET_PCM_RING_FRAMES;p+=(size_t)skip*4;r->dropped+=skip;n=SWEET_PCM_RING_FRAMES;}
 if(n>SWEET_PCM_RING_FRAMES-r->count){uint32_t skip=n-(SWEET_PCM_RING_FRAMES-r->count);r->tail=(r->tail+skip)%SWEET_PCM_RING_FRAMES;r->count-=skip;r->dropped+=skip;}
 uint32_t first=n<SWEET_PCM_RING_FRAMES-r->head?n:SWEET_PCM_RING_FRAMES-r->head;
 memcpy(r->data+(size_t)r->head*4,p,(size_t)first*4);
 memcpy(r->data,p+(size_t)first*4,(size_t)(n-first)*4);
 r->head=(r->head+n)%SWEET_PCM_RING_FRAMES;r->count+=n;
}
static inline uint32_t sweet_pcm_ring_pull(struct sweet_pcm_ring *r,uint8_t *p,uint32_t n)
{
 if(n>r->count)n=r->count;
 uint32_t first=n<SWEET_PCM_RING_FRAMES-r->tail?n:SWEET_PCM_RING_FRAMES-r->tail;
 memcpy(p,r->data+(size_t)r->tail*4,(size_t)first*4);
 memcpy(p+(size_t)first*4,r->data,(size_t)(n-first)*4);
 r->tail=(r->tail+n)%SWEET_PCM_RING_FRAMES;r->count-=n;r->read+=n;return n;
}
#endif
