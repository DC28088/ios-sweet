/* SPDX-License-Identifier: MIT */
#import <Foundation/Foundation.h>
#include <IOSurface/IOSurfaceRef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
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
    {   /* 26b9c: every IOSurface texture request, first 48 per process (wallpaper / snapshot formats unknown) */
        static unsigned told_req;
        if(IOSurfaceGetPixelFormat(s)!=0x26424741u && told_req++<48)fprintf(stderr,"SweetNativeMetal: IOSurface texture request: surface %u %#x planes %zu plane %lu Metal format %u %ux%u type %u usage %#x\n",
            IOSurfaceGetID(s),IOSurfaceGetPixelFormat(s),IOSurfaceGetPlaneCount(s),plane,want->pixel_format,
            want->width,want->height,want->type,want->usage);
    }
    /*
     * iOS 26 backboardd asks for its 828x1792 render targets as '&BGA' (lossless-compressed BGRA, 0x26424741),
     * one plane plus 16 KiB of compression metadata (26b5d: "planar IOSurface textures are not implemented" on
     * every frame, dark screen). Nothing here compresses: Sweet's IOSurface lays plane 0 out linearly at the
     * surface base with the surface's own row pitch, and the panel scans it as BGRA. Such a surface is imported
     * as linear BGRA; any other plane layout is still refused, and the first refusals say why.
     */
    uint32_t surface_format=IOSurfaceGetPixelFormat(s);
    size_t surface_planes=IOSurfaceGetPlaneCount(s);
    uintptr_t plane0_base=0;    /* nonzero: the pixels are plane 0's, at this address */
    /*
     * iOS 26 wallpapers and photos (26b6g: "planar IOSurface textures are not implemented (0x34323066, 2 planes)"):
     * 8-bit bi-planar YCbCr, '420f'/'420v' -- plane 0 luma, one byte per texel (Metal R8Unorm), plane 1 CbCr at
     * half size, two bytes per texel (RG8Unorm). Both planes are linear in the surface's one allocation, so a
     * plane is imported like any other surface whose rows start at that plane's base, with that plane's pitch.
     */
    size_t plane_width=0,plane_height=0,plane_pitch=0;
    struct rwd_texture_shape ycbcr_shape;   /* the caller's descriptor with a YCbCr plane's own format */
    size_t ycbcr_shadow_offset=0,ycbcr_shadow_bytes=0;   /* nonzero bytes: the planes live in a shadow BO */
    bool ycbcr_convert=false;   /* 26bB/26bC: format 500 read through an RGB RGBA8 shadow */
    if(surface_planes==2 && (surface_format==0x34323066u || surface_format==0x34323076u)) {
        if(plane>1)REFUSE("interface: plane %lu of a 2-plane IOSurface",plane);
        uintptr_t pb=(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s,plane),b0=(uintptr_t)IOSurfaceGetBaseAddress(s);
        size_t bpe=IOSurfaceGetBytesPerElementOfPlane(s,plane);
        plane_width=IOSurfaceGetWidthOfPlane(s,plane);plane_height=IOSurfaceGetHeightOfPlane(s,plane);
        plane_pitch=IOSurfaceGetBytesPerRowOfPlane(s,plane);
        static unsigned told_yuv;
        if(told_yuv++<32) {
            /* 26b9d: is there a picture in it? Mean of a sparse sample of this plane's bytes (luma ~16 or chroma
             * ~128 everywhere = an empty/black decode upstream, not a GPU problem). */
            unsigned long long sum=0,n=0;unsigned lo=255,hi=0;
            const uint8_t *px=(const uint8_t *)pb;
            if(px && plane_width && plane_height && IOSurfaceLock(s,kIOSurfaceLockReadOnly,NULL)==0) {
                for(size_t y=0;y<plane_height;y+=17)for(size_t x=0;x<plane_width*bpe;x+=13) {
                    unsigned v=px[y*plane_pitch+x];sum+=v;n++;if(v<lo)lo=v;if(v>hi)hi=v;
                }
                IOSurfaceUnlock(s,kIOSurfaceLockReadOnly,NULL);
            }
            fprintf(stderr,"SweetNativeMetal: YCbCr IOSurface %u %#x plane %lu: offset %ld bpe %zu bpr %zu %zux%zu Metal format %u usage %#x; bytes mean %llu min %u max %u (%llu samples)\n",
                IOSurfaceGetID(s),surface_format,plane,(long)(pb-b0),bpe,plane_pitch,plane_width,plane_height,
                want->pixel_format,want->usage,n?sum/n:0,lo,hi,n);
        }
        if(!pb || pb<b0 || (bpe!=1 && bpe!=2) || bpe!=(plane?2u:1u))
            REFUSE("interface: YCbCr IOSurface plane %lu is not an 8-bit linear plane (bpe %zu)",plane,bpe);
        /* 26b8: the wallpaper's planes have 592-byte rows. A618 samples linear rows at 64-byte pitches only, so
         * such a surface is read through one shadow BO holding both planes, laid out by transport/sweet_drm.cpp
         * ycbcr_shadow_layout(): plane 0 at its in-page offset from the allocation's first 16 KiB page, plane 1
         * at the next 4 KiB after plane 0's rounded rows, every row rounded to 64 bytes. */
        size_t r0=IOSurfaceGetBytesPerRowOfPlane(s,0),r1=IOSurfaceGetBytesPerRowOfPlane(s,1);
        if((r0|r1)&63) {
            uintptr_t first=(uintptr_t)IOSurfaceGetBaseAddress(s)&~(uintptr_t)16383;
            size_t a0=(r0+63)&~(size_t)63,a1=(r1+63)&~(size_t)63;
            size_t off0=(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s,0)-first;
            size_t off1=(off0+a0*IOSurfaceGetHeightOfPlane(s,0)+4095)&~(size_t)4095;
            ycbcr_shadow_offset=plane?off1:off0;
            ycbcr_shadow_bytes=(off1+a1*IOSurfaceGetHeightOfPlane(s,1)+16383)&~(size_t)16383;
        }
        if(want->usage&(2|4))
            REFUSE("interface: a written YCbCr IOSurface plane (usage %#x)",want->usage);
        /* 26bB/26bC: format 500 is ONE texture over both planes, and CoreAnimation draws it with its plain RGB image
         * path (no pipeline has fc_has_ycbcr_matrix=1): a sample must be RGB, as Apple's sampler converts. Plane 0
         * alone as R8 answered (Y, 0, 0) -> red snapshots; (Cr, Y, Cb) -> green. Such a texture is an RGBA8 shadow the
         * transport fills with RGB (transport/sweet_drm.cpp ycbcr_convert_layout, shadow_refill);
         * /var/local/sweet-native-ycbcr-raw restores R8. */
        static int convert_on=-1;
        if(convert_on<0) {
            convert_on=access("/var/local/sweet-native-ycbcr-raw",F_OK)!=0;
            fprintf(stderr,"SweetNativeMetal: Metal format 500 textures %s\n",convert_on?
                "read through an RGB RGBA8 shadow (/var/local/sweet-native-ycbcr-raw restores plane 0 as R8)":
                "are plane 0 as R8 (raw)");
        }
        if(convert_on && want->pixel_format==500 && plane==0) {
            ycbcr_convert=true;
            ycbcr_shape=*want;ycbcr_shape.pixel_format=70;want=&ycbcr_shape;   /* RGBA8Unorm */
            plane_pitch=(plane_width*4+63)&~(size_t)63;
            ycbcr_shadow_offset=0;
            ycbcr_shadow_bytes=(plane_pitch*plane_height+16383)&~(size_t)16383;
            surface_format='RGBA';
        } else {
            surface_format=plane?'LA08':'A008';   /* compared with the Metal format below: RG8 / R8 */
            /* 26b7b: a plane view of format 500 is R8 (plane 0) or RG8 (plane 1), the uber shader's two-texture
             * YCbCr path (image function 26: luma .r of one texture, CbCr .rg of another). */
            if(want->pixel_format==500) {
                ycbcr_shape=*want;
                ycbcr_shape.pixel_format=plane?30:10;
                want=&ycbcr_shape;
            }
        }
        plane0_base=pb;
        surface_planes=0;
        plane=0;
    }
    if(plane)REFUSE("interface: planar IOSurface textures are not implemented (plane %lu asked)",plane);
    if(surface_planes) {
        uintptr_t p0=(uintptr_t)IOSurfaceGetBaseAddressOfPlane(s,0),b0=(uintptr_t)IOSurfaceGetBaseAddress(s);
        size_t r0=IOSurfaceGetBytesPerRowOfPlane(s,0);
        static unsigned told;
        if(told++<4)fprintf(stderr,"SweetNativeMetal: IOSurface %u %#x planes %zu: plane0 offset %ld bpr %zu (surface bpr %zu) %zux%zu\n",
            IOSurfaceGetID(s),surface_format,surface_planes,(long)(p0-b0),r0,IOSurfaceGetBytesPerRow(s),
            IOSurfaceGetWidthOfPlane(s,0),IOSurfaceGetHeightOfPlane(s,0));
        if(surface_planes!=1 || surface_format!=0x26424741u || !p0 || p0<b0 || r0!=IOSurfaceGetBytesPerRow(s) ||
           IOSurfaceGetWidthOfPlane(s,0)!=IOSurfaceGetWidth(s) || IOSurfaceGetHeightOfPlane(s,0)!=IOSurfaceGetHeight(s))
            REFUSE("interface: planar IOSurface textures are not implemented (%#x, %zu planes)",surface_format,surface_planes);
        surface_format='BGRA';     /* single linear plane: '&BGA' read and written as BGRA */
        plane0_base=p0;            /* metadata may precede the pixels: rows start at plane 0 */
    }
    size_t width=IOSurfaceGetWidth(s),height=IOSurfaceGetHeight(s),pitch=IOSurfaceGetBytesPerRow(s);
    if(plane_width) {width=plane_width;height=plane_height;pitch=plane_pitch;}   /* a YCbCr plane */
    unsigned bpp=0;uint32_t fourcc=0;
    switch(want->pixel_format) {
    case 1:case 10:fourcc='A008';bpp=1;break;
    case 30:fourcc='LA08';bpp=2;break;
    case 70:case 71:fourcc='RGBA';bpp=4;break;
    case 80:case 81:fourcc='BGRA';bpp=4;break;
    case 115:fourcc='RGhA';bpp=8;break;
    default:REFUSE("interface: Metal format %u / IOSurface %#x (%zux%zu pitch %zu) is not a supported linear import",want->pixel_format,IOSurfaceGetPixelFormat(s),width,height,pitch);
    }
    if(want->width!=width || want->height!=height || want->type!=2 || want->depth>1 ||
       want->array_length>1 || want->mip_levels>1 || want->sample_count>1)
        REFUSE("interface: IOSurface texture must match a single 2D plane, mip and sample");
    if(surface_format!=fourcc || width>UINT32_MAX || height>UINT32_MAX ||
       pitch>UINT32_MAX-63 || width>SIZE_MAX/bpp || pitch<width*bpp)
        REFUSE("interface: IOSurface %#x for Metal %u expected %#x, %zux%zu pitch %zu; incompatible A618 linear layout",IOSurfaceGetPixelFormat(s),want->pixel_format,fourcc,width,height,pitch);
    // A618 samples linear rows only at 64-byte pitches. A surface another
    // process laid out with a shorter row (SpringBoard's LA08 glyphs ask for
    // 210/224 bytes) is read through a shadow at the rounded pitch, which the
    // transport refills from the surface whenever its seed changes (the same
    // rounding rule is in sweet_drm.cpp surface_description). Only for
    // textures the GPU reads: a GPU write would not reach the surface.
    size_t gpu_pitch=pitch;
    if(pitch&63) {
        if(want->usage&(2|4))
            REFUSE("interface: IOSurface %u %zux%zu pitch %zu is not 64-byte aligned and the texture is written (usage %#x); a read-only shadow cannot carry GPU writes back",
                   IOSurfaceGetID(s),width,height,pitch,want->usage);
        gpu_pitch=(pitch+63)&~(size_t)63;
    }
    /* base = the allocation's first page, as transport/sweet_drm.cpp surface_description pins it; offset =
     * where the rows start inside it (plane 0 for '&BGA'). */
    uintptr_t pixels=plane0_base?plane0_base:(uintptr_t)IOSurfaceGetBaseAddress(s),
              base=(uintptr_t)IOSurfaceGetBaseAddress(s)&~(uintptr_t)16383;
    size_t bytes=IOSurfaceGetAllocSize(s),offset=pixels-base;
    if(ycbcr_shadow_bytes){bytes=ycbcr_shadow_bytes;offset=ycbcr_shadow_offset;}
    /* the shadow BO of a YCbCr surface holds rows at gpu_pitch; any other surface's own rows are at pitch (a
     * glyph shadow is sized by the transport, not by this allocation: 26b9b refused those with gpu_pitch here) */
    const size_t row_span=ycbcr_shadow_bytes?gpu_pitch:pitch;
    if(!base || offset>=bytes || (height && row_span>(bytes-offset)/height))
        REFUSE("interface: IOSurface rows extend beyond its mapped allocation");
    unsigned id=IOSurfaceGetID(s);
    if(!id)REFUSE("IOSurface has no shareable ID");
    if(ycbcr_convert) {
        if(id&0x40000000u)REFUSE("interface: IOSurface ID %u collides with the YCbCr conversion flag",id);
        id|=0x40000000u;   /* transport/sweet_drm.cpp YCbCrConvertFlag */
    }
    // The winsys looks up this ID only inside this process, pins its actual CPU
    // mapping through ABI selector16 and retains the surface through GEM_CLOSE.
    struct rwa_texture *texture=rwa_texture_from_surface(device,(const struct rwa_texture_shape*)want,
                                                         id,(unsigned)offset,(unsigned)gpu_pitch,why,n);
    static unsigned shadow_imports;
    if(texture && gpu_pitch!=pitch && shadow_imports++<16)
        fprintf(stderr,"SweetNativeMetal: IOSurface %u %#x %zux%zu pitch %zu read through a %zu-byte-row shadow\n",
                id,fourcc,width,height,pitch,gpu_pitch);
    static unsigned rgha_imports;
    if(texture && want->pixel_format==115 && rgha_imports++<8)
        fprintf(stderr,"SweetNativeMetal: RGBA16F IOSurface %u imported, %zux%zu pitch %zu usage %#x\n",id,width,height,pitch,want->usage);
    static unsigned rg8_imports;
    if(texture && want->pixel_format==30 && rg8_imports++<4)
        fprintf(stderr,"SweetNativeMetal: RG8 IOSurface %u imported, %zux%zu pitch %zu\n",id,width,height,pitch);
    if(texture && took)*took=(struct rwd_surface){id,0,bytes,(unsigned)offset,(unsigned)gpu_pitch,
                                                (unsigned)width,(unsigned)height,fourcc,0};
    return texture;
#undef REFUSE
}

/* 26b9b: CoreAnimation reaches a YCbCr plane through a texture view of the Metal format 500 texture it made from
 * the surface ("a texture view is not implemented" -> chroma missing, wallpaper and app snapshots wrong). The
 * device frontend asks this for the plane's size and imports that plane again as its own texture. */
int rwd_ycbcr_plane_size(void *surface,unsigned long plane,unsigned *width,unsigned *height)
{
    IOSurfaceRef s=(IOSurfaceRef)surface;
    if(!s || IOSurfaceGetPlaneCount(s)!=2 || plane>1)return 0;
    uint32_t f=IOSurfaceGetPixelFormat(s);
    if(f!=0x34323066u && f!=0x34323076u)return 0;
    *width=(unsigned)IOSurfaceGetWidthOfPlane(s,plane);
    *height=(unsigned)IOSurfaceGetHeightOfPlane(s,plane);
    return 1;
}
