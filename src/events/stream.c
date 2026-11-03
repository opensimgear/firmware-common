#include "opensimgear/event_stream.h"
#include <stdio.h>
#include <string.h>

static void snapshot_axes(osg_event_stream_t *s,const osg_events_t *e) {
    s->axis_count=e->acquisition.definition->axis_count;
    s->axis_available=e->acquisition.axis_available;
    memcpy(s->axes,e->axes,sizeof s->axes);
    s->last_head=e->head;
}
static bool axes_changed(const osg_event_stream_t *s,const osg_events_t *e) {
    return s->axis_count!=e->acquisition.definition->axis_count ||
        s->axis_available!=e->acquisition.axis_available || memcmp(s->axes,e->axes,sizeof s->axes)!=0;
}
static bool write_frame(osg_event_stream_t *s,const osg_events_t *e,const char *kind,const char *reason,
                        uint32_t cursor,uint32_t next,bool baseline,bool loss,uint32_t now,
                        char *frame,size_t capacity,size_t *length) {
    char state[OSG_EVENTS_REPLY_CAPACITY];
    bool ok=loss || baseline || e->exhausted ? osg_events_state_snapshot(e,state,sizeof state) : false;
    if(!loss && !baseline && !e->exhausted) {
        char command[64];int n=snprintf(command,sizeof command,"events read %lu %lu",
            (unsigned long)e->stream,(unsigned long)cursor);
        if(n<0 || (size_t)n>=sizeof command) return false;
        ok=osg_events_command(e,command,state,sizeof state);
    }
    if(!ok) return false;
    uint32_t sequence=s->next_sequence;
    int n=loss && e->exhausted ? snprintf(frame,capacity,"@events {\"boot\":%lu,\"sequence\":%lu,\"kind\":\"%s\",\"reason\":\"%s\",\"exhausted\":true,\"state\":%s}\r\n",
        (unsigned long)s->boot,(unsigned long)sequence,kind,reason,state) :
        reason ? snprintf(frame,capacity,"@events {\"boot\":%lu,\"sequence\":%lu,\"kind\":\"%s\",\"reason\":\"%s\",\"state\":%s}\r\n",
        (unsigned long)s->boot,(unsigned long)sequence,kind,reason,state) :
        snprintf(frame,capacity,"@events {\"boot\":%lu,\"sequence\":%lu,\"kind\":\"%s\",\"state\":%s}\r\n",
        (unsigned long)s->boot,(unsigned long)sequence,kind,state);
    if(n<0 || (size_t)n>=capacity) return false;
    *length=(size_t)n;s->pending=true;s->next_sequence++;
    if(sequence==UINT32_MAX) s->sequence_exhausted=true;
    s->cursor=next;s->stream=e->stream;s->last_health_at=now;snapshot_axes(s,e);
    s->pending_more=!loss && !baseline && !e->exhausted && next<e->head;
    s->source_exhausted=e->exhausted;
    return true;
}
bool osg_event_stream_start(osg_event_stream_t *s,const osg_events_t *e,uint32_t boot,uint32_t now) {
    if(!s || !e || !e->valid || e->exhausted || !boot || !e->stream) return false;
    memset(s,0,sizeof *s);s->active=true;s->boot=boot;s->stream=e->stream;
    s->next_sequence=1;s->cursor=e->head;s->last_health_at=now;s->baseline_due=true;
    snapshot_axes(s,e);return true;
}
void osg_event_stream_stop(osg_event_stream_t *s) { if(s) memset(s,0,sizeof *s); }
osg_event_stream_result_t osg_event_stream_prepare(osg_event_stream_t *s,const osg_events_t *e,
    uint32_t now,char *frame,size_t capacity,size_t *length) {
    if(length) *length=0;
    if(!s || !s->active || !e || !e->valid || !e->stream || !frame || !capacity || !length)
        return OSG_EVENT_STREAM_FAULT;
    if(s->pending) return OSG_EVENT_STREAM_BLOCKED;
    if(s->sequence_exhausted || !s->next_sequence) return OSG_EVENT_STREAM_FAULT;
    if(s->baseline_due) {
        if(!write_frame(s,e,"baseline",NULL,e->head,e->head,true,false,now,frame,capacity,length))
            return OSG_EVENT_STREAM_FAULT;
        s->baseline_due=false;return OSG_EVENT_STREAM_READY;
    }
    bool changed_scope=s->stream!=e->stream;
    if(changed_scope) {
        const char *reason=e->exhausted?"streamExhausted":"scopeChanged";
        return write_frame(s,e,"loss",reason,e->head,e->head,true,true,now,frame,capacity,length) ?
            OSG_EVENT_STREAM_READY : OSG_EVENT_STREAM_FAULT;
    }
    if(s->source_exhausted) return OSG_EVENT_STREAM_IDLE;
    if(e->exhausted) {
        return write_frame(s,e,"loss","streamExhausted",e->head,e->head,true,true,now,frame,capacity,length) ?
            OSG_EVENT_STREAM_READY : OSG_EVENT_STREAM_FAULT;
    }
    bool gap=s->cursor>e->head || s->cursor<e->head-e->count;
    if(gap) {
        return write_frame(s,e,"loss","eventGap",e->head,e->head,true,true,now,frame,capacity,length) ?
            OSG_EVENT_STREAM_READY : OSG_EVENT_STREAM_FAULT;
    }
    bool axes=axes_changed(s,e),edges=e->head!=s->cursor;
    bool more=s->pending_more;
    if(!edges && !axes && !more && (uint32_t)(now-s->last_health_at)<100U) return OSG_EVENT_STREAM_IDLE;
    uint32_t next=s->cursor;
    if(edges) { uint32_t remaining=e->head-s->cursor;next+=remaining>OSG_EVENTS_PAGE_SIZE?OSG_EVENTS_PAGE_SIZE:remaining; }
    const char *reason=edges?"events":axes?"axes":"health";
    return write_frame(s,e,"update",reason,s->cursor,next,false,false,now,frame,capacity,length) ?
        OSG_EVENT_STREAM_READY : OSG_EVENT_STREAM_FAULT;
}
void osg_event_stream_frame_sent(osg_event_stream_t *s) { if(s) s->pending=false; }
