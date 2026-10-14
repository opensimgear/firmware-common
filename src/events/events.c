#include "opensimgear/events.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static bool closed(const osg_profile_sample_t *s,uint8_t input) {
    return input<24 ? (s->digital_closed&(UINT32_C(1)<<input))!=0 :
        (s->expansion_closed[(input-24)/16]&(1U<<((input-24)%16)))!=0;
}
static uint8_t ab(const osg_profile_sample_t *s,osg_profile_encoder_t e) {
    return (uint8_t)((closed(s,e.a)?0:1)|(closed(s,e.b)?0:2));
}
bool osg_events_init(osg_events_t *s,const osg_profile_t *p,const osg_profile_sample_t *sample,uint32_t now,uint32_t stream) {
    if(!s) return false;
    memset(s,0,sizeof *s);
    if(!stream || !osg_profile_init(&s->acquisition,p,sample,now)) return false;
    s->stream=stream;
    s->sampled_at=now;
    for(unsigned i=0;i<p->axis_count;i++) s->axes[i]=p->axes[i].neutral;
    for(unsigned i=0;i<p->encoder_count;i++) osg_encoder_init(&s->decoders[i],ab(sample,p->encoders[i]));
    s->valid=true;return true;
}
void osg_events_set_transform(osg_events_t *s,osg_events_transform_fn transform,void *context) {
    if(!s) return;
    s->transform=transform;s->transform_context=context;
}
static void append_event(osg_events_t *s,uint8_t control,bool pressed) {
    if(s->head==UINT32_MAX) {s->exhausted=true;return;}
    if(s->count==OSG_EVENTS_RING_CAPACITY) {
        s->start=(uint8_t)((s->start+1)%OSG_EVENTS_RING_CAPACITY);
        if(s->overflow<UINT32_MAX) s->overflow++;
    } else s->count++;
    unsigned index=(s->start+s->count-1)%OSG_EVENTS_RING_CAPACITY;
    s->ring[index]=(osg_input_event_t){.sequence=++s->head,.control=control,.pressed=pressed};
}
void osg_events_observe(osg_events_t *s,const osg_profile_sample_t *sample,uint32_t now) {
    if(!s || !s->valid || s->exhausted || !sample) return;
    uint8_t report[OSG_REPORT_CAPACITY];
    osg_profile_update(&s->acquisition,sample,now,true,report);
    if(s->transform) s->transform(s->transform_context,&s->acquisition,report);
    s->sampled_at=now;
    const osg_profile_t *p=s->acquisition.definition;
    for(unsigned i=0;i<p->axis_count;i++) {
        unsigned offset=s->acquisition.layout.axis_offset+2U*i;
        s->axes[i]=(uint16_t)(report[offset]|(uint16_t)report[offset+1]<<8);
    }
    unsigned encoder_end=p->button_count+2U*p->encoder_count;
    for(unsigned control=0;control<s->acquisition.layout.buttons;control++) {
        if(control>=p->button_count && control<encoder_end) continue;
        bool held=(report[control/8]&(1U<<(control%8)))!=0;
        if(held!=s->held[control]) {
            s->held[control]=held;append_event(s,(uint8_t)control,held);
            if(s->exhausted) return;
        }
    }
    for(unsigned i=0;i<p->encoder_count;i++) {
        int8_t direction=osg_encoder_decode(&s->decoders[i],ab(sample,p->encoders[i]));
        if(!direction) continue;
        /* A detent is an atomic pair; never publish half at sequence exhaustion. */
        if(s->head>UINT32_MAX-2U) {s->exhausted=true;return;}
        uint8_t control=(uint8_t)(p->button_count+2U*i+(direction<0));
        append_event(s,control,true);append_event(s,control,false);
    }
}
typedef struct {char *out;size_t capacity,used;bool failed;} writer_t;
static void writef(writer_t *w,const char *format,...) {
    if(w->failed) return;
    va_list args;va_start(args,format);
    int n=vsnprintf(w->out+w->used,w->capacity-w->used,format,args);va_end(args);
    if(n<0 || (size_t)n>=w->capacity-w->used) {w->failed=true;return;}
    w->used+=(size_t)n;
}
static void error(char *reply,size_t capacity,const char *token) {
    if(!reply || !capacity) return;
    int n=snprintf(reply,capacity,"{\"ok\":false,\"error\":\"%s\"}",token);
    if(n<0 || (size_t)n>=capacity) reply[0]=0;
}
static bool number(const char *text,uint32_t *value) {
    if(!*text) return false;
    uint32_t n=0;
    while(*text) {
        if(*text<'0' || *text>'9') return false;
        unsigned digit=(unsigned)(*text++-'0');
        if(n>(UINT32_MAX-digit)/10U) return false;
        n=n*10U+digit;
    }
    *value=n;return true;
}
static void metadata(writer_t *w,const osg_events_t *s) {
    writef(w,"{\"ok\":true,\"schemaVersion\":1,\"stream\":%lu,\"head\":%lu,\"oldest\":%lu,\"overflow\":%lu,\"buttonCount\":%u,\"held\":[",
        (unsigned long)s->stream,(unsigned long)s->head,(unsigned long)(s->head-s->count+1U),
        (unsigned long)s->overflow,s->acquisition.layout.buttons);
    for(unsigned i=0;i<s->acquisition.layout.buttons;i++) writef(w,"%s%s",i?",":"",s->held[i]?"true":"false");
    writef(w,"],\"encoderButtons\":[");
    const osg_profile_t *p=s->acquisition.definition;
    for(unsigned i=0;i<2U*p->encoder_count;i++) writef(w,"%s%u",i?",":"",p->button_count+i);
    writef(w,"],\"contactButtonCount\":%u,\"maintainedButtons\":[",p->button_count);
    bool comma=false;
    for(unsigned i=0;i<p->button_count;i++) if(p->buttons[i].maintained) {
        writef(w,"%s%u",comma?",":"",i);comma=true;
    }
    writef(w,"],\"selectorButtons\":[");
    unsigned base=p->button_count+2U*p->encoder_count;
    for(unsigned i=0;i<p->selector_count;i++) {
        unsigned first=base+3U*i;
        writef(w,"%s[%u,%u,%u]",i?",":"",first,first+1U,first+2U);
    }
    writef(w,"],\"axisCount\":%u,\"axisUsage\":[",p->axis_count);
    for(unsigned i=0;i<p->axis_count;i++) writef(w,"%s%u",i?",":"",p->axes[i].usage);
    writef(w,"],\"axisNeutral\":[");
    for(unsigned i=0;i<p->axis_count;i++) writef(w,"%s%u",i?",":"",p->axes[i].neutral);
    writef(w,"],\"axes\":[");
    for(unsigned i=0;i<p->axis_count;i++) writef(w,"%s%u",i?",":"",s->axes[i]);
    writef(w,"],\"axisAvailable\":[");
    for(unsigned i=0;i<p->axis_count;i++) writef(w,"%s%s",i?",":"",(s->acquisition.axis_available&(1U<<i))?"true":"false");
    writef(w,"],\"sampledAtMs\":%lu",(unsigned long)s->sampled_at);
}
bool osg_events_command(const osg_events_t *s,const char *line,char *reply,size_t capacity) {
    if(!line || strncmp(line,"events",6) || (line[6] && line[6]!=' ' && line[6]!='\t')) return false;
    if(!s || !s->valid) {error(reply,capacity,"invalidStream");return true;}
    if(s->exhausted) {error(reply,capacity,"streamExhausted");return true;}
    if(!reply || capacity<OSG_EVENTS_REPLY_CAPACITY) {error(reply,capacity,"replyTooSmall");return true;}
    if(strlen(line)>=OSG_EVENTS_COMMAND_CAPACITY) {error(reply,capacity,"invalidArgument");return true;}
    char command[OSG_EVENTS_COMMAND_CAPACITY];strcpy(command,line);
    char *tokens[4];unsigned count=0;
    for(char *p=command;*p;) {
        while(*p==' ' || *p=='\t') p++;
        if(!*p) break;
        if(count==4) {error(reply,capacity,"invalidArgument");return true;}
        tokens[count++]=p;while(*p && *p!=' ' && *p!='\t') p++;
        if(*p) *p++=0;
    }
    bool reading=count>=2 && !strcmp(tokens[1],"read");
    uint32_t stream=0,after=0;
    if(reading) {
        if(count!=4 || !number(tokens[2],&stream) || !number(tokens[3],&after)) {error(reply,capacity,"invalidArgument");return true;}
        if(stream!=s->stream) {error(reply,capacity,"staleStream");return true;}
        if(after>s->head) {error(reply,capacity,"invalidCursor");return true;}
        if(after<s->head-s->count) {error(reply,capacity,"eventGap");return true;}
    } else if(count!=2 || strcmp(tokens[1],"state")) {error(reply,capacity,"unknownCommand");return true;}
    writer_t w={.out=reply,.capacity=OSG_EVENTS_REPLY_CAPACITY-4};metadata(&w,s);
    if(reading) {
        uint32_t remaining=s->head-after;
        unsigned amount=remaining>OSG_EVENTS_PAGE_SIZE?OSG_EVENTS_PAGE_SIZE:(unsigned)remaining;
        uint32_t next=after+amount;
        writef(&w,",\"after\":%lu,\"next\":%lu,\"more\":%s,\"events\":[",
            (unsigned long)after,(unsigned long)next,next<s->head?"true":"false");
        unsigned offset=(unsigned)(after-(s->head-s->count));
        for(unsigned i=0;i<amount;i++) {
            const osg_input_event_t *event=&s->ring[(s->start+offset+i)%OSG_EVENTS_RING_CAPACITY];
            writef(&w,"%s{\"sequence\":%lu,\"control\":%u,\"pressed\":%s}",i?",":"",
                (unsigned long)event->sequence,event->control,event->pressed?"true":"false");
        }
        writef(&w,"]");
    }
    writef(&w,"}");if(w.failed) error(reply,capacity,"replyTooSmall");return true;
}
