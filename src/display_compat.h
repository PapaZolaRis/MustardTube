/* MustardTube display geometry helpers; no SDL or heap allocation needed.
 * SPDX-License-Identifier: GPL-3.0-only
 * Preserve 4:3 logical UI on Andromeda native modes. */
#ifndef MT_DISPLAY_COMPAT_H
#define MT_DISPLAY_COMPAT_H
#include <stdint.h>
typedef struct MTViewport { int x,y,w,h; } MTViewport;
static int mt_valid_display(int w,int h) {
    /* Bound dimensions to prevent bogus driver data or overflow. */
    return w>=320 && h>=240 && w<=7680 && h<=4320;
}
static MTViewport mt_viewport(int physical_w,int physical_h,int logical_w,int logical_h) {
    MTViewport r={0,0,logical_w,logical_h};
    if(!mt_valid_display(physical_w,physical_h)||logical_w<=0||logical_h<=0)return r;
    if((int64_t)physical_w*logical_h < (int64_t)physical_h*logical_w){
        r.w=physical_w;
        r.h=(int)((int64_t)physical_w*logical_h/logical_w);
    }else{
        r.h=physical_h;
        r.w=(int)((int64_t)physical_h*logical_w/logical_h);
    }
    r.x=(physical_w-r.w)/2;
    r.y=(physical_h-r.h)/2;
    return r;
}
static int mt_window_to_logical(MTViewport v,int mx,int my,int lw,int lh,int *x,int *y) {
    if(!x||!y||v.w<=0||v.h<=0||lw<=0||lh<=0)return 0;
    if(mx<v.x || my<v.y || mx>=v.x+v.w || my>=v.y+v.h)return 0;
    *x=(int)((int64_t)(mx-v.x)*lw/v.w);
    *y=(int)((int64_t)(my-v.y)*lh/v.h);
    if(*x>=lw)*x=lw-1;
    if(*y>=lh)*y=lh-1;
    return 1;
}
#endif
