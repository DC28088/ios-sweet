/* SPDX-License-Identifier: MIT */
#include "sweet_aw2032.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void p16(uint8_t *p,unsigned v){p[0]=v;p[1]=v>>8;}
static void p32(uint8_t *p,unsigned v){p16(p,v);p16(p+2,v>>16);}
static unsigned crc8(const uint8_t *p,size_t n){unsigned c=0;for(size_t i=0;i<n;i++){unsigned v=p[i];for(unsigned j=0;j<8;j++){c=(c^v)&1?((c^0x18)>>1)|0x80:c>>1;v>>=1;}}return c;}
static size_t make_acf(uint8_t *b)
{
 memset(b,0,512);p32(b,0x0a15f908);p32(b+52,128);p32(b+56,2);p32(b+60,80);p32(b+64,1);
 const unsigned regs[]={3,4,5,6,7,0x56,0x66};
 for(unsigned c=0;c<2;c++){
  uint8_t *e=b+80+c*64,*d=b+208+c*92;p32(e,2);p16(e+20,c);p32(e+28,4);p32(e+32,92);p32(e+36,208+c*92);
  p32(d+4,0x01000000);p32(d+12,1);p32(d+16,32);p32(d+32,2);p32(d+36,2);p32(d+60,7);
  for(unsigned i=0;i<7;i++){p16(d+64+4*i,regs[i]);p16(d+66+4*i,regs[i]==0x56?0xdead:regs[i]==6?0x1c20:0);}
  unsigned sum=0;for(unsigned i=4;i<92;i++)sum+=d[i];p32(d,sum);p32(e+40,crc8(d,92));
 }
 return 392;
}
struct mock {uint16_t reg[128];unsigned calls,fail_at,writes,resets,asserted,wrong_id,no_clock,fault,unmuted;int ical,vcal;};
static int step(struct mock *m){return ++m->calls==m->fail_at;}
static int rd(void *p,uint8_t r,uint16_t *v){struct mock *m=p;if(step(m))return -1;if(r==1)*v=m->fault?0x19:m->no_clock?0:0x11;else *v=m->reg[r];return 0;}
static int wr(void *p,uint8_t r,uint16_t v)
{
 struct mock *m=p;if(step(m))return -1;assert(r<=0x6c);++m->writes;
 if(r==5 && (m->reg[5]&0x10) && !(v&0x10)){assert(!(m->reg[4]&3));assert(!m->no_clock && !m->fault);m->unmuted=1;}
 if(r!=4 && r!=5 && r!=7 && r!=3 && r!=0x56 && !m->unmuted)assert((m->reg[4]&3)==3);
 m->reg[r]=v;return 0;
}
static int reset(void *p,int asserted)
{
 struct mock *m=p;if(step(m))return -1;++m->resets;m->asserted=asserted;
 if(!asserted){memset(m->reg,0,sizeof m->reg);m->reg[0]=m->wrong_id?0x1852:0x2032;m->reg[4]=3;m->reg[5]=0x38;m->reg[0x7a]=(uint16_t)m->ical&1023;m->reg[0x78]=(uint16_t)m->vcal&1023;}
 return 0;
}
static void sleep_ms(void *p,unsigned n){(void)p;assert(n<=20);}
static struct sweet_aw_io io(struct mock *m){struct sweet_aw_io o={m,rd,wr,reset,sleep_ms};return o;}
static unsigned successful(const struct sweet_aw_profile *p)
{
 struct mock m={0};struct sweet_aw2032 a;struct sweet_aw_io o=io(&m);
 assert(!sweet_aw2032_prepare(&a,&o,p));assert(a.prepared && !a.active && (m.reg[4]&3)==3 && (m.reg[5]&0x10));
 assert((m.reg[6]&0xff)==8 && (m.reg[6]&0x1c00)==0x1c00); /* same 32FS as 48k/16-bit stereo AFE; RX/channel preserved */
 assert(m.reg[0x56]==8192 && m.reg[0x56]!=0xdead); /* per-chip, not copied profile calibration */
 assert(!sweet_aw2032_start(&a,0));assert(a.active && m.unmuted);assert((m.reg[5]>>6)>=0x100); /* clamped >=24dB */
 assert(!sweet_aw2032_poll(&a));unsigned calls=m.calls;sweet_aw2032_stop(&a);assert(m.asserted && !a.prepared && !a.active);return calls;
}
int main(int argc,char **argv)
{
 uint8_t b[512],bad[512];size_t n=make_acf(b);struct sweet_aw_profile p;
 for(unsigned c=0;c<2;c++){assert(!sweet_aw_profile_select(b,n,c,&p));assert(p.count==7);successful(&p);}
 assert(sweet_aw_profile_select(b,n,2,&p));
 for(size_t i=0;i<n;i++){assert(sweet_aw_profile_select(b,i,0,&p));}
 memcpy(bad,b,n);bad[300]^=1;assert(sweet_aw_profile_select(bad,n,0,&p));
 memcpy(bad,b,n);p32(bad+80+36,200);assert(sweet_aw_profile_select(bad,n,0,&p));
 assert(!sweet_aw_profile_select(b,n,0,&p));unsigned max=successful(&p);
 for(unsigned fail=1;fail<=max;fail++){
  struct mock m={.fail_at=fail};struct sweet_aw2032 a;struct sweet_aw_io o=io(&m);
  int ret=sweet_aw2032_prepare(&a,&o,&p);if(!ret)ret=sweet_aw2032_start(&a,192);if(!ret)ret=sweet_aw2032_poll(&a);
  assert(ret);assert(m.asserted && !a.prepared && !a.active);
 }
 struct mock m={.wrong_id=1};struct sweet_aw2032 a;struct sweet_aw_io o=io(&m);assert(sweet_aw2032_prepare(&a,&o,&p));assert(m.asserted && !m.writes);sweet_aw2032_stop(&a);assert(!m.writes);
 memset(&m,0,sizeof m);m.no_clock=1;o=io(&m);assert(!sweet_aw2032_prepare(&a,&o,&p));assert(sweet_aw2032_start(&a,192));assert(m.asserted && !m.unmuted);
 memset(&m,0,sizeof m);o=io(&m);assert(!sweet_aw2032_prepare(&a,&o,&p));assert(!sweet_aw2032_start(&a,192));m.fault=1;assert(sweet_aw2032_poll(&a));assert(m.asserted && !a.active);
 memset(&m,0,sizeof m);m.ical=-512;m.vcal=511;o=io(&m);
 assert(!sweet_aw2032_prepare(&a,&o,&p));assert(m.reg[0x56]==8192u*488u/1511u);sweet_aw2032_stop(&a);
 struct sweet_aw_profile invalid={p.records,65};memset(&m,0,sizeof m);o=io(&m);assert(sweet_aw2032_prepare(&a,&o,&invalid));assert(!m.calls);
 if(argc==2){FILE *f=fopen(argv[1],"rb");assert(f);uint8_t real[65536];size_t bytes=fread(real,1,sizeof real,f);assert(!ferror(f) && feof(f));fclose(f);
  for(unsigned c=0;c<2;c++){assert(!sweet_aw_profile_select(real,bytes,c,&p));successful(&p);printf("Private vendor channel %u parsed and mock-started (%zu registers)\n",c,p.count);}
 }
 puts("PASS: ACF bounds/checksums/channel selection, muted profile load, calibration, clock/fault gates, and every I/O failure resets the amp");
}
