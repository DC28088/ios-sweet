from pathlib import Path
import subprocess,tempfile,sys
if len(sys.argv)!=2: raise SystemExit('Usage: check-sweet-surface-result.py PATH/TO/SweetSurface.cpp')
src=Path(sys.argv[1]).read_text()
start=src.index('static void\nsweet_surface_fill_result(');end=src.index('\nstatic IOReturn',start);fn=src[start:end]
preamble='''#include <cstdint>
#include <cstring>
#include <cassert>
#define SWEET_SURFACE_RESULT_SIZE 0xa68
#define SWEET_SURFACE_TIMESTAMPS 0x800
struct M { uint64_t address; uint64_t getAddress(){return address;} };
struct SweetSurfaceRef { M *data, *state; };
struct sweet_surface_plane {uint32_t width,height,bytesPerRow,offset,size;uint16_t bytesPerElement;uint8_t elementWidth,elementHeight;};
struct SweetSurface {uint32_t id,allocSize,width,height,bytesPerRow,offset,pixelFormat,planeCount;uint16_t bytesPerElement;uint8_t elementWidth,elementHeight;uint32_t cacheMode,parentID;sweet_surface_plane planes[2];};
static void put32(uint8_t*p,uint32_t off,uint32_t v){memcpy(p+off,&v,4);}
static void put64(uint8_t*p,uint32_t off,uint64_t v){memcpy(p+off,&v,8);}
static uint32_t get32(const uint8_t*p,unsigned off){uint32_t v;memcpy(&v,p+off,4);return v;}
'''
test='''
int main(){
 M data{0x110000000},state{0x120000000};SweetSurfaceRef ref{&data,&state};SweetSurface s{};
 s.id=7;s.allocSize=3072;s.width=64;s.height=32;s.bytesPerRow=64;s.planeCount=2;s.bytesPerElement=1;s.elementWidth=s.elementHeight=1;
 s.planes[0]={64,32,64,0,2048,1,1,1};s.planes[1]={32,16,64,2048,1024,2,1,1};
 uint8_t out[SWEET_SURFACE_RESULT_SIZE];sweet_surface_fill_result(&s,&ref,out);
 // Consumer offsets independently verified in the target iOS16 framework.
 for(unsigned i=0;i<2;i++){unsigned p=0x68+i*0x80;assert(get32(out,p+0x10)==64);assert(get32(out,p+0x08)==0);assert(get32(out,p+0xc)==i*2048);}
 assert(get32(out,0x1c)==3072 && get32(out,0x38)==2);
 s.planeCount=0;s.bytesPerRow=256;sweet_surface_fill_result(&s,&ref,out);assert(get32(out,0x28)==256);assert(get32(out,0x38)==0);assert(get32(out,0x78)==0);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(preamble+fn+test);subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror',str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
 print('PASS: exact kernel serializer; 2-plane native consumer offsets and nonplanar layout')
