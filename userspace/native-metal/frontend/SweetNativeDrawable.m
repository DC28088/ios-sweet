/* SPDX-License-Identifier: MIT */
#import <Foundation/Foundation.h>
#include <IOSurface/IOSurface.h>
#include <stdio.h>
#include <string.h>
#include "redwood_drawable.h"
#include "sweet_native_ext.h"

uint32_t rwd_surface_id(void *surface) {return surface?IOSurfaceGetID((IOSurfaceRef)surface):0;}
int rwd_import_available(void) {return rwa_native_abi()==SWEET_NATIVE_METAL_ABI;}
struct rwa_texture *rwd_texture_for_surface(struct rwa_device *device,void *surface,unsigned long plane,
    const struct rwd_texture_shape *want,struct rwd_surface *took,char *why,size_t n)
{
    IOSurfaceRef s=(IOSurfaceRef)surface;
    if(took)memset(took,0,sizeof(*took));
#define REFUSE(...) do {snprintf(why,n,__VA_ARGS__);return NULL;} while(0)
    if(!device || !s || !want)REFUSE("IOSurface import requires a device, surface and descriptor");
    if(plane || IOSurfaceGetPlaneCount(s))REFUSE("interface: planar IOSurface textures are not implemented");
    size_t width=IOSurfaceGetWidth(s),height=IOSurfaceGetHeight(s),pitch=IOSurfaceGetBytesPerRow(s);
    unsigned bpp=0;uint32_t fourcc=0;
    switch(want->pixel_format) {
    case 10:fourcc='A008';bpp=1;break;
    case 70:case 71:fourcc='RGBA';bpp=4;break;
    case 80:case 81:fourcc='BGRA';bpp=4;break;
    case 115:fourcc='RGhA';bpp=8;break;
    default:REFUSE("interface: Metal pixel format %u is not a supported linear IOSurface format",want->pixel_format);
    }
    if(want->width!=width || want->height!=height || want->type!=2 || want->depth>1 ||
       want->array_length>1 || want->mip_levels>1 || want->sample_count>1)
        REFUSE("interface: IOSurface texture must match a single 2D plane, mip and sample");
    if(IOSurfaceGetPixelFormat(s)!=fourcc || width>UINT32_MAX || height>UINT32_MAX ||
       pitch>UINT32_MAX || width>SIZE_MAX/bpp || pitch<width*bpp || (pitch&63))
        REFUSE("interface: IOSurface format/row pitch does not match the A618 linear texture layout");
    uintptr_t pixels=(uintptr_t)IOSurfaceGetBaseAddress(s),base=pixels&~(uintptr_t)16383;
    size_t bytes=IOSurfaceGetAllocSize(s),offset=pixels-base;
    if(!base || offset>=bytes || (height && pitch>(bytes-offset)/height))
        REFUSE("interface: IOSurface rows extend beyond its mapped allocation");
    unsigned id=IOSurfaceGetID(s);
    if(!id)REFUSE("IOSurface has no shareable ID");
    // The winsys looks up this ID only inside this process, pins its actual CPU
    // mapping through ABI selector16 and retains the surface through GEM_CLOSE.
    struct rwa_texture *texture=rwa_texture_from_surface(device,(const struct rwa_texture_shape*)want,
                                                         id,(unsigned)offset,(unsigned)pitch,why,n);
    if(texture && took)*took=(struct rwd_surface){id,0,bytes,(unsigned)offset,(unsigned)pitch,
                                                (unsigned)width,(unsigned)height,fourcc,0};
    return texture;
#undef REFUSE
}
