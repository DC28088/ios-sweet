/* SPDX-License-Identifier: MIT */
#include "sweet_pcm.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void p32(uint8_t *p,uint32_t x){for(unsigned i=0;i<4;i++)p[i]=x>>(8*i);}
static uint32_t g32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static size_t reply(uint8_t *b,const struct sweet_apr_msg *m,uint32_t op,const uint8_t *p,unsigned len)
{
 size_t n=sweet_apr_pack(b,128,APR_MSG_TYPE_CMD_RSP,m->dest_svc,m->dest_port,m->src_port,m->token,op,p,len);
 b[5]=APR_DOMAIN_ADSP;b[9]=APR_DOMAIN_APPS;return n;
}
static void setup(struct sweet_pcm *s,int fallback)
{
 uint8_t b[128],r[128],p[8];struct sweet_apr_msg m;
 static const uint32_t expected[]={0x10d92,0x10db3,0x10326,0x10325,0x10d98,0x10daa};
 for(unsigned i=0;i<6;i++){
  size_t n=sweet_pcm_next(s,b,sizeof(b),100);assert(n && !sweet_apr_parse(b,n,&m));assert(m.opcode==expected[i]);if(i==3)assert((g32(m.payload+12)&0xffff)==3);
  assert(!sweet_pcm_next(s,r,sizeof(r),100));memset(p,0,sizeof(p));uint32_t op=APR_BASIC_RSP_RESULT;
  if(i==0){assert(m.payload_len==20 && g32(m.payload+8)==0x90000000 && g32(m.payload+12)==1 && g32(m.payload+16)==32768);p32(p,77);op=0x10d93;}
  else if(i==2){assert(m.payload_len==28 && m.dest_port==0x1000 && m.src_port==0x1000 && m.token==0x280000 && g32(m.payload+16)==48000);p32(p+4,0xb0d60003u);op=0x10329;}
  else {p32(p,m.opcode);if(i==1)assert(g32(m.payload+12)==0x10da5);if(i==4)assert(g32(m.payload)==20 && g32(m.payload+8)==48000);}
  if(i==2 && fallback){
   uint8_t failed[8];p32(failed,m.opcode);p32(failed+4,1);
   size_t rn=reply(r,&m,APR_BASIC_RSP_RESULT,failed,8);assert(sweet_pcm_receive(s,r,rn)==1 && !s->waiting && s->adm_default);
   n=sweet_pcm_next(s,b,sizeof b,100);assert(n && !sweet_apr_parse(b,n,&m));
   assert(m.opcode==0x10326 && m.token==0x280001 && g32(m.payload+8)==0x10314);
  }
  n=reply(r,&m,op,p,i==0?4:8);assert(sweet_pcm_receive(s,r,n)==1);
 }
}
int main(void)
{
 struct sweet_pcm s;uint8_t b[128],r[128],p[16];struct sweet_apr_msg m;
 sweet_pcm_init(&s,0x190000000ull);setup(&s,0);assert(s.mapped && s.copp_open && s.stream_open && s.copp==3);
 unsigned live=0;struct sweet_apr_msg queue[SWEET_PCM_SLOTS];uint8_t saved[SWEET_PCM_SLOTS][128];unsigned most=0;
 while(s.completed<100){
  while(live<SWEET_PCM_SLOTS && s.sent<100){size_t n=sweet_pcm_next(&s,saved[live],128,200);assert(n && !sweet_apr_parse(saved[live],n,&queue[live]));assert(queue[live].opcode==0x10dab && g32(queue[live].payload+12)==1920);++live;}
  assert(!sweet_pcm_next(&s,b,128,200) || s.sent==100);if(live>most)most=live;assert(live);m=queue[--live];memcpy(p,m.payload,12);p32(p+12,0);size_t n=reply(r,&m,0x10d99,p,16);
  assert(sweet_pcm_receive(&s,r,n)==1);assert(sweet_pcm_receive(&s,r,n)==0); /* stale completion cannot free slot twice */
 }
 assert(s.sent==100 && !s.inflight && most==SWEET_PCM_SLOTS); /* all slots queue at once */
 const uint32_t end[]={0x10bd3,0x10bcd,0x10327,0x10d94};
 for(unsigned i=0;i<4;i++){size_t n=sweet_pcm_next(&s,b,128,300);assert(n && !sweet_apr_parse(b,n,&m) && m.opcode==end[i]);if(i==2)assert(m.dest_port==3);p32(p,m.opcode);p32(p+4,0);n=reply(r,&m,APR_BASIC_RSP_RESULT,p,8);assert(sweet_pcm_receive(&s,r,n)==1);}
 assert(!sweet_pcm_next(&s,b,128,300));assert(s.done && !s.failed && !s.mapped && !s.stream_open && !s.copp_open);
 sweet_pcm_init(&s,1);assert(s.done && s.failed);
 sweet_pcm_init(&s,0x190000000ull);setup(&s,0);assert(sweet_pcm_next(&s,b,128,100));sweet_pcm_poll(&s,3101);assert(s.failed && !s.done);assert(sweet_pcm_next(&s,b,128,3101));sweet_pcm_poll(&s,6202);assert(s.done && s.failed && s.mapped); /* no unsafe unmap/free after missing pause */
 sweet_pcm_init(&s,0x190000000ull);assert(!sweet_pcm_next(&s,b,1,100));assert(!s.waiting && !s.mapped);
 size_t n=sweet_pcm_next(&s,b,128,100);assert(n && !sweet_apr_parse(b,n,&m));p32(p,m.opcode);p32(p+4,0);n=reply(r,&m,APR_BASIC_RSP_RESULT,p,8);assert(sweet_pcm_receive(&s,r,n)==0 && s.waiting); /* need handle response */
 r[9]=APR_DOMAIN_ADSP;assert(sweet_pcm_receive(&s,r,n)==0);r[9]=APR_DOMAIN_APPS;r[12]^=1;assert(sweet_pcm_receive(&s,r,n)==0);
 sweet_pcm_init(&s,0x190000000ull);setup(&s,1);assert(s.adm_default && s.stream_open && s.copp_open && !s.status);
 for(unsigned invalid=0;invalid<2;invalid++){
  sweet_pcm_init(&s,0x190000000ull);
  for(unsigned i=0;i<3;i++){
   n=sweet_pcm_next(&s,b,128,100);assert(n && !sweet_apr_parse(b,n,&m));memset(p,0,sizeof p);
   uint32_t op=APR_BASIC_RSP_RESULT;unsigned bytes=8;
   if(i==0){p32(p,77);op=0x10d93;bytes=4;}
   else if(i==1)p32(p,m.opcode);
   else {p32(p+4,0xb0d60000u|(invalid?255u:0u));op=0x10329;}
   n=reply(r,&m,op,p,bytes);int accepted=sweet_pcm_receive(&s,r,n);
   if(i==2 && invalid){assert(accepted==1 && !s.copp_open && s.adm_default);}
   else assert(accepted==1);
  }
  if(!invalid)assert(s.copp_open && s.copp==0 && !s.failed && !s.adm_default);
 }
 sweet_pcm_init(&s,0x190000000ull);assert(s.limit==100);
 assert(sweet_pcm_set_limit(&s,0)==-1 && sweet_pcm_set_limit(&s,10001)==-1);
 assert(!sweet_pcm_set_limit(&s,3));setup(&s,0);assert(sweet_pcm_set_limit(&s,4)==-1);
 for(unsigned i=0;i<3;i++){
  n=sweet_pcm_next(&s,b,128,200);assert(n && !sweet_apr_parse(b,n,&m) && m.opcode==0x10dab);
  memcpy(p,m.payload,12);p32(p+12,0);n=reply(r,&m,0x10d99,p,16);assert(sweet_pcm_receive(&s,r,n)==1);
 }
 n=sweet_pcm_next(&s,b,128,200);assert(n && !sweet_apr_parse(b,n,&m) && m.opcode==0x10bd3 && s.sent==3);
 sweet_pcm_init(&s,0x190000000ull);assert(!sweet_pcm_set_limit(&s,10000));setup(&s,0);
 for(unsigned i=0;i<10000;i++){
  n=sweet_pcm_next(&s,b,128,200);assert(n && !sweet_apr_parse(b,n,&m));
  memcpy(p,m.payload,12);p32(p+12,0);n=reply(r,&m,0x10d99,p,16);assert(sweet_pcm_receive(&s,r,n)==1);
 }
 assert(s.completed==10000 && !s.failed);
 sweet_pcm_init(&s,0x190000000ull);assert(!sweet_pcm_set_limit(&s,SWEET_PCM_STREAM_LIMIT));setup(&s,0);
 s.sent=s.completed=16383;
 for(unsigned i=0;i<5;i++){
  n=sweet_pcm_next(&s,b,128,200);assert(n && !sweet_apr_parse(b,n,&m));
  memcpy(p,m.payload,12);p32(p+12,0);n=reply(r,&m,0x10d99,p,16);assert(sweet_pcm_receive(&s,r,n)==1);
 }
 assert(s.completed==16388 && !s.failed); /* crosses old 16-bit token filter */
 s.sent=s.completed=SWEET_PCM_STREAM_LIMIT-1;
 n=sweet_pcm_next(&s,b,128,200);assert(n && !sweet_apr_parse(b,n,&m));
 memcpy(p,m.payload,12);p32(p+12,0);n=reply(r,&m,0x10d99,p,16);assert(sweet_pcm_receive(&s,r,n)==1);
 n=sweet_pcm_next(&s,b,128,200);assert(n && !sweet_apr_parse(b,n,&m) && m.opcode==0x10bd3);
 puts("PASS: PCM ABI vectors, 100 periods, slot ownership, stale replies, ordered teardown, timeout retention and bad packets");
}
