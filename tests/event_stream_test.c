#include "opensimgear/event_stream.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

static const osg_profile_button_t button={.input=0};
static const osg_profile_encoder_t encoder={.a=1,.b=2};
static const osg_profile_axis_t axis={.channel=3,.usage=0x30,.minimum=0,.maximum=4095,.neutral=32768};
static const osg_profile_t panel={.name="stream-test",.buttons=&button,.button_count=1,
    .encoders=&encoder,.encoder_count=1,.axes=&axis,.axis_count=1};
static char frame[OSG_EVENTS_REPLY_CAPACITY+192];
static size_t length;

static osg_event_stream_result_t prepare(osg_event_stream_t *stream,const osg_events_t *events,uint32_t now) {
    return osg_event_stream_prepare(stream,events,now,frame,sizeof frame,&length);
}
static void sent(osg_event_stream_t *stream) { osg_event_stream_frame_sent(stream); }
static void emit(osg_event_stream_t *stream,const osg_events_t *events,uint32_t now) {
    assert(prepare(stream,events,now)==OSG_EVENT_STREAM_READY);
    assert(fwrite(frame,1,length,stdout)==length);sent(stream);
}
static void contact(osg_events_t *events,osg_profile_sample_t *sample,uint32_t *now,bool pressed) {
    sample->digital_closed=pressed?1U:0U;
    osg_events_observe(events,sample,*now);*now+=10;osg_events_observe(events,sample,*now);*now+=1;
}
static void detent(osg_events_t *events,osg_profile_sample_t *sample,uint32_t now) {
    sample->digital_closed|=UINT32_C(1)<<1;osg_events_observe(events,sample,now);
    sample->digital_closed|=UINT32_C(1)<<2;osg_events_observe(events,sample,now+1);
    sample->digital_closed&=~(UINT32_C(1)<<1);osg_events_observe(events,sample,now+2);
    sample->digital_closed&=~(UINT32_C(1)<<2);osg_events_observe(events,sample,now+3);
}
static void baseline_and_backpressure(void) {
    osg_profile_button_t maintained={.input=0,.maintained=true};
    osg_profile_t held_profile={.name="held",.buttons=&maintained,.button_count=1};
    osg_profile_sample_t sample={.digital_closed=1};osg_events_t events;osg_event_stream_t stream;
    assert(osg_events_init(&events,&held_profile,&sample,0,4));osg_events_observe(&events,&sample,10);
    assert(events.held[0]);assert(osg_event_stream_start(&stream,&events,77,10));
    char tiny[1];size_t short_length=0;
    assert(osg_event_stream_prepare(&stream,&events,10,tiny,sizeof tiny,&short_length)==OSG_EVENT_STREAM_FAULT);
    /* Tiny output is not consumed. */
    assert(osg_event_stream_prepare(&stream,&events,10,frame,sizeof frame,&length)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"kind\":\"baseline\"") && strstr(frame,"\"held\":[true]"));
    assert(!strstr(frame,"\"events\":["));char retained[sizeof frame];memcpy(retained,frame,length+1);
    char other[]="untouched";size_t ignored=0;
    assert(osg_event_stream_prepare(&stream,&events,11,other,sizeof other,&ignored)==OSG_EVENT_STREAM_BLOCKED);
    assert(!strcmp(other,"untouched") && !memcmp(frame,retained,length));sent(&stream);
    assert(prepare(&stream,&events,109)==OSG_EVENT_STREAM_IDLE);
    assert(prepare(&stream,&events,110)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"kind\":\"update\"") && strstr(frame,"\"reason\":\"health\""));
    assert(strstr(frame,"\"sampledAtMs\":10"));sent(&stream);
    osg_event_stream_stop(&stream);assert(!stream.active && !stream.boot);
}
static void edges_pages_and_axes(void) {
    osg_profile_sample_t sample={0};sample.axes_fresh=1U<<3;osg_events_t events;osg_event_stream_t stream;
    assert(osg_events_init(&events,&panel,&sample,0,8));
    osg_events_observe(&events,&sample,1);osg_events_observe(&events,&sample,11);
    assert(osg_event_stream_start(&stream,&events,9,11));assert(prepare(&stream,&events,11)==OSG_EVENT_STREAM_READY);sent(&stream);
    uint32_t now=20;
    for(unsigned i=0;i<9;i++) { contact(&events,&sample,&now,true);contact(&events,&sample,&now,false); }
    assert(events.head==18);assert(prepare(&stream,&events,now)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"reason\":\"events\"") && strstr(frame,"\"next\":16,\"more\":true"));
    assert(strstr(frame,"\"sequence\":1,\"control\":0,\"pressed\":true,\"capturedAtMs\":30"));sent(&stream);
    assert(prepare(&stream,&events,now)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"after\":16,\"next\":18,\"more\":false"));sent(&stream);
    sample.axes[3]=4095;osg_events_observe(&events,&sample,now+1);
    assert(prepare(&stream,&events,now+1)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"reason\":\"axes\"") && strstr(frame,"\"axisAvailable\":[true]"));sent(&stream);
    sample.axes_fresh=0;osg_events_observe(&events,&sample,now+102);
    assert(prepare(&stream,&events,now+102)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"reason\":\"axes\"") && strstr(frame,"\"axisAvailable\":[false]"));sent(&stream);
    /* Encoder detents remain ordered edge pairs and carry the collector time. */
    detent(&events,&sample,now+110);assert(prepare(&stream,&events,now+110)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"control\":1,\"pressed\":true,\"capturedAtMs\":") &&
           strstr(frame,"\"control\":1,\"pressed\":false,\"capturedAtMs\":") );sent(&stream);
}
static void loss_and_exhaustion(void) {
    osg_profile_sample_t sample={0};osg_events_t events;osg_event_stream_t stream;
    assert(osg_events_init(&events,&panel,&sample,0,12));assert(osg_event_stream_start(&stream,&events,33,0));
    assert(prepare(&stream,&events,0)==OSG_EVENT_STREAM_READY);sent(&stream);
    osg_events_observe(&events,&sample,0);osg_events_observe(&events,&sample,10);
    uint32_t now=20;for(unsigned i=0;i<33;i++) {contact(&events,&sample,&now,true);contact(&events,&sample,&now,false);}
    assert(events.overflow>0);assert(prepare(&stream,&events,now)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"kind\":\"loss\"") && strstr(frame,"\"reason\":\"eventGap\"") &&
           strstr(frame,"\"head\":66"));sent(&stream);
    assert(osg_events_init(&events,&panel,&sample,now,13));
    assert(prepare(&stream,&events,now)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"reason\":\"scopeChanged\"") && strstr(frame,"\"stream\":13"));sent(&stream);
    events.exhausted=true;assert(prepare(&stream,&events,now+1)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"reason\":\"streamExhausted\"") && strstr(frame,"\"exhausted\":true") &&
           strstr(frame,"\"head\":0"));sent(&stream);
    assert(prepare(&stream,&events,now+2)==OSG_EVENT_STREAM_IDLE);
    assert(prepare(&stream,&events,now+1000)==OSG_EVENT_STREAM_IDLE);
    assert(!osg_event_stream_start(&stream,&events,33,now+1001));
    assert(osg_events_init(&events,&panel,&sample,now+1001,14));
    assert(prepare(&stream,&events,now+1001)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"reason\":\"scopeChanged\"") && strstr(frame,"\"stream\":14"));sent(&stream);
    events.exhausted=true;assert(prepare(&stream,&events,now+1002)==OSG_EVENT_STREAM_READY);
    assert(strstr(frame,"\"reason\":\"streamExhausted\"") && strstr(frame,"\"exhausted\":true"));sent(&stream);
    assert(prepare(&stream,&events,now+2000)==OSG_EVENT_STREAM_IDLE);
    assert(osg_events_init(&events,&panel,&sample,now+2001,15));
    assert(osg_event_stream_start(&stream,&events,33,now+2001));stream.next_sequence=UINT32_MAX;
    assert(prepare(&stream,&events,0)==OSG_EVENT_STREAM_READY);sent(&stream);
    assert(prepare(&stream,&events,100)==OSG_EVENT_STREAM_FAULT);
    osg_event_stream_stop(&stream);assert(prepare(&stream,&events,101)==OSG_EVENT_STREAM_FAULT);
}
static int emit_vectors(void) {
    osg_profile_sample_t sample={0};sample.axes_fresh=1U<<3;
    osg_events_t events;osg_event_stream_t stream;uint32_t now=0;
    assert(osg_events_init(&events,&panel,&sample,now,4));
    osg_events_observe(&events,&sample,++now);now+=10;osg_events_observe(&events,&sample,now);
    assert(osg_event_stream_start(&stream,&events,77,now));emit(&stream,&events,now);
    now+=10;
    for(unsigned i=0;i<8;i++) {contact(&events,&sample,&now,true);contact(&events,&sample,&now,false);}
    detent(&events,&sample,now);now+=4;
    emit(&stream,&events,now); /* First 16 button edges. */
    emit(&stream,&events,now); /* Second page contains the ordered encoder pair. */
    sample.axes[3]=4095;osg_events_observe(&events,&sample,++now);
    emit(&stream,&events,now);
    for(unsigned i=0;i<33;i++) {contact(&events,&sample,&now,true);contact(&events,&sample,&now,false);}
    emit(&stream,&events,now); /* Ring gap is an explicit loss baseline. */
    assert(osg_events_init(&events,&panel,&sample,now,5));
    emit(&stream,&events,now); /* A greater stream creates an explicit scope baseline. */
    events.exhausted=true;emit(&stream,&events,++now);
    assert(osg_events_init(&events,&panel,&sample,now,6));
    osg_events_observe(&events,&sample,++now);now+=10;osg_events_observe(&events,&sample,now);
    emit(&stream,&events,now); /* A fresh greater stream recovers with a new baseline. */
    contact(&events,&sample,&now,true);emit(&stream,&events,now);
    return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--vectors")) {
#ifdef _WIN32
        assert(_setmode(_fileno(stdout),_O_BINARY)!=-1);
#endif
        return emit_vectors();
    }
    if(argc!=1) return EXIT_FAILURE;
    osg_events_t events;osg_event_stream_t stream;osg_profile_sample_t sample={0};char tiny[1];size_t n=0;
    assert(osg_events_init(&events,&panel,&sample,0,1));assert(osg_event_stream_start(&stream,&events,1,0));
    assert(osg_event_stream_prepare(&stream,&events,0,tiny,sizeof tiny,&n)==OSG_EVENT_STREAM_FAULT);
    assert(prepare(&stream,&events,0)==OSG_EVENT_STREAM_READY);sent(&stream);
    baseline_and_backpressure();edges_pages_and_axes();loss_and_exhaustion();
    puts("reactive event stream producer passed");return 0;
}
