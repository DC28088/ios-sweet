#!/usr/bin/env python3
"""Exercise actual patched FIFO transfer code against independent byte rings."""
from pathlib import Path
import subprocess,sys,tempfile
source=Path(sys.argv[1]).read_text()
a=source.index('static uint32_t tx_write(')
function=source[a:].replace('__asm__ volatile("dsb sy" ::: "memory");','/* host barrier stub */')
harness=r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t rx_len=64,tx_len=64,tail,rxhead,txhead,txtail;
static unsigned reads,writes,descriptors,publishes,ipcs,failread,failwrite;
static uint64_t rx_fifo=0x20000,tx_fifo=0x10000;
static uint8_t ring[16384],txring[16384];
static int g_io_error;
static unsigned g_ipc=24;
#define RX_TAIL 8
#define RX_HEAD 12
#define TX_TAIL 0
#define TX_HEAD 4
#define APCS_IPC 0x30000
static uint32_t rd32(uint64_t p){
 ++descriptors;
 if(p==RX_TAIL)return tail;
 if(p==RX_HEAD)return rxhead;
 if(p==TX_HEAD)return txhead;
 if(p==TX_TAIL)return txtail;
 assert(0);return 0;
}
static void wr32(uint64_t p,uint32_t v){
 assert(!g_io_error);
 if(p==RX_TAIL){tail=v;return;}
 if(p==TX_HEAD){++publishes;txhead=v;return;}
 if(p==APCS_IPC){++ipcs;assert(v==1u<<g_ipc);return;}
 assert(0);
}
static int pm_rd(uint64_t p,void *out,size_t n){
 ++reads;assert(!(p&3) && !(n&3) && n && p>=rx_fifo && p+n<=rx_fifo+rx_len);
 if(reads==failread)return -1;
 memcpy(out,ring+(size_t)(p-rx_fifo),n);return 0;
}
static int pm_wr(uint64_t p,const void *in,size_t n){
 ++writes;assert(!(p&3) && !(n&3) && n && p>=tx_fifo && p+n<=tx_fifo+tx_len);
 if(writes==failwrite)return -1;
 memcpy(txring+(size_t)(p-tx_fifo),in,n);return 0;
}
'''+function+r'''
static void reset(void){reads=writes=descriptors=publishes=ipcs=failread=failwrite=0;g_io_error=0;}
int main(void){
 for(unsigned i=0;i<sizeof ring;i++)ring[i]=(uint8_t)(i*71+19);
 for(tail=0;tail<64;tail++)for(unsigned off=0;off<64;off++)for(unsigned n=0;n<=64;n++){
  uint8_t out[66];memset(out,0xa5,sizeof out);reset();glink_rx_peek(out+1,off,n);
  unsigned start=(tail+off)%64;
  for(unsigned i=0;i<n;i++)assert(out[i+1]==ring[(start+i)%64]);
  assert(out[0]==0xa5 && out[n+1]==0xa5 && !g_io_error);
  assert(descriptors==1 && reads==(n?(start+n>64?2u:1u):0u));
 }
 rx_len=16384;
 for(unsigned k=0;k<2000;k++){
  unsigned off=(k*971)%rx_len,n=(k*631)%rx_len;tail=(k*719)%rx_len;
  uint8_t out[16386];memset(out,0xa5,sizeof out);reset();glink_rx_peek(out+1,off,n);
  for(unsigned i=0;i<n;i++)assert(out[i+1]==ring[(tail+off+i)%rx_len]);
  assert(out[0]==0xa5 && out[n+1]==0xa5 && reads<=(n+4095)/4096+2 && !g_io_error);
 }
 reset();tail=16380;failread=2;uint8_t out[8];memset(out,0xa5,sizeof out);glink_rx_peek(out,0,8);
 assert(g_io_error && !memcmp(out,ring+16380,4));for(unsigned i=4;i<8;i++)assert(!out[i]);
 unsigned before=descriptors;assert(!glink_rx_avail());glink_rx_advance(8);assert(tail==16380 && descriptors==before);
 for(unsigned h=0;h<64;h+=8)for(unsigned n=0;n<=32;n++){
  reset();txhead=txtail=h;memset(txring,0xa5,sizeof txring);
  uint8_t payload[32],expected[48]={0x34,0x12,0x78,0x56,0xba,0xdc,0xcd,0xab};
  for(unsigned i=0;i<n;i++)payload[i]=(uint8_t)(i*13+9);
  memcpy(expected+8,payload,n);unsigned padded=(8+n+7)&~7u;
  assert(!glink_send(0x1234,0x5678,0xabcddcba,payload,n));
  for(unsigned i=0;i<padded;i++)assert(txring[(h+i)%64]==expected[i]);
  for(unsigned i=padded;i<64;i++)assert(txring[(h+i)%64]==0xa5);
  assert(txhead==(h+padded)%64 && publishes==1 && ipcs==1 && writes==(h+padded>64?2u:1u));
 }
 for(unsigned f=1;f<=2;f++){
  reset();txhead=txtail=56;failwrite=f;uint8_t payload[16]={0};
  assert(glink_send(1,2,3,payload,16)==-1 && g_io_error && txhead==56 && !publishes && !ipcs);
  unsigned before=writes;assert(glink_send(1,2,3,payload,16)==-1 && writes==before);
 }
 puts("PASS: batched FIFO byte equivalence, alignment, small/large wrap, syscall bounds, TX publication and injected read/write failures");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(harness)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
