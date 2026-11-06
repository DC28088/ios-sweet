/* SPDX-License-Identifier: MIT */
- (BOOL)nativeRefreshPlanar {
    if(!_nativePlanar)return YES;
    IOSurfaceRef s=(IOSurfaceRef)_nativeSurface;
    if(!s || IOSurfaceGetPlaneCount(s)!=2)return NO;
    uint32_t seed=IOSurfaceGetSeed(s);
    if(_nativePlanarValid && seed==_nativePlanarSeed)return YES;
    const size_t w=_shape.width,h=_shape.height;
    if(!w || !h || w>SIZE_MAX/4 || h>(64u<<20)/(w*4))return NO;
    size_t bytes=w*h*4;
    if(bytes!=_nativePlanarBytes) {
        void *p=realloc(_nativePlanarPixels,bytes);
        if(!p)return NO;
        _nativePlanarPixels=p;_nativePlanarBytes=bytes;
    }
    if(IOSurfaceLock(s,kIOSurfaceLockReadOnly,NULL)!=0)return NO;
    struct sweet_nv12 view={
      IOSurfaceGetBaseAddressOfPlane(s,0),IOSurfaceGetBaseAddressOfPlane(s,1),
      IOSurfaceGetBytesPerRowOfPlane(s,0),IOSurfaceGetBytesPerRowOfPlane(s,1),
      IOSurfaceGetWidthOfPlane(s,0),IOSurfaceGetHeightOfPlane(s,0),
      IOSurfaceGetWidthOfPlane(s,1),IOSurfaceGetHeightOfPlane(s,1),
      (uintptr_t)IOSurfaceGetBaseAddress(s)&~(uintptr_t)16383,IOSurfaceGetAllocSize(s)
    };
    unsigned matrix=1; /* documented BT.601 fallback only when metadata absent */
    if(&kIOSurfaceYCbCrMatrix && kIOSurfaceYCbCrMatrix) {
        CFTypeRef value=IOSurfaceCopyValue(s,kIOSurfaceYCbCrMatrix);
        if(value) {
            if(CFGetTypeID(value)==CFStringGetTypeID()) {
                if(CFEqual(value,CFSTR("ITU_R_709_2")))matrix=2;
                else if(CFEqual(value,CFSTR("ITU_R_2020")))matrix=3;
                else if(!CFEqual(value,CFSTR("ITU_R_601_4")))matrix=0;
            }
            CFRelease(value);
        }
    }
    BOOL converted=IOSurfaceGetBytesPerElementOfPlane(s,0)==1 &&
      IOSurfaceGetBytesPerElementOfPlane(s,1)==2 &&
      sweet_nv12_convert(&view,w,h,IOSurfaceGetPixelFormat(s)=='420v',matrix,
          _nativePlanarPixels,_nativePlanarBytes);
    seed=IOSurfaceGetSeed(s);
    IOSurfaceUnlock(s,kIOSurfaceLockReadOnly,NULL);
    if(!converted)return NO;
    unsigned origin[3]={0,0,0},size[3]={(unsigned)w,(unsigned)h,1};char why[256]={0};
    if(mesa.texture_write(_texture,0,0,origin,size,_nativePlanarPixels,w*4,why,sizeof why)) {
        static unsigned failures;if(failures++<8)rw_print("combined YUV upload failed: %s\n",why);return NO;
    }
    _nativePlanarSeed=seed;_nativePlanarValid=YES;
    return YES;
}
- (void)nativeEnablePlanar {_nativePlanar=YES;_nativePlanarValid=NO;}

- (struct rwa_texture *)nativeRenderTexture {
    if(_nativePlanar){static unsigned refused;if(refused++<8)rw_print("combined YUV GPU writeback is not supported\n");return NULL;}
    return _texture;
}
