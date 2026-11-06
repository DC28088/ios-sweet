#!/usr/bin/env python3
"""Execute the patched FIFO reader against an independent byte-ring model."""
from pathlib import Path
import subprocess,sys,tempfile
source=Path(sys.argv[1]).read_text()
a=source.index('void glink_rx_peek(');b=source.index('\nvoid glink_rx_advance',a)
function=source[a:b]
harness=r'''
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <stdio.h>
static uint32_t rx_len=64,tail,calls;
static uint64_t rx_fifo=0x1000;
static uint8_t ring[64];
#define RX_TAIL 8
static uint32_t rd32(uint64_t p){
 ++calls;if(p==RX_TAIL)return tail;
 assert(p>=rx_fifo && p+4<=rx_fifo+rx_len && !(p&3));
 unsigned i=(unsigned)(p-rx_fifo);
 return (uint32_t)ring[i]|(uint32_t)ring[i+1]<<8|(uint32_t)ring[i+2]<<16|(uint32_t)ring[i+3]<<24;
}
'''+function+r'''
int main(void){
 for(unsigned i=0;i<64;i++)ring[i]=(uint8_t)(i*71+19);
 for(tail=0;tail<64;tail++)for(unsigned off=0;off<64;off++)for(unsigned n=0;n<=64;n++){
  uint8_t out[66];for(unsigned i=0;i<66;i++)out[i]=0xa5;
  calls=0;glink_rx_peek(out+1,off,n);
  unsigned start=(tail+off)%64;
  for(unsigned i=0;i<n;i++)assert(out[i+1]==ring[(start+i)%64]);
  assert(out[0]==0xa5 && out[n+1]==0xa5);
  assert(calls==1+(n?((start&3)+n+3)/4:0));
 }
 puts("PASS: 266240 FIFO peeks, byte equivalence, wrap, alignment, guards and word-read count");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(harness)
 subprocess.run(['cc','-std=c99','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
