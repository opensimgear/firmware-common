#include "opensimgear/profile.h"
#include <string.h>

static osg_profile_result_t claim(uint8_t input, const osg_profile_t *p, bool used[OSG_INPUTS], osg_profile_layout_t *l) {
    if (input>=OSG_INPUTS || (input>=24 && !(p->expander_mask&(1U<<((input-24)/16))))) return OSG_PROFILE_INVALID;
    if (used[input]) return OSG_PROFILE_OVERLAP;
    used[input]=true; if(input<24) l->digital_mask|=1UL<<input;
    return OSG_PROFILE_OK;
}
osg_profile_result_t osg_profile_validate(const osg_profile_t *p, osg_profile_layout_t *layout) {
    if(!p || !layout || !p->name || !p->name[0]) return OSG_PROFILE_INVALID;
    if(p->button_count>OSG_MAX_BUTTONS || p->axis_count>8 || p->hat_count>4 ||
       p->encoder_count>8 || p->selector_count>8) return OSG_PROFILE_TOO_LARGE;
    if((p->button_count && !p->buttons) || (p->axis_count && !p->axes) ||
       (p->hat_count && !p->hats) || (p->encoder_count && !p->encoders) ||
       (p->selector_count && !p->selectors)) return OSG_PROFILE_INVALID;
    unsigned buttons=p->button_count+2U*p->encoder_count+3U*p->selector_count;
    if(buttons>128) return OSG_PROFILE_TOO_LARGE;
    if(!buttons && !p->axis_count && !p->hat_count) return OSG_PROFILE_INVALID;
    osg_profile_layout_t l={.buttons=(uint8_t)buttons,.axes=p->axis_count,.hats=p->hat_count};
    l.hat_offset=(uint8_t)((buttons+7)/8); l.axis_offset=(uint8_t)(l.hat_offset+(p->hat_count+1)/2);
    l.length=(uint8_t)(l.axis_offset+2*p->axis_count);
    if(l.length>OSG_REPORT_CAPACITY) return OSG_PROFILE_TOO_LARGE;
    bool used[OSG_INPUTS]={false}, usages[9]={false};
#define CLAIM(input) do { osg_profile_result_t result=claim(input,p,used,&l); if(result!=OSG_PROFILE_OK) return result; } while(0)
    for(unsigned i=0;i<p->button_count;i++) {
        CLAIM(p->buttons[i].input);
        if(p->buttons[i].maintained && p->buttons[i].exclusive_group) return OSG_PROFILE_INVALID;
    }
    for(unsigned i=0;i<p->axis_count;i++) {
        osg_profile_axis_t a=p->axes[i];
        if(a.channel>=8 || a.minimum>=a.maximum || a.maximum>4095 ||
           a.usage<0x30 || a.usage>0x38) return OSG_PROFILE_INVALID;
        if((l.adc_mask&(1U<<a.channel)) || usages[a.usage-0x30]) return OSG_PROFILE_OVERLAP;
        l.adc_mask|=(uint8_t)(1U<<a.channel); usages[a.usage-0x30]=true;
    }
    for(unsigned i=0;i<p->hat_count;i++) for(unsigned j=0;j<4;j++) CLAIM(p->hats[i].inputs[j]);
    for(unsigned i=0;i<p->encoder_count;i++) {
        if(p->encoders[i].a>=24 || p->encoders[i].b>=24) return OSG_PROFILE_INVALID;
        CLAIM(p->encoders[i].a); CLAIM(p->encoders[i].b);
    }
    for(unsigned i=0;i<p->selector_count;i++) { CLAIM(p->selectors[i].low); CLAIM(p->selectors[i].high); }
#undef CLAIM
    *layout=l; return OSG_PROFILE_OK;
}
size_t osg_profile_descriptor(const osg_profile_t *p, uint8_t *out, size_t capacity) {
    osg_profile_layout_t l;
    if(osg_profile_validate(p,&l)!=OSG_PROFILE_OK || !out) return 0;
    uint8_t d[OSG_DESCRIPTOR_CAPACITY]; size_t n=0;
#define EMIT(...) do { const uint8_t items[]={__VA_ARGS__}; memcpy(d+n,items,sizeof items); n+=sizeof items; } while(0)
    EMIT(0x05,1,0x09,4,0xa1,1);
    if(l.buttons) {
        EMIT(0x05,9,0x19,1,0x29,l.buttons,0x15,0,0x25,1,0x75,1,0x95,l.buttons,0x81,2);
        if(l.buttons%8) EMIT(0x75,1,0x95,8-l.buttons%8,0x81,3);
    }
    if(l.hats) {
        EMIT(0x05,1,0x09,0x39,0x15,0,0x25,7,0x35,0,0x46,0x3b,1,
             0x65,0x14,0x75,4,0x95,l.hats,0x81,0x42);
        if(l.hats%2) EMIT(0x75,4,0x95,1,0x81,3);
    }
    if(l.axes) {
        EMIT(0x05,1,0x15,0,0x27,0xff,0xff,0,0,0x35,0,0x45,0,0x65,0,0x75,16,0x95,1);
        for(unsigned i=0;i<l.axes;i++) EMIT(0x09,p->axes[i].usage,0x81,2);
    }
    EMIT(0xc0);
#undef EMIT
    if(capacity<n) return 0;
    memcpy(out,d,n); return n;
}
static bool available(const osg_profile_sample_t *s, uint8_t input) {
    return input<24 || (s->expansion_available&(1U<<((input-24)/16)));
}
static bool closed(const osg_profile_sample_t *s, uint8_t input) {
    return input<24 ? (s->digital_closed&(1UL<<input))!=0 :
           (s->expansion_closed[(input-24)/16]&(1U<<((input-24)%16)))!=0;
}
static uint8_t ab(const osg_profile_sample_t *s, osg_profile_encoder_t e) {
    return (uint8_t)((closed(s,e.a)?0:1)|(closed(s,e.b)?0:2));
}
static void bit(uint8_t *r, unsigned position, bool on) { if(on) r[position/8]|=(uint8_t)(1U<<(position%8)); }
void osg_profile_neutral(const osg_profile_state_t *s, uint8_t r[OSG_REPORT_CAPACITY]) {
    memset(r,0,OSG_REPORT_CAPACITY);
    for(unsigned i=0;i<s->layout.hats;i++) r[s->layout.hat_offset+i/2]|=(uint8_t)(8U<<(4*(i%2)));
    for(unsigned i=0;i<s->layout.axes;i++) {
        uint16_t value=s->definition->axes[i].neutral;
        r[s->layout.axis_offset+2*i]=(uint8_t)value; r[s->layout.axis_offset+2*i+1]=(uint8_t)(value>>8);
    }
}
void osg_profile_rearm(osg_profile_state_t *s, const osg_profile_sample_t *sample, uint32_t now) {
    const osg_profile_t *p=s->definition;
    for(unsigned i=0;i<p->button_count;i++) {
        osg_button_rearm(&s->buttons[i],closed(sample,p->buttons[i].input),now,10);
        osg_contact_init(&s->maintained[i],closed(sample,p->buttons[i].input),now);
    }
    for(unsigned i=0;i<p->hat_count;i++) for(unsigned j=0;j<4;j++)
        osg_contact_init(&s->hats[i][j],closed(sample,p->hats[i].inputs[j]),now);
    for(unsigned i=0;i<p->selector_count;i++) {
        osg_contact_init(&s->selectors[i][0],closed(sample,p->selectors[i].low),now);
        osg_contact_init(&s->selectors[i][1],closed(sample,p->selectors[i].high),now);
    }
    for(unsigned i=0;i<p->encoder_count;i++) osg_encoder_rearm(&s->encoders[i],ab(sample,p->encoders[i]));
    s->hat_armed=0; s->axis_seen=0; s->axis_available=0;
    memset(s->exclusive_blocked,0,sizeof s->exclusive_blocked);
}
bool osg_profile_init(osg_profile_state_t *s,const osg_profile_t *p,const osg_profile_sample_t *sample,uint32_t now) {
    if(!s || !sample) return false;
    memset(s,0,sizeof *s);
    if(osg_profile_validate(p,&s->layout)!=OSG_PROFILE_OK) return false;
    s->definition=p; osg_profile_rearm(s,sample,now); return true;
}
void osg_profile_update(osg_profile_state_t *s,const osg_profile_sample_t *sample,uint32_t now,bool active,uint8_t r[OSG_REPORT_CAPACITY]) {
    const osg_profile_t *p=s->definition;
    osg_profile_neutral(s,r);
    uint8_t recovered=(uint8_t)(sample->expansion_available&~s->expansion_available);
    s->expansion_available=(uint8_t)(sample->expansion_available&p->expander_mask);
    s->expansion_faults=(uint8_t)(p->expander_mask&~s->expansion_available);
    s->axis_available=0; s->hat_conflict=0; s->selector_conflict=0;
    if(!active) { osg_profile_rearm(s,sample,now); return; }
    unsigned group_closed[256]={0}; bool group_ready[256]; memset(group_ready,1,sizeof group_ready);
    for(unsigned i=0;i<p->button_count;i++) {
        osg_profile_button_t b=p->buttons[i]; bool ok=available(sample,b.input), value=closed(sample,b.input);
        bool recovery=b.input>=24 && (recovered&(1U<<((b.input-24)/16)));
        if(!ok || recovery) {
            osg_button_rearm(&s->buttons[i],value,now,10); osg_contact_init(&s->maintained[i],value,now);
        }
        bool output=b.maintained ? osg_contact_update(&s->maintained[i],value,now) && s->maintained[i].ready :
                                  osg_button_update(&s->buttons[i],value,now);
        if(b.exclusive_group) {
            group_closed[b.exclusive_group]+=ok && value;
            group_ready[b.exclusive_group]&=ok && s->buttons[i].armed && !s->buttons[i].stable;
        }
        bit(r,i,ok && output);
    }
    for(unsigned group=1;group<256;group++) {
        if(group_closed[group]>1 && !s->exclusive_blocked[group]) {
            s->exclusive_blocked[group]=true;
            for(unsigned i=0;i<p->button_count;i++) if(p->buttons[i].exclusive_group==group)
                osg_button_rearm(&s->buttons[i],closed(sample,p->buttons[i].input),now,10);
        }
        if(!group_closed[group] && group_ready[group]) s->exclusive_blocked[group]=false;
    }
    for(unsigned i=0;i<p->button_count;i++) if(s->exclusive_blocked[p->buttons[i].exclusive_group])
        r[i/8]&=(uint8_t)~(1U<<(i%8));
    unsigned button=p->button_count;
    for(unsigned i=0;i<p->encoder_count;i++,button+=2) {
        osg_encoder_sample(&s->encoders[i],ab(sample,p->encoders[i]));
        int8_t direction=osg_encoder_output(&s->encoders[i],now);
        bit(r,button,direction>0); bit(r,button+1,direction<0);
    }
    for(unsigned i=0;i<p->selector_count;i++,button+=3) {
        uint8_t pins[2]={p->selectors[i].low,p->selectors[i].high}; bool value[2], ready=true;
        for(unsigned j=0;j<2;j++) {
            bool ok=available(sample,pins[j]);
            if(!ok || (pins[j]>=24 && (recovered&(1U<<((pins[j]-24)/16)))))
                osg_contact_init(&s->selectors[i][j],closed(sample,pins[j]),now);
            value[j]=osg_contact_update(&s->selectors[i][j],closed(sample,pins[j]),now);
            ready&=ok && s->selectors[i][j].ready;
        }
        bool conflict=value[0] && value[1]; if(conflict) s->selector_conflict|=(uint8_t)(1U<<i);
        if(ready && !conflict) bit(r,button+(value[0]?0:value[1]?2:1),true);
    }
    for(unsigned i=0;i<p->hat_count;i++) {
        uint8_t contacts=0; bool ready=true;
        for(unsigned j=0;j<4;j++) {
            uint8_t pin=p->hats[i].inputs[j]; bool ok=available(sample,pin);
            if(!ok || (pin>=24 && (recovered&(1U<<((pin-24)/16))))) {
                osg_contact_init(&s->hats[i][j],closed(sample,pin),now); s->hat_armed&=(uint8_t)~(1U<<i);
            }
            if(osg_contact_update(&s->hats[i][j],closed(sample,pin),now)) contacts|=(uint8_t)(1U<<j);
            ready&=ok && s->hats[i][j].ready;
        }
        uint8_t position=osg_hat_position(contacts);
        if(contacts && position==8) { s->hat_conflict|=(uint8_t)(1U<<i); s->hat_armed&=(uint8_t)~(1U<<i); }
        if(ready && !contacts) s->hat_armed|=(uint8_t)(1U<<i);
        if(ready && (s->hat_armed&(1U<<i))) {
            uint8_t shift=(uint8_t)(4*(i%2));
            r[s->layout.hat_offset+i/2]=(uint8_t)((r[s->layout.hat_offset+i/2]&~(15U<<shift))|(position<<shift));
        }
    }
    for(unsigned i=0;i<p->axis_count;i++) {
        osg_profile_axis_t a=p->axes[i];
        if((sample->axes_fresh&(1U<<a.channel)) && sample->axes[a.channel]<=4095) {
            s->axes[i]=osg_axis_scale(sample->axes[a.channel],a.minimum,a.maximum);
            s->axis_at[i]=now; s->axis_seen|=(uint8_t)(1U<<i);
        }
        bool fresh=(s->axis_seen&(1U<<i)) && (uint32_t)(now-s->axis_at[i])<100;
        if(fresh) {
            s->axis_available|=(uint8_t)(1U<<i); r[s->layout.axis_offset+2*i]=(uint8_t)s->axes[i];
            r[s->layout.axis_offset+2*i+1]=(uint8_t)(s->axes[i]>>8);
        }
    }
}
void osg_profile_reported(osg_profile_state_t *s,const uint8_t *r,uint32_t now) {
    for(unsigned i=0;i<s->definition->encoder_count;i++) {
        unsigned position=s->definition->button_count+2*i;
        bool plus=(r[position/8]&(1U<<(position%8)))!=0; position++;
        bool minus=(r[position/8]&(1U<<(position%8)))!=0;
        osg_encoder_reported(&s->encoders[i],plus?1:minus?-1:0,now);
    }
}
