/* SPDX-License-Identifier: MIT
 * Checked CPU fallback for combined 8-bit NV12 textures. Inspired by the
 * ios-flame sunfish frontend's CPU fallback; no source pixels are modified.
 */
#ifndef SWEET_NV12_H
#define SWEET_NV12_H
#include <stdint.h>
#include <stddef.h>
struct sweet_nv12 {
    const uint8_t *y, *uv;
    size_t yrow, uvrow, yw, yh, uvw, uvh;
    uintptr_t allocation;
    size_t bytes;
};
static inline int sweet_nv12_range(const struct sweet_nv12 *s, const uint8_t *p,
    size_t row, size_t width, size_t height) {
    uintptr_t a=(uintptr_t)p;
    if(!a || a<s->allocation || !width || !height || row<width)return 0;
    size_t off=a-s->allocation;
    return off<s->bytes && width<=s->bytes-off &&
        height-1<=(s->bytes-off-width)/row;
}
static inline uint8_t sweet_nv12_byte(int x) {return (uint8_t)(x<0?0:x>255?255:x);}
/* matrix: 1 BT.601, 2 BT.709, 3 BT.2020 non-constant luminance.
 * Coefficients are 16.16, including the video-range scale when requested. */
static inline int sweet_nv12_convert(const struct sweet_nv12 *s,size_t w,size_t h,
    int video,unsigned matrix,uint8_t *dst,size_t length) {
    if(!s || !dst || !w || !h || matrix<1 || matrix>3 || w>SIZE_MAX/4 ||
       h>length/(w*4) || w>s->yw || h>s->yh ||
       (w/2+w%2)>s->uvw || (h/2+h%2)>s->uvh ||
       !sweet_nv12_range(s,s->y,s->yrow,w,h) ||
       !sweet_nv12_range(s,s->uv,s->uvrow,(w/2+w%2)*2,h/2+h%2))return 0;
    static const int c[3][2][5]={
      {{65536,91881,22553,46802,116130},{76309,104597,25675,53279,132201}},
      {{65536,103206,12276,30679,121609},{76309,117489,13975,34925,138438}},
      {{65536,96639,10784,37444,123299},{76309,110014,12277,42626,140363}}
    };
    const int *k=c[matrix-1][video?1:0];
    for(size_t y=0;y<h;y++) {
        const uint8_t *ly=s->y+y*s->yrow,*uv=s->uv+(y/2)*s->uvrow;
        uint8_t *out=dst+y*w*4;
        for(size_t x=0;x<w;x++,out+=4) {
            int Y=((int)ly[x]-(video?16:0))*k[0];
            int U=(int)uv[(x/2)*2]-128,V=(int)uv[(x/2)*2+1]-128;
            out[0]=sweet_nv12_byte((Y+k[1]*V+32768)>>16);
            out[1]=sweet_nv12_byte((Y-k[2]*U-k[3]*V+32768)>>16);
            out[2]=sweet_nv12_byte((Y+k[4]*U+32768)>>16);
            out[3]=255;
        }
    }
    return 1;
}
#endif
