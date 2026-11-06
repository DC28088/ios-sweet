#!/usr/bin/env python3
"""Exercise the real frontend and transport layout checks with synthetic IOSurfaces."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
front=(root/'userspace/native-metal/frontend/SweetNativeDrawable.m').read_text()
front=front[front.index('uint32_t rwd_surface_id'):]
s=(root/'userspace/native-metal/transport/sweet_drm.cpp').read_text()
transport=s[s.index('int surface_description('):s.index('\nint allocate(',s.index('int surface_description('))]
pre=r'''
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <cassert>
#define SWEET_NATIVE_METAL_ABI 0x53574e01u
struct MockSurface {
 uintptr_t base=0x100000000;size_t bytes=360448,width=640,height=360,pitch=984,planes=2;
 uint32_t format=0x34323076;size_t offsets[2]={64,235584},rows[2]={640,640};
 bool region=true;unsigned refs=1;
} surface;
using IOSurfaceRef=MockSurface*;
unsigned IOSurfaceGetID(IOSurfaceRef){return 7;}
size_t IOSurfaceGetPlaneCount(IOSurfaceRef s){return s->planes;}
uint32_t IOSurfaceGetPixelFormat(IOSurfaceRef s){return s->format;}
size_t IOSurfaceGetWidth(IOSurfaceRef s){return s->width;}
size_t IOSurfaceGetHeight(IOSurfaceRef s){return s->height;}
size_t IOSurfaceGetBytesPerRow(IOSurfaceRef s){return s->pitch;}
size_t IOSurfaceGetBytesPerElementOfPlane(IOSurfaceRef,size_t i){return i?2:1;}
size_t IOSurfaceGetWidthOfPlane(IOSurfaceRef,size_t i){return i?320:640;}
size_t IOSurfaceGetHeightOfPlane(IOSurfaceRef,size_t i){return i?180:360;}
size_t IOSurfaceGetBytesPerRowOfPlane(IOSurfaceRef s,size_t i){return s->rows[i];}
void *IOSurfaceGetBaseAddress(IOSurfaceRef s){return (void*)s->base;}
void *IOSurfaceGetBaseAddressOfPlane(IOSurfaceRef s,size_t i){return (void*)(s->base+s->offsets[i]);}
size_t IOSurfaceGetAllocSize(IOSurfaceRef s){return s->bytes;}
IOSurfaceRef IOSurfaceLookup(unsigned id){if(id!=7)return nullptr;++surface.refs;return &surface;}
void CFRelease(IOSurfaceRef s){assert(s->refs>1);--s->refs;}
struct rwa_device{};struct rwa_texture{};
struct rwd_texture_shape{unsigned pixel_format,type,width,height,depth,array_length,mip_levels,sample_count,usage;};
struct rwa_texture_shape{unsigned pixel_format,type,width,height,depth,array_length,mip_levels,sample_count,usage;};
struct rwd_surface{uint32_t id,handle;uint64_t bytes;uint32_t offset,pitch,width,height,format,planes;};
unsigned rwa_native_abi(){return SWEET_NATIVE_METAL_ABI;}
unsigned imported_id,imported_offset,imported_pitch;
rwa_texture texture;
rwa_texture *rwa_texture_from_surface(rwa_device*,const rwa_texture_shape*,unsigned id,unsigned offset,unsigned pitch,char*,size_t){imported_id=id;imported_offset=offset;imported_pitch=pitch;return &texture;}
using vm_address_t=uint64_t;using vm_size_t=uint64_t;using mach_port_t=unsigned;using mach_msg_type_number_t=unsigned;using kern_return_t=int;using vm_region_info_t=int*;
struct vm_region_basic_info_data_64_t{int protection;};
constexpr unsigned VM_REGION_BASIC_INFO_COUNT_64=1,VM_REGION_BASIC_INFO_64=1,MACH_PORT_NULL=0,VM_PROT_READ=1,VM_PROT_WRITE=2;
unsigned mach_task_self(){return 1;}void mach_port_deallocate(unsigned,unsigned){}
int vm_region_64(unsigned,vm_address_t *address,vm_size_t *size,unsigned,vm_region_info_t p,unsigned*,unsigned*){if(!surface.region)return 1;*address=surface.base;*size=(surface.bytes+16383)&~size_t(16383);((vm_region_basic_info_data_64_t*)p)->protection=3;return 0;}
constexpr uint64_t HostPage=16384;
uint64_t rounded(uint64_t n){return (n+16383)&~uint64_t(16383);}
struct {struct {uint64_t max_bo_bytes=1u<<28;}params;}state;
struct redwood_drm_surface{uint32_t surface,handle;uint64_t size;uint32_t offset,row_bytes,width,height,pixel_format,planes;};
#include <cerrno>
'''
test=r'''
int main(){
 rwa_device dev;char why[256];rwd_surface took{};
 rwd_texture_shape y{10,2,640,360,1,1,1,1,1},uv{30,2,320,180,1,1,1,1,1};
 assert(rwd_texture_for_surface(&dev,&surface,0,&y,&took,why,sizeof why));
 assert(imported_id==7&&imported_offset==64&&imported_pitch==640&&took.planes==2);
 assert(rwd_texture_for_surface(&dev,&surface,1,&uv,&took,why,sizeof why));
 assert(imported_id==7&&imported_offset==235584&&imported_pitch==640);
 assert(!rwd_texture_for_surface(&dev,&surface,2,&uv,&took,why,sizeof why));
 assert(!rwd_texture_for_surface(&dev,&surface,1,&y,&took,why,sizeof why));
 redwood_drm_surface desc{};assert(!surface_description(7,&desc)&&desc.size==360448&&desc.planes==2);
 surface.rows[1]=641;assert(!rwd_texture_for_surface(&dev,&surface,1,&uv,&took,why,sizeof why));assert(surface_description(7,&desc));
 surface.rows[1]=640;surface.offsets[1]=360440;assert(!rwd_texture_for_surface(&dev,&surface,1,&uv,&took,why,sizeof why));assert(surface_description(7,&desc));
 surface.offsets[1]=235584;surface.region=false;assert(surface_description(7,&desc));surface.region=true;
 surface.format=0x78343230;assert(!rwd_texture_for_surface(&dev,&surface,0,&y,&took,why,sizeof why));assert(surface_description(7,&desc));
 surface.format=0x34323066;assert(rwd_texture_for_surface(&dev,&surface,1,&uv,&took,why,sizeof why));assert(!surface_description(7,&desc));
 surface.planes=0;surface.width=surface.height=16;surface.pitch=64;surface.format=0x42475241;
 rwd_texture_shape bgra{80,2,16,16,1,1,1,1,1};assert(rwd_texture_for_surface(&dev,&surface,0,&bgra,&took,why,sizeof why));assert(imported_offset==0&&imported_pitch==64);assert(!surface_description(7,&desc)&&!desc.planes);
 assert(surface.refs==1);puts("PASS: real NV12 frontend and transport checks, both planes, offsets, rejection cases, retention and BGRA");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(pre+front+transport+test)
 subprocess.run(['c++','-std=c++17','-Wall','-Wextra','-Werror','-Wno-multichar',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
