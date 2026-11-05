#include <assert.h>
#include <stdio.h>
#include "../src/display_compat.h"
int main(void) {
    struct {int w,h;} modes[]={
        {640,480},{720,480},{720,720},{1024,768},{1280,720},{1920,1080},{320,240},{800,480},{720,576},
    };
    for(unsigned i=0;i<sizeof(modes)/sizeof(modes[0]);i++){
        MTViewport r=mt_viewport(modes[i].w,modes[i].h,640,480);
        assert(r.x>=0 && r.y>=0 && r.w<=modes[i].w && r.h<=modes[i].h);
        assert(r.w>0 && r.h>0);
        assert((int64_t)r.w*480 <= (int64_t)r.h*640+640);
        int x,y;
        assert(mt_window_to_logical(r,r.x+r.w/2,r.y+r.h/2,640,480,&x,&y));
        assert(x>=318 && x<=322 && y>=238 && y<=242);
        if(r.x>0)assert(!mt_window_to_logical(r,0,r.y,640,480,&x,&y));
        if(r.y>0)assert(!mt_window_to_logical(r,r.x,0,640,480,&x,&y));
        printf("%dx%d -> viewport %d,%d %dx%d\n",modes[i].w,modes[i].h,r.x,r.y,r.w,r.h);
    }
    assert(!mt_valid_display(200,100));
    assert(!mt_valid_display(99999,99999));
    puts("Display geometry tests PASS");
}
