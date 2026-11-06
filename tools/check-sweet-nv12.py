#!/usr/bin/env python3
"""Check actual NV12 conversion against independent floating-point colour equations."""
from pathlib import Path
import subprocess,tempfile,sys
root=Path(__file__).resolve().parents[1]
source=r'''
#include "SweetNV12.h"
#include <assert.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
static int reference(double x){int n=(int)floor(x+0.5);return n<0?0:n>255?255:n;}
int main(void){
 uint8_t data[128],out[5*3*4+16];memset(data,0xcc,sizeof data);
 struct sweet_nv12 s={data+8,data+64,8,8,5,3,3,2,(uintptr_t)data,sizeof data};
 const double kr[]={.299,.2126,.2627},kb[]={.114,.0722,.0593};
 for(unsigned matrix=1;matrix<=3;matrix++)for(int video=0;video<=1;video++){
  for(unsigned y=0;y<3;y++)for(unsigned x=0;x<5;x++)data[8+y*8+x]=(uint8_t)(x*53+y*29);
  for(unsigned y=0;y<2;y++)for(unsigned x=0;x<3;x++){data[64+y*8+x*2]=(uint8_t)(20+x*73);data[65+y*8+x*2]=(uint8_t)(245-y*90-x*43);}
  memset(out,0xa5,sizeof out);assert(sweet_nv12_convert(&s,5,3,video,matrix,out,60));
  for(unsigned y=0;y<3;y++)for(unsigned x=0;x<5;x++){
   double Y=(data[8+y*8+x]-(video?16.:0.))*(video?255./219.:1.);
   double U=(data[64+(y/2)*8+(x/2)*2]-128.)*(video?255./224.:1.);
   double V=(data[65+(y/2)*8+(x/2)*2]-128.)*(video?255./224.:1.);
   double r=kr[matrix-1],b=kb[matrix-1],g=1-r-b;
   int want[]={reference(Y+2*(1-r)*V),reference(Y-2*b*(1-b)/g*U-2*r*(1-r)/g*V),reference(Y+2*(1-b)*U)};
   for(unsigned c=0;c<3;c++)assert(abs((int)out[(y*5+x)*4+c]-want[c])<=1);
   assert(out[(y*5+x)*4+3]==255);
  }
  for(unsigned i=60;i<sizeof out;i++)assert(out[i]==0xa5);
 }
 memset(data+8,16,24);memset(data+64,128,16);assert(sweet_nv12_convert(&s,5,3,1,1,out,60));assert(out[0]==0&&out[1]==0&&out[2]==0);
 memset(data+8,235,24);assert(sweet_nv12_convert(&s,5,3,1,1,out,60));assert(out[0]==255&&out[1]==255&&out[2]==255);
 memset(out,0xa5,sizeof out);s.uvrow=5;assert(!sweet_nv12_convert(&s,5,3,0,1,out,60));s.uvrow=8;
 s.bytes=77;assert(!sweet_nv12_convert(&s,5,3,0,1,out,60));s.bytes=128;
 s.uvh=1;assert(!sweet_nv12_convert(&s,5,3,0,1,out,60));s.uvh=2;
 assert(!sweet_nv12_convert(&s,5,3,0,0,out,60));assert(!sweet_nv12_convert(&s,SIZE_MAX,3,0,1,out,60));assert(!sweet_nv12_convert(&s,5,3,0,1,out,59));
 for(unsigned i=0;i<sizeof out;i++)assert(out[i]==0xa5);
 puts("PASS: NV12 ranges, BT.601/709/2020 colour, odd sizes, padding, bounds and overflow");
}
'''
source=source.replace('#include <math.h>','#include <math.h>\n#include <stdlib.h>')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(source)
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror',*(['-fsanitize=address,undefined'] if '--sanitize' in sys.argv else []),'-I'+str(root/'userspace/native-metal/frontend'),str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
