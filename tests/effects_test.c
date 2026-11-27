/* SPDX-License-Identifier: MIT */
#include "recomp_input_effects.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static unsigned byte_oracle(int v, unsigned pct) {
    unsigned m=(unsigned)(v<0?-v:v), t=32767u*(pct>100?100:pct)/100u;
    if(m>32767)m=32767;
    if(m<=t || t==32767)return 128;
    m=(m-t)*127/(32767-t);
    return v<0?127-m:128+m;
}
static unsigned pointer_oracle(int v, unsigned pct) {
    int t=(int)(32767u*(pct>100?100:pct)/100u), r;
    if(t<1)t=1;
    if(v>-t && v<t)v=0;
    r=128+v/256;
    return (unsigned)(r<0?0:r>255?255:r);
}
static int half_oracle(int v, unsigned pct) {
    int dz=(int)((pct>90?90:pct)*32768u/100u);
    if(dz<=0)return v;
    if(v<=dz)return 0;
    return (int)((long long)(v-dz)*32768/(32768-dz));
}
typedef struct { uint32_t device, duration; uint16_t low,high; } command;
static command commands[256];
static unsigned count;
static void emit(void *user, uint32_t device, uint16_t low,uint16_t high,uint32_t duration) {
    CHECK(user==commands); CHECK(count<256);
    commands[count].device=device;commands[count].low=low;
    commands[count].high=high;commands[count++].duration=duration;
}
static void tick(ri_haptics*c,uint32_t frame,uint64_t ms) {ri_haptics_tick(c,frame,ms,emit,commands);}
static void frame_profile(void) {
    ri_haptic_profile profile={100,0};
    ri_haptics*c=ri_haptics_create(8,profile);
    uint16_t low,high;
    CHECK(c);count=0;
    ri_haptics_route(c,0,11);tick(c,0,0);CHECK(count==0);
    ri_haptics_set(c,0,0,200*257,0,RI_HAPTIC_HELD,0,0);
    ri_haptics_set(c,0,1,150*257,65535,RI_HAPTIC_FRAME32,10,8);
    tick(c,10,0);CHECK(count==1 && commands[0].device==11 && commands[0].low==200*257 && commands[0].high==65535 && commands[0].duration==100);
    tick(c,17,0);CHECK(count==2 && commands[1].high==65535);
    tick(c,18,0);CHECK(count==3 && commands[2].high==0 && commands[2].low==200*257);
    ri_haptics_set(c,0,0,0,0,RI_HAPTIC_HELD,0,0);tick(c,19,0);
    CHECK(count==4 && commands[3].duration==0);tick(c,20,0);CHECK(count==4);
    ri_haptics_set(c,0,1,65535,0,RI_HAPTIC_FRAME32,0xfffffffeu,8);
    ri_haptics_values(c,0,1,0,&low,&high);CHECK(low==65535 && high==0);
    ri_haptics_values(c,0,6,0,&low,&high);CHECK(low==0 && high==0);
    ri_haptics_set(c,0,0,99,66,RI_HAPTIC_HELD,0,0);tick(c,7,0);
    ri_haptics_route(c,0,22);tick(c,8,0);
    CHECK(commands[count-2].device==11 && commands[count-2].duration==0);
    CHECK(commands[count-1].device==22 && commands[count-1].duration==100);
    ri_haptics_route(c,1,22);ri_haptics_set(c,1,0,200,0,RI_HAPTIC_HELD,0,0);
    tick(c,9,0);CHECK(commands[count-1].low==200 && commands[count-1].high==66);
    ri_haptics_release_device(c,22,emit,commands);CHECK(commands[count-1].duration==0);
    {unsigned previous=count;tick(c,10,0);CHECK(count==previous);}
    ri_haptics_route(c,2,22);{unsigned previous=count;tick(c,11,0);CHECK(count==previous);}
    ri_haptics_destroy(c);
}
static void millisecond_profile(void) {
    ri_haptic_profile profile={240,160};
    ri_haptics*c=ri_haptics_create(4,profile);unsigned previous;
    CHECK(c);count=0;ri_haptics_route(c,0,1);
    ri_haptics_set(c,0,0,65535,65535,RI_HAPTIC_MILLISECONDS,1000,120);
    tick(c,0,1000);CHECK(count==1 && commands[0].duration==240);
    tick(c,0,1119);CHECK(count==1);
    tick(c,0,1120);CHECK(count==2 && commands[1].duration==0);
    ri_haptics_set(c,0,0,65535,65535,RI_HAPTIC_MILLISECONDS,1200,120);
    tick(c,0,1200);CHECK(count==3);
    ri_haptics_set(c,0,0,65535,65535,RI_HAPTIC_MILLISECONDS,1300,120);
    tick(c,0,1359);CHECK(count==3);tick(c,0,1360);CHECK(count==4);
    ri_haptics_set(c,0,0,65535,65535,RI_HAPTIC_HELD,0,0);
    tick(c,0,1519);CHECK(count==4);tick(c,0,1520);CHECK(count==5);
    ri_haptics_release_device(c,1,emit,commands);CHECK(count==6 && commands[5].duration==0);
    previous=count;ri_haptics_release_device(c,1,emit,commands);CHECK(count==previous);
    ri_haptics_destroy(c);
}
int main(void) {
    int v;unsigned pct;
    /* Frozen adapter arithmetic at every signed endpoint/percent, including
     * the extra positive endpoint produced by pointer-Y inversion. */
    for(pct=0;pct<=101;++pct)for(v=-32768;v<=32768;++v) {
        CHECK(ri_axis_u8(v,pct,RI_AXIS_U8_RESCALED_127)==byte_oracle(v,pct));
        CHECK(ri_axis_u8(v,pct,RI_AXIS_U8_CLIPPED_256)==pointer_oracle(v,pct));
        if(v>=0)CHECK(ri_axis_half(v,pct)==half_oracle(v,pct));
    }
    frame_profile();millisecond_profile();
    puts("axis profiles and haptic routing/lifetimes passed");return 0;
}
