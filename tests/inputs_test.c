#include "opensimgear/inputs.h"
#include <assert.h>
#include <stdint.h>
static void turn(osg_encoder_t *e, int direction) {
    const uint8_t cw[] = {2,0,1,3}, ccw[] = {1,0,2,3};
    for (unsigned i=0;i<4;i++) osg_encoder_sample(e, direction>0 ? cw[i] : ccw[i]);
}
int main(void) {
    osg_contact_t c; osg_contact_init(&c, true, 0);
    assert(!osg_contact_update(&c, true, 9)); assert(osg_contact_update(&c, true, 10));
    assert(osg_contact_update(&c, false, 20)); assert(osg_contact_update(&c, true, 21));
    assert(osg_contact_update(&c, false, 22)); assert(!osg_contact_update(&c, false, 32));
    const uint8_t compass[] = {1,3,2,6,4,12,8,9};
    for(unsigned i=0;i<8;i++) assert(osg_hat_position(compass[i]) == i);
    assert(osg_hat_position(0)==8 && osg_hat_position(5)==8 && osg_hat_position(10)==8 && osg_hat_position(15)==8);
    assert(osg_axis_scale(0,0,4095)==0 && osg_axis_scale(4095,0,4095)==65535);
    assert(osg_axis_scale(100,100,4000)==0 && osg_axis_scale(5000,100,4000)==65535);
    assert(osg_axis_scale(2048,0,4095)>32760 && osg_axis_scale(2048,0,4095)<32790);
    assert(osg_axis_scale(100,100,100)==32768);
    osg_encoder_t e; osg_encoder_init(&e,3);
    /* A bounced edge cancels, and a complete cycle emits exactly one event. */
    osg_encoder_sample(&e,2); osg_encoder_sample(&e,3); assert(osg_encoder_output(&e,0)==0);
    turn(&e,1); turn(&e,-1); assert(osg_encoder_output(&e,0)==1);
    assert(osg_encoder_output(&e,1000)==1); /* Host stalled: don't drain. */
    osg_encoder_reported(&e,1,1000); assert(osg_encoder_output(&e,1024)==1);
    assert(osg_encoder_output(&e,1025)==0); assert(osg_encoder_output(&e,2000)==0);
    osg_encoder_reported(&e,0,2000); assert(osg_encoder_output(&e,2024)==0);
    assert(osg_encoder_output(&e,2025)==-1);
    osg_encoder_rearm(&e,3); assert(osg_encoder_output(&e,3000)==0);
    osg_encoder_sample(&e,0); assert(e.invalid==1); osg_encoder_sample(&e,3);
    assert(osg_encoder_output(&e,3001)==0);
    for(unsigned i=0;i<20;i++) turn(&e,1);
    assert(e.dropped==4); osg_encoder_rearm(&e,2); /* Half-detent recovery is discarded. */
    osg_encoder_sample(&e,0); osg_encoder_sample(&e,1); osg_encoder_sample(&e,3);
    assert(osg_encoder_output(&e,4000)==0); turn(&e,1); assert(osg_encoder_output(&e,4001)==1);
    osg_encoder_reported(&e,1,UINT32_MAX-10); assert(osg_encoder_output(&e,15)==0);
    return 0;
}
