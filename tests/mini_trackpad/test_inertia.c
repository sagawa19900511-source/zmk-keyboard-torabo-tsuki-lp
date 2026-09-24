#include <assert.h>
#include <stdio.h>
#include "../../src/mini_scroll_inertia_core.h"
#include "../../snippets/input-split-listener/mini-trackpad-settings.h"
static const struct mini_inertia_config config = {
    MINI_TRACKPAD_INERTIA_TICK_MS, MINI_TRACKPAD_INERTIA_RELEASE_MS,
    MINI_TRACKPAD_INERTIA_DECAY_PERMILLE, MINI_TRACKPAD_INERTIA_BLEND_PERMILLE,
    MINI_TRACKPAD_INERTIA_MIN_SAMPLES, MINI_TRACKPAD_INERTIA_START_MILLI,
    MINI_TRACKPAD_INERTIA_STOP_MILLI, MINI_TRACKPAD_INERTIA_LIMIT_MILLI,
    MINI_TRACKPAD_INERTIA_MAX_MS
};
static int64_t flick(struct mini_inertia_state *s, int x, int y) {
    mini_cancel(s);
    for (int i=0; i<8; i++) {
        mini_input(s,&config,0,x,100+i*8);
        mini_input(s,&config,1,y,100+i*8);
    }
    return 156+config.release_ms;
}
int main(void) {
    struct mini_inertia_state s={0}; int16_t out[2]; int32_t decoded;
    int64_t t=flick(&s,12,-8);
    assert(!mini_tick(&s,&config,t-1,false,out));
    assert(mini_tick(&s,&config,t,false,out));
    assert(out[0]>0 && out[0]<=12 && out[1]<0 && out[1]>=-8);
    int32_t queued=mini_pack(s.generation,out[0]);
    assert(mini_unpack(&s,false,queued,&decoded) && decoded==out[0]);
    /* Mode entry discards old queued output. Exit must not revive it. */
    assert(!mini_tick(&s,&config,t+1,true,out));
    assert(!mini_unpack(&s,true,queued,&decoded));
    assert(!mini_unpack(&s,false,queued,&decoded));
    assert(!mini_tick(&s,&config,t+2,false,out));
    /* New same-direction, reverse, zero and button events each stop coasting. */
    for (int k=0;k<4;k++) {
        t=flick(&s,12,-8);assert(mini_tick(&s,&config,t,false,out));
        queued=mini_pack(s.generation,out[0]);
        mini_input(&s,&config,k==3?-1:0,k==0?4:k==1?-4:0,t+1);
        assert(!s.coasting && !mini_unpack(&s,false,queued,&decoded));
        assert(!mini_tick(&s,&config,t+1+config.release_ms,false,out));
    }
    /* Next complete gesture works after cancellation. Both axes decay and stop. */
    t=flick(&s,12,-8);int ticks=0;
    while (true) {
        int32_t prevx=mini_abs(s.velocity[0]),prevy=mini_abs(s.velocity[1]);
        if (!mini_tick(&s,&config,t+ticks*config.tick_ms,false,out))break;
        assert(out[0]>=0 && out[1]<=0);
        assert(mini_abs(s.velocity[0])<=prevx && mini_abs(s.velocity[1])<=prevy);
        assert(++ticks<=config.max_ms/config.tick_ms+1);
    }
    assert(ticks>1 && !s.coasting);
    /* Output scale is inherited: halve physical deltas => half tracked velocity. */
    struct mini_inertia_state a={0},b={0};
    flick(&a,20,-16);flick(&b,10,-8);
    assert(mini_abs(a.velocity[0]-2*b.velocity[0])<=2);
    assert(mini_abs(a.velocity[1]-2*b.velocity[1])<=2);
    /* A BLE arrival burst must not turn 1 ms packet spacing into 8x speed. */
    mini_cancel(&a);mini_cancel(&b);
    mini_input(&a,&config,0,0,0);mini_input(&b,&config,0,0,0);
    for (int frame=1;frame<=8;frame++) {
        mini_input(&a,&config,0,8,frame*8);
        mini_input(&b,&config,0,4,frame*8-1);
        mini_input(&b,&config,0,4,frame*8);
    }
    assert(a.velocity[0]==b.velocity[0]);
    /* Pure horizontal and pure vertical do not leak to the other axis. */
    t=flick(&s,-10,0);assert(mini_tick(&s,&config,t,false,out));assert(out[0]<0 && out[1]==0);
    t=flick(&s,0,10);assert(mini_tick(&s,&config,t,false,out));assert(out[0]==0 && out[1]>0);
    /* No launch from one noisy event; max-duration stop; bounded extreme deltas. */
    mini_cancel(&s);mini_input(&s,&config,0,1,0);
    assert(!mini_tick(&s,&config,config.release_ms,false,out));
    t=flick(&s,1000000,-1000000);assert(mini_tick(&s,&config,t,false,out));
    assert(mini_abs(out[0])<=127 && mini_abs(out[1])<=127);
    assert(!mini_tick(&s,&config,t+config.max_ms,false,out));
    /* Event guard survives generation wrap and negative packed values. */
    t=flick(&s,-12,8);assert(mini_tick(&s,&config,t,false,out));s.generation=65535;
    assert(mini_unpack(&s,false,mini_pack(65535,-7),&decoded) && decoded==-7);
    mini_cancel(&s);assert(s.generation==0);
    assert(!mini_unpack(&s,false,mini_pack(65535,-7),&decoded));
    puts("PASS: 2D velocity/decay, scale continuity, release timing, all cancellation paths, queue guard, timeout, bounds, generation wrap");
    return 0;
}
