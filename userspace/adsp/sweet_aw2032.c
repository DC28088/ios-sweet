/* SPDX-License-Identifier: MIT */
/* Functional implementation from the public Xiaomi/Awinic PID2032 register ABI:
 * MiCode/vendor_qcom_opensource_audio-kernel 65be95690af834887f876ca6d4a46df7a0ec50ac,
 * aw_device.c, aw_init.c, aw_pid_2032_reg.h and aw_bin_parse.c. No vendor profile
 * values are embedded here. This intentionally supports a bounded diagnostic,
 * not continuous playback without a supervising owner and fault monitoring. */
#include "sweet_aw2032.h"
#include <string.h>
static uint16_t g16(const uint8_t *p){return (uint16_t)p[0]|(uint16_t)p[1]<<8;}
static uint32_t g32(const uint8_t *p){return g16(p)|(uint32_t)g16(p+2)<<16;}
static unsigned crc8(const uint8_t *p,size_t n)
{
 unsigned c=0;for(size_t i=0;i<n;i++){unsigned v=p[i];for(unsigned j=0;j<8;j++){c=(c^v)&1?((c^0x18)>>1)|0x80:c>>1;v>>=1;}}return c;
}
/* Writable profile registers only. Excludes ID/reset, status and efuse/OTP. */
static int profile_reg(unsigned r)
{
 return (r>=3 && r<=15) || r==0x15 || r==0x20 || (r>=0x30 && r<=0x38) ||
        r==0x50 || (r>=0x52 && r<=0x56) || r==0x59 || r==0x5a || (r>=0x60 && r<=0x6c);
}
static int validate_profile(const struct sweet_aw_profile *p)
{
 if(!p || !p->records || !p->count || p->count>64)return -1;
 uint64_t seen[2]={0,0};unsigned mandatory=0;
 for(size_t i=0;i<p->count;i++){
  unsigned r=g16(p->records+4*i);
  uint16_t value=g16(p->records+4*i+2);
  if(r==5 && (value>>12)*48+((value>>6)&63)>720)return -1;
  if(!profile_reg(r) || (seen[r/64]&(1ull<<(r%64))))return -1;
  seen[r/64]|=1ull<<(r%64);
  if(r>=3 && r<=7)mandatory|=1u<<(r-3);
 }
 return mandatory==31?0:-1;
}
int sweet_aw_profile_select(const uint8_t *b,size_t n,unsigned channel,struct sweet_aw_profile *out)
{
 if(out)memset(out,0,sizeof(*out));
 if(!b || !out || channel>1 || n<80 || n>65536 || g32(b)!=0x0a15f908 || g32(b+64)!=1)return -1;
 uint32_t table=g32(b+60),count=g32(b+56),length=g32(b+52);
 if(table!=80 || !count || count>16 || length!=count*64 || length>n-table)return -1;
 struct sweet_aw_profile chosen={0,0};
 for(uint32_t i=0;i<count;i++){
  const uint8_t *e=b+table+64*i;size_t bytes=g32(e+32),at=g32(e+36);
  if(at<table+length || at>n || bytes>n-at || crc8(b+at,bytes)!=g32(e+40))return -1;
  for(uint32_t j=0;j<i;j++){
   const uint8_t *a=b+table+64*j;size_t other=g32(a+36),size=g32(a+32);
   if(at<other+size && other<at+bytes)return -1;
  }
  if(g32(e)!=2 || g16(e+20)!=channel || g16(e+26)!=0)continue;
  if(chosen.records || g32(e+28)!=4 || bytes<64)return -1;
  const uint8_t *d=b+at;uint32_t sum=0;for(size_t k=4;k<bytes;k++)sum+=d[k];
  if(sum!=g32(d) || g32(d+4)!=0x01000000 || g32(d+8)!=0 || g32(d+12)!=1 ||
     g32(d+16)!=bytes-60 || g32(d+32)!=2 || g32(d+36)!=2)return -1;
  uint32_t regs=g32(d+60);if(!regs || regs>64 || bytes!=64+regs*4)return -1;
  chosen.records=d+64;chosen.count=regs;if(validate_profile(&chosen))return -1;
 }
 if(!chosen.records)return -1;
 *out=chosen;return 0;
}
static unsigned vol_decode(uint16_t v){unsigned a=v>>6;return (a>>6)*48+(a&63);}
static uint16_t vol_encode(unsigned v){return (uint16_t)((((v/48)<<6)|(v%48))<<6);}
static int update(struct sweet_aw2032 *a,uint8_t r,uint16_t mask,uint16_t bits)
{
 uint16_t v;if(a->io.read(a->io.ctx,r,&v))return -1;
 return a->io.write(a->io.ctx,r,(v&~mask)|(bits&mask))?-1:0;
}
static int volume(struct sweet_aw2032 *a,unsigned v){return update(a,5,0xffc0,vol_encode(v));}
static void sleep_ms(struct sweet_aw2032 *a,unsigned n){a->io.sleep_ms(a->io.ctx,n);}
void sweet_aw2032_stop(struct sweet_aw2032 *a)
{
 if(!a || !a->io.reset)return;
 /* Best effort over I2C, then hardware reset even if the bus has failed. */
 if(a->verified && a->io.read && a->io.write && a->io.sleep_ms){
  (void)volume(a,720);(void)update(a,5,0x10,0x10);(void)update(a,7,1,0);
  (void)update(a,4,2,2);sleep_ms(a,1);(void)update(a,4,1,1);
 }
 (void)a->io.reset(a->io.ctx,1);a->active=a->prepared=a->verified=0;
}
int sweet_aw2032_prepare(struct sweet_aw2032 *a,const struct sweet_aw_io *io,const struct sweet_aw_profile *p)
{
 if(!a || !io || !io->read || !io->write || !io->reset || !io->sleep_ms || validate_profile(p))return -1;
 memset(a,0,sizeof(*a));a->io=*io;uint16_t id,iv,vv;
 if(io->reset(io->ctx,1))goto reset_only;
 sleep_ms(a,2);
 if(io->reset(io->ctx,0))goto reset_only;
 sleep_ms(a,20);
 if(io->read(io->ctx,0,&id) || id!=0x2032)goto reset_only;
 a->verified=1;
 if(update(a,4,3,3) || update(a,5,0x10,0x10) || volume(a,720))goto fail;
 for(size_t i=0;i<p->count;i++){
  unsigned r=g16(p->records+4*i);uint16_t v=g16(p->records+4*i+2);
  if(r==0x56)continue; /* device-specific voltage calibration, never profile replay */
  if(r==3)v=0xefbf; /* stock default mask; owner polls SYSST instead of enabling IRQs */
  if(r==4)v|=3; /* keep both power stages off during profile loading */
  if(r==5){a->profile_volume=(uint16_t)vol_decode(v);v=(v&0x3f)|0x10|vol_encode(720);}
  if(r==6)v=(v&~0xcfu)|8u; /* stock update_rate: 48kHz,16-bit; keep channel selection */
  if(r==7)v&=~1u; /* feedback off until startup */
  if(r==0x66)v|=0x4000; /* stock CCO mux bypass for 48kHz */
  if(io->write(io->ctx,(uint8_t)r,v))goto fail;
 }
 if(io->read(io->ctx,0x7a,&iv) || io->read(io->ctx,0x78,&vv))goto fail;
 int ical=iv&1023,vcal=vv&1023;if(ical&512)ical-=1024;if(vcal&512)vcal-=1024;
 int numerator=1000+ical,denominator=1000+vcal;
 if(numerator<=0 || denominator<=0)goto fail;
 uint32_t calibrated=(uint32_t)(8192u*(uint32_t)numerator/(uint32_t)denominator);
 if(calibrated>65535 || io->write(io->ctx,0x56,(uint16_t)calibrated))goto fail;
 a->prepared=1;return 0;
fail:sweet_aw2032_stop(a);return -1;
reset_only:(void)io->reset(io->ctx,1);return -1;
}
static int status_ok(struct sweet_aw2032 *a)
{
 uint16_t s;if(a->io.read(a->io.ctx,1,&s))return -1;a->last_status=s;
 /* Clock stable + PLL lock, without over-voltage/current/temp, under-voltage or missing clock. */
 if(s&0xcc2a)return -2;
 return (s&0x11)==0x11;
}
int sweet_aw2032_start(struct sweet_aw2032 *a,unsigned attenuation)
{
 if(!a || !a->prepared || a->active)return -1;
 if(attenuation<192)attenuation=192;
 if(attenuation<a->profile_volume)attenuation=a->profile_volume;
 if(attenuation>720)attenuation=720;
 if(update(a,7,1,1) || update(a,4,1,0))goto fail;
 sleep_ms(a,2);
 if(update(a,4,2,0))goto fail;
 sleep_ms(a,1);
 unsigned i;for(i=0;i<10;i++){int st=status_ok(a);if(st<0)goto fail;if(st==1)break;sleep_ms(a,2);}if(i==10)goto fail;
 uint16_t discard;if(a->io.read(a->io.ctx,2,&discard) || a->io.read(a->io.ctx,2,&discard))goto fail;
 if(update(a,5,0x10,0))goto fail;
 for(unsigned v=720;v>attenuation;){v=v-attenuation>48?v-48:attenuation;if(volume(a,v))goto fail;
 sleep_ms(a,1);}
 a->active=1;return 0;
fail:sweet_aw2032_stop(a);return -1;
}
int sweet_aw2032_poll(struct sweet_aw2032 *a)
{
 if(!a || !a->active)return -1;
 if(status_ok(a)==1)return 0;
 sweet_aw2032_stop(a);return -1;
}
