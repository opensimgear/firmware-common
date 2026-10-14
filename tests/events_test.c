#include "opensimgear/events.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const osg_profile_button_t button={.input=0};
static const osg_profile_encoder_t encoder={.a=1,.b=2};
static const osg_profile_t profile={.name="panel",.buttons=&button,.button_count=1,.encoders=&encoder,.encoder_count=1};
static char reply[OSG_EVENTS_REPLY_CAPACITY];
static void command(const osg_events_t *,const char *,const char *);
static void transform(void *context,const osg_profile_state_t *state,uint8_t report[OSG_REPORT_CAPACITY]) {
    unsigned *calls=context;(*calls)++;
    /* A production product uses its runtime inversion/deadzone here. This
     * independent test transform proves the snapshot sees the final values. */
    if(state->axis_available&1U) {
        unsigned offset=state->layout.axis_offset;
        uint16_t value=(uint16_t)(report[offset]|(uint16_t)report[offset+1]<<8);
        value=(uint16_t)(65535U-value);report[offset]=(uint8_t)value;report[offset+1]=(uint8_t)(value>>8);
    }
}
static void axis_snapshot_test(void) {
    osg_profile_axis_t axes[]={ {.channel=3,.usage=0x30,.minimum=100,.maximum=3900,.neutral=123},
                               {.channel=1,.usage=0x31,.minimum=0,.maximum=4095,.neutral=32768} };
    osg_profile_button_t buttons[]={ {.input=0}, {.input=1,.maintained=true} };
    osg_profile_encoder_t encoder={.a=2,.b=3};
    osg_profile_selector_t selector={.low=4,.high=5};
    osg_profile_t panel={.name="radio",.buttons=buttons,.button_count=2,.encoders=&encoder,.encoder_count=1,
        .selectors=&selector,.selector_count=1,.axes=axes,.axis_count=2};
    osg_profile_sample_t sample={0};osg_events_t events;
    assert(osg_events_init(&events,&panel,&sample,UINT32_MAX-20U,7));
    command(&events,"events state","\"contactButtonCount\":2,\"maintainedButtons\":[1],\"selectorButtons\":[[4,5,6]]");
    assert(strstr(reply,"\"axisCount\":2,\"axisUsage\":[48,49],\"axisNeutral\":[123,32768],\"axes\":[123,32768],\"axisAvailable\":[false,false]"));
    assert(strstr(reply,"\"sampledAtMs\":4294967275"));
    osg_events_set_transform(NULL,transform,NULL);
    unsigned calls=0;osg_events_set_transform(&events,transform,&calls);
    sample.axes[3]=100;sample.axes[1]=4095;sample.axes_fresh=(1U<<3)|(1U<<1);
    osg_events_observe(&events,&sample,UINT32_MAX-10U);
    command(&events,"events state","\"axes\":[65535,65535],\"axisAvailable\":[true,true]");
    assert(calls==1 && strstr(reply,"\"encoderButtons\":[2,3]"));
    sample.axes[3]=2000;sample.axes[1]=0;osg_events_observe(&events,&sample,0);
    command(&events,"events read 7 0","\"axes\":[32767,0],\"axisAvailable\":[true,true],\"sampledAtMs\":0");
    sample.axes_fresh=0;osg_events_observe(&events,&sample,99);
    command(&events,"events state","\"axes\":[32767,0],\"axisAvailable\":[true,true]");
    osg_events_observe(&events,&sample,100);
    command(&events,"events state","\"axes\":[123,32768],\"axisAvailable\":[false,false]");
    assert(calls==4);
    /* A new mapping/stream discards both data and the previous callback. */
    assert(osg_events_init(&events,&panel,&sample,101,8));
    assert(!events.transform && !events.transform_context);
    sample.axes_fresh=(1U<<3);sample.axes[3]=3900;osg_events_observe(&events,&sample,111);
    command(&events,"events state","\"axes\":[65535,32768],\"axisAvailable\":[true,false]");
}
static void command(const osg_events_t *events,const char *line,const char *contains) {
    assert(osg_events_command(events,line,reply,sizeof reply));
    if(!strstr(reply,contains)) fprintf(stderr,"%s expected %s in %s\n",line,contains,reply);
    assert(strstr(reply,contains));
    assert(strlen(reply)<=OSG_EVENTS_REPLY_CAPACITY-5);
}
static void contact(osg_events_t *events,osg_profile_sample_t *sample,uint32_t now,bool pressed) {
    sample->digital_closed=pressed?1:0;
    osg_events_observe(events,sample,now);osg_events_observe(events,sample,now+10);
}
static void detent(osg_events_t *events,osg_profile_sample_t *sample,uint32_t now,bool reverse) {
    unsigned a=reverse?2:1,b=reverse?1:2;
    sample->digital_closed|=UINT32_C(1)<<a;osg_events_observe(events,sample,now);
    sample->digital_closed|=UINT32_C(1)<<b;osg_events_observe(events,sample,now+1);
    sample->digital_closed&=~(UINT32_C(1)<<a);osg_events_observe(events,sample,now+2);
    sample->digital_closed&=~(UINT32_C(1)<<b);osg_events_observe(events,sample,now+3);
}
int main(void) {
    osg_events_t events;osg_profile_sample_t sample={0};
    assert(!osg_events_init(NULL,&profile,&sample,0,1));
    assert(!osg_events_init(&events,&profile,&sample,0,0));
    assert(!osg_events_init(&events,NULL,&sample,0,1));
    assert(!osg_events_init(&events,&profile,NULL,0,1));
    command(&events,"events state","invalidStream");command(NULL,"events state","invalidStream");
    assert(osg_events_init(&events,&profile,&sample,0,1));
    assert(!osg_events_command(&events,NULL,reply,sizeof reply));
    strcpy(reply,"untouched");assert(!osg_events_command(&events,"eventsx state",reply,sizeof reply));assert(!strcmp(reply,"untouched"));
    command(&events,"events state","\"held\":[false,false,false]");assert(strstr(reply,"\"encoderButtons\":[1,2]"));
    command(&events,"events read 1 0","\"next\":0,\"more\":false,\"events\":[]");
    osg_events_observe(&events,&sample,10);
    contact(&events,&sample,20,true);contact(&events,&sample,31,false);
    assert(events.head==2 && !events.held[0]);
    command(&events,"events read 1 0","\"sequence\":1,\"control\":0,\"pressed\":true");
    assert(strstr(reply,"\"sequence\":2,\"control\":0,\"pressed\":false"));
    char retained[sizeof reply];strcpy(retained,reply);
    command(&events,"events read 1 0","\"next\":2");assert(!strcmp(retained,reply));
    /* More than the HID pulse queue capacity: events still see every detent,
     * independently of USB/CAN completion acknowledgements. */
    for(unsigned i=0;i<20;i++) detent(&events,&sample,50+4*i,i%2!=0);
    assert(events.head==42 && events.count==42);
    for(unsigned i=2;i<42;i++) {
        const osg_input_event_t *event=&events.ring[i];
        assert(event->sequence==i+1 && event->pressed==(i%2==0));
        assert(event->control==(uint8_t)(1+((i-2)/2)%2));
    }
    assert(!events.held[1] && !events.held[2]);
    command(&events,"events read 1 2","\"next\":18,\"more\":true");
    command(&events,"events read 1 34","\"next\":42,\"more\":false");
    const char *bad[]={"events","events unknown","events state extra","events read","events read 1",
        "events read 1 0 extra","events read -1 0","events read 4294967296 0","events read 1 x"};
    for(unsigned i=0;i<sizeof bad/sizeof *bad;i++) command(&events,bad[i],"\"ok\":false");
    command(&events,"events read 2 0","staleStream");command(&events,"events read 1 43","invalidCursor");
    char long_command[400];memset(long_command,'x',399);memcpy(long_command,"events ",7);long_command[399]=0;
    command(&events,long_command,"invalidArgument");
    assert(osg_events_command(&events,"events state",reply,64));assert(strstr(reply,"replyTooSmall"));
    assert(osg_events_command(&events,"events state",NULL,0));
    char tiny[1];assert(osg_events_command(&events,"events state",tiny,1));assert(!*tiny);
    osg_events_observe(NULL,&sample,0);osg_events_observe(&events,NULL,0);
    for(unsigned i=0;i<20;i++) contact(&events,&sample,200+11*i,i%2==0);
    assert(events.head==62);
    for(unsigned i=0;i<4;i++) detent(&events,&sample,500+4*i,false);
    assert(events.head==70 && events.count==64 && events.overflow==6);
    command(&events,"events state","\"oldest\":7,\"overflow\":6");
    command(&events,"events read 1 5","eventGap");command(&events,"events read 1 6","\"sequence\":7");
    events.overflow=UINT32_MAX-1U;detent(&events,&sample,600,false);assert(events.overflow==UINT32_MAX);
    /* A mapping activation starts a fresh stream; historical cursors fail. */
    assert(osg_events_init(&events,&profile,&sample,700,2));
    command(&events,"events read 1 70","staleStream");command(&events,"events read 2 70","invalidCursor");
    events.head=UINT32_MAX-1U;detent(&events,&sample,710,false);assert(events.exhausted && events.count==0);
    command(&events,"events state","streamExhausted");osg_events_observe(&events,&sample,900);
    assert(osg_events_init(&events,&profile,&sample,0,3));osg_events_observe(&events,&sample,10);
    events.head=UINT32_MAX;contact(&events,&sample,20,true);assert(events.exhausted);
    /* Held-at-start momentary contacts do not synthesize a press. */
    sample.digital_closed=1;assert(osg_events_init(&events,&profile,&sample,0,4));
    osg_events_observe(&events,&sample,20);assert(events.head==0);
    contact(&events,&sample,30,false);contact(&events,&sample,41,true);assert(events.head==1);
    /* Maintained contacts and selectors share production debounce/conflicts. */
    osg_profile_button_t maintained={.input=24,.maintained=true};
    osg_profile_selector_t selector={.low=0,.high=1};
    osg_profile_t controls={.name="switches",.buttons=&maintained,.button_count=1,.selectors=&selector,.selector_count=1,.expander_mask=1};
    sample=(osg_profile_sample_t){.expansion_available=1};
    assert(osg_events_init(&events,&controls,&sample,0,5));osg_events_observe(&events,&sample,10);
    assert(events.held[2]);sample.expansion_closed[0]=1;osg_events_observe(&events,&sample,20);osg_events_observe(&events,&sample,30);assert(events.held[0]);
    sample.expansion_available=0;osg_events_observe(&events,&sample,31);assert(!events.held[0]);
    sample.digital_closed=3;osg_events_observe(&events,&sample,40);osg_events_observe(&events,&sample,50);
    assert(!events.held[1] && !events.held[2] && !events.held[3]);
    /* Maximum mapped layout still produces a framed-transport-sized page. */
    osg_profile_button_t buttons[128];for(unsigned i=0;i<128;i++) buttons[i]=(osg_profile_button_t){.input=(uint8_t)i};
    osg_profile_t maximum={.name="maximum",.buttons=buttons,.button_count=128,.expander_mask=255};
    sample=(osg_profile_sample_t){.expansion_available=255};assert(osg_events_init(&events,&maximum,&sample,0,UINT32_MAX));
    osg_events_observe(&events,&sample,10);osg_events_observe(&events,&sample,20);sample.digital_closed=0xFFFFFF;
    for(unsigned i=0;i<8;i++) sample.expansion_closed[i]=0xFFFF;
    osg_events_observe(&events,&sample,30);osg_events_observe(&events,&sample,40);
    assert(events.head==128);command(&events,"events read 4294967295 64","\"next\":80");
    /* Maximum metadata and a full page remain within sealed response bounds. */
    for(unsigned i=0;i<128;i++) buttons[i].maintained=true;
    osg_profile_axis_t axes[8];for(unsigned i=0;i<8;i++) axes[i]=(osg_profile_axis_t){.channel=(uint8_t)i,.usage=(uint8_t)(0x30+i),.minimum=0,.maximum=4095,.neutral=65535};
    maximum.axes=axes;maximum.axis_count=8;
    assert(osg_events_init(&events,&maximum,&sample,UINT32_MAX,UINT32_MAX));
    osg_events_observe(&events,&sample,UINT32_MAX);osg_events_observe(&events,&sample,9);
    command(&events,"events read 4294967295 64","\"next\":80");
    assert(strstr(reply,"\"maintainedButtons\":[0,1,2") && strstr(reply,"\"axisCount\":8"));
    axis_snapshot_test();
    puts("bounded independent panel input events passed");
}
