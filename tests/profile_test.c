#include "opensimgear/profile.h"
#include "opensimgear/tca9555.h"
#include <assert.h>
#include <string.h>

static bool pressed(const uint8_t *report,unsigned button) { return (report[button/8]&(1U<<(button%8)))!=0; }
static void descriptor_fields(const osg_profile_t *profile) {
    uint8_t bytes[256]; osg_profile_layout_t layout;
    assert(osg_profile_validate(profile,&layout)==OSG_PROFILE_OK);
    size_t length=osg_profile_descriptor(profile,bytes,sizeof bytes);
    unsigned page=0,usage=0,size=0,count=0,bits=0,buttons=0,hats=0,axes=0;
    for(size_t i=0;i<length;) {
        uint8_t tag=bytes[i++]; unsigned n=tag&3; n=n==3?4:n; assert(i+n<=length);
        uint32_t value=0; for(unsigned j=0;j<n;j++) value|=(uint32_t)bytes[i++]<<(8*j);
        switch(tag&0xfc) {
            case 0x04:page=value;break; case 0x08:usage=value;break;
            case 0x74:size=value;break; case 0x94:count=value;break;
            case 0x80:
                bits+=size*count;
                if(!(value&1)) { if(page==9) buttons+=count; else if(usage==0x39) hats+=count; else axes+=count; }
                usage=0;break;
        }
    }
    assert(bits==8U*layout.length && buttons==layout.buttons && hats==layout.hats && axes==layout.axes);
}
static unsigned axis(const osg_profile_state_t *state,const uint8_t *report,unsigned index) {
    unsigned offset=state->layout.axis_offset+2*index; return report[offset]|report[offset+1]<<8;
}
static void validation(void) {
    osg_profile_button_t buttons[128];
    for(unsigned i=0;i<128;i++) buttons[i]=(osg_profile_button_t){(uint8_t)(24+i),false,0};
    osg_profile_axis_t axes[8];
    for(unsigned i=0;i<8;i++) axes[i]=(osg_profile_axis_t){(uint8_t)i,(uint8_t)(0x30+i),0,4095,32768};
    osg_profile_hat_t hats[4]={{{0,1,2,3}},{{4,5,6,7}},{{8,9,10,11}},{{12,13,14,15}}};
    osg_profile_t profile={.name="capacity",.buttons=buttons,.button_count=128,
        .axes=axes,.axis_count=8,.hats=hats,.hat_count=4,.expander_mask=0xff};
    osg_profile_layout_t layout;
    assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_OK);
    assert(layout.buttons==128 && layout.length==34 && layout.adc_mask==0xff && layout.digital_mask==0xffff);
    uint8_t descriptor[256]; memset(descriptor,0xa5,sizeof descriptor);
    assert(osg_profile_descriptor(&profile,descriptor,4)==0 && descriptor[0]==0xa5);
    assert(osg_profile_descriptor(&profile,descriptor,sizeof descriptor)>0);
    descriptor_fields(&profile);
    buttons[1].input=buttons[0].input; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_OVERLAP);
    buttons[1].input=25; profile.expander_mask=1; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_INVALID);
    profile.expander_mask=0xff; axes[1].channel=0; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_OVERLAP);
    axes[1].channel=1; axes[1].usage=axes[0].usage; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_OVERLAP);
    axes[1].usage=0x31; axes[1].maximum=0; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_INVALID);
    axes[1].maximum=4095; profile.button_count=129; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_TOO_LARGE);
    profile=(osg_profile_t){.name="empty"}; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_INVALID);
    profile.button_count=1; assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_INVALID);
    osg_profile_encoder_t encoder={24,25}; profile=(osg_profile_t){.name="slow-encoder",.encoders=&encoder,.encoder_count=1,.expander_mask=1};
    assert(osg_profile_validate(&profile,&layout)==OSG_PROFILE_INVALID);
    profile=(osg_profile_t){.name="axes-only",.axes=axes,.axis_count=8}; descriptor_fields(&profile);
    const osg_profile_button_t one_button={0,false,0};
    profile=(osg_profile_t){.name="button-only",.buttons=&one_button,.button_count=1}; descriptor_fields(&profile);
    profile=(osg_profile_t){.name="hat-only",.hats=hats,.hat_count=1}; descriptor_fields(&profile);
}
static void shifter(void) {
    const osg_profile_button_t buttons[]={{0,false,1},{1,false,1},{2,false,1}};
    const osg_profile_t profile={.name="shifter",.buttons=buttons,.button_count=3};
    osg_profile_sample_t sample={.digital_closed=1}; osg_profile_state_t state; uint8_t report[64];
    assert(osg_profile_init(&state,&profile,&sample,0)); assert(state.layout.length==1);
    osg_profile_update(&state,&sample,100,true,report); assert(!report[0]);
    sample.digital_closed=0; osg_profile_update(&state,&sample,101,true,report); osg_profile_update(&state,&sample,111,true,report);
    sample.digital_closed=1; osg_profile_update(&state,&sample,112,true,report); assert(!report[0]);
    osg_profile_update(&state,&sample,122,true,report); assert(report[0]==1);
    sample.digital_closed=3; osg_profile_update(&state,&sample,123,true,report); assert(!report[0] && state.exclusive_blocked[1]);
    sample.digital_closed=1; osg_profile_update(&state,&sample,140,true,report); assert(!report[0]);
    sample.digital_closed=0; osg_profile_update(&state,&sample,141,true,report); osg_profile_update(&state,&sample,151,true,report);
    assert(!state.exclusive_blocked[1]);
    sample.digital_closed=4; osg_profile_update(&state,&sample,152,true,report); osg_profile_update(&state,&sample,162,true,report); assert(report[0]==4);
    osg_profile_update(&state,&sample,163,false,report); osg_profile_update(&state,&sample,200,true,report); assert(!report[0]);
}
static void mixed_and_recovery(void) {
    const osg_profile_button_t buttons[]={{0,false,0},{24,false,0},{25,true,0}};
    const osg_profile_axis_t axes[]={{0,0x30,0,4095,32768},{7,0x37,0,4095,0}};
    const osg_profile_hat_t hats[]={{{26,27,42,43}}}; /* Hat spans two expansion banks. */
    const osg_profile_selector_t selectors[]={{28,44}};
    const osg_profile_encoder_t encoders[]={{1,2}};
    const osg_profile_t profile={.name="mixed",.buttons=buttons,.button_count=3,.axes=axes,.axis_count=2,
        .hats=hats,.hat_count=1,.selectors=selectors,.selector_count=1,.encoders=encoders,.encoder_count=1,.expander_mask=3};
    osg_profile_sample_t sample={.expansion_available=3,.axes_fresh=0x81,.axes={4095,0,0,0,0,0,0,0}};
    osg_profile_state_t state; uint8_t report[64]; assert(osg_profile_init(&state,&profile,&sample,0));
    osg_profile_update(&state,&sample,0,true,report); osg_profile_update(&state,&sample,10,true,report);
    assert(axis(&state,report,0)==65535 && axis(&state,report,1)==0 && state.axis_available==3);
    assert(report[state.layout.hat_offset]==8 && pressed(report,6)); /* selector centre after fresh debounce */
    sample.axes_fresh=0; sample.digital_closed=1; sample.expansion_closed[0]=3;
    osg_profile_update(&state,&sample,11,true,report); osg_profile_update(&state,&sample,21,true,report);
    assert(pressed(report,0) && pressed(report,1) && pressed(report,2));
    sample.expansion_available=2; osg_profile_update(&state,&sample,22,true,report);
    assert(pressed(report,0) && !pressed(report,1) && !pressed(report,2) && !pressed(report,6) && state.expansion_faults==1);
    sample.expansion_available=3; osg_profile_update(&state,&sample,23,true,report); osg_profile_update(&state,&sample,33,true,report);
    assert(!pressed(report,1) && pressed(report,2)); /* held momentary stays inhibited; maintained recovers */
    sample.expansion_closed[0]=2; osg_profile_update(&state,&sample,34,true,report); osg_profile_update(&state,&sample,44,true,report);
    sample.expansion_closed[0]=3; osg_profile_update(&state,&sample,45,true,report); osg_profile_update(&state,&sample,55,true,report); assert(pressed(report,1));
    /* North + south is invalid and disarms; release is required. */
    sample.expansion_closed[0]|=1U<<2; sample.expansion_closed[1]|=1U<<2;
    osg_profile_update(&state,&sample,56,true,report); osg_profile_update(&state,&sample,66,true,report);
    assert(state.hat_conflict==1 && report[state.layout.hat_offset]==8);
    sample.expansion_closed[1]=0; osg_profile_update(&state,&sample,67,true,report); osg_profile_update(&state,&sample,77,true,report); assert(report[state.layout.hat_offset]==8);
    sample.expansion_closed[0]&=(uint16_t)~(1U<<2); osg_profile_update(&state,&sample,78,true,report); osg_profile_update(&state,&sample,88,true,report);
    sample.expansion_closed[0]|=1U<<2; osg_profile_update(&state,&sample,89,true,report); osg_profile_update(&state,&sample,99,true,report); assert(report[state.layout.hat_offset]==0);
    sample.expansion_available=1; osg_profile_update(&state,&sample,100,true,report); assert(report[state.layout.hat_offset]==8);
    sample.expansion_available=3; osg_profile_update(&state,&sample,101,true,report); osg_profile_update(&state,&sample,111,true,report); assert(report[state.layout.hat_offset]==8);
    assert(axis(&state,report,0)==32768 && axis(&state,report,1)==0 && state.axis_available==0);
    /* Encoder directions are acknowledged, then released and acknowledged. */
    const uint8_t closed_bits[]={2,6,4,0};
    for(unsigned i=0;i<4;i++) { sample.digital_closed=closed_bits[i]; osg_profile_update(&state,&sample,120+i,true,report); }
    assert(pressed(report,3) || pressed(report,4));
    uint8_t sent[64]; memcpy(sent,report,64); osg_profile_reported(&state,sent,124);
    osg_profile_update(&state,&sample,148,true,report); assert(pressed(report,3) || pressed(report,4));
    osg_profile_update(&state,&sample,149,true,report); assert(!pressed(report,3) && !pressed(report,4));
    osg_profile_reported(&state,report,150);
    osg_profile_update(&state,&sample,176,true,report); assert(!pressed(report,3) && !pressed(report,4));
    for(unsigned i=0;i<4;i++) { sample.digital_closed=closed_bits[i]; osg_profile_update(&state,&sample,180+i,true,report); }
    osg_profile_rearm(&state,&sample,184); osg_profile_update(&state,&sample,200,true,report); assert(!pressed(report,3) && !pressed(report,4));
}
typedef struct { uint8_t registers[8][8]; unsigned calls; bool fail; } bus_mock_t;
static bool write_mock(void *context,uint8_t address,uint8_t reg,const uint8_t *data,uint8_t count) {
    bus_mock_t *m=context; m->calls++; if(m->fail) return false;
    assert(address>=0x20 && address<=0x27 && count==2 && (reg==4 || reg==6));
    memcpy(&m->registers[address-0x20][reg],data,count); return true;
}
static bool read_mock(void *context,uint8_t address,uint8_t reg,uint8_t *data,uint8_t count) {
    bus_mock_t *m=context; m->calls++; if(m->fail) return false;
    assert(address>=0x20 && address<=0x27 && count==2 && (reg==0 || reg==4 || reg==6));
    memcpy(data,&m->registers[address-0x20][reg],count); return true;
}
static void expansion_driver(void) {
    bus_mock_t mock={0}; osg_tca9555_t device; uint16_t contacts=123;
    osg_tca9555_init(&device,(osg_i2c_t){&mock,write_mock,read_mock});
    mock.registers[7][0]=0xfe; mock.registers[7][1]=0x7f;
    assert(osg_tca9555_sample(&device,7,&contacts) && contacts==0x8001 && device.configured==0x80 && mock.calls==5);
    mock.fail=true; assert(!osg_tca9555_sample(&device,7,&contacts) && contacts==0 && device.configured==0 && device.failures[7]==1);
    mock.fail=false; assert(osg_tca9555_sample(&device,7,&contacts) && contacts==0x8001);
    mock.registers[7][6]=0; assert(!osg_tca9555_sample(&device,7,&contacts) && contacts==0 && device.configured==0);
    assert(osg_tca9555_sample(&device,7,&contacts) && mock.registers[7][6]==0xff);
    assert(!osg_tca9555_sample(&device,8,&contacts));
    osg_tca9555_init(&device,(osg_i2c_t){&mock,write_mock,read_mock});
    for(unsigned bank=0;bank<8;bank++) { mock.registers[bank][0]=0xfe; mock.registers[bank][1]=0xff; }
    for(uint32_t tick=1;tick<=8;tick++) osg_tca9555_poll(&device,0xff,tick);
    uint16_t banks[8]; assert(osg_tca9555_inputs(&device,8,banks)==0xff);
    for(unsigned bank=0;bank<8;bank++) assert(banks[bank]==1);
    unsigned calls=mock.calls; osg_tca9555_poll(&device,0xff,8); assert(mock.calls==calls);
    assert(osg_tca9555_inputs(&device,101,banks)==0xfe && banks[0]==0);
    assert(osg_tca9555_inputs(&device,108,banks)==0);
    /* Independent bank freshness survives failures at other addresses. */
    for(uint32_t tick=109;tick<125;tick++) {
        mock.fail=device.next!=7; osg_tca9555_poll(&device,0xff,tick);
    }
    assert(osg_tca9555_inputs(&device,125,banks)==0x80 && banks[7]==1);
    /* Unsigned ages remain valid across the millisecond counter wrap. */
    device.available=1; device.updated_at[0]=UINT32_MAX-10; device.closed[0]=3;
    assert(osg_tca9555_inputs(&device,5,banks)==1 && banks[0]==3);
    assert(osg_tca9555_inputs(&device,90,banks)==0);
}
int main(void) { validation(); shifter(); mixed_and_recovery(); expansion_driver(); return 0; }
