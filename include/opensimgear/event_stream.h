#pragma once
#include "opensimgear/events.h"
#include <stddef.h>

typedef enum {
    OSG_EVENT_STREAM_FAULT=-1,
    OSG_EVENT_STREAM_IDLE=0,
    OSG_EVENT_STREAM_READY=1,
    OSG_EVENT_STREAM_BLOCKED=2
} osg_event_stream_result_t;

typedef struct {
    uint32_t boot,stream,cursor,next_sequence,last_health_at;
    uint32_t last_head;
    uint16_t axes[OSG_ADC_CHANNELS];
    uint8_t axis_count;
    uint32_t axis_available;
    bool active,baseline_due,pending,pending_more,sequence_exhausted,source_exhausted;
} osg_event_stream_t;

/* Starts an independent producer. Its first prepared frame is a baseline. */
bool osg_event_stream_start(osg_event_stream_t *,const osg_events_t *,uint32_t boot,uint32_t now);
void osg_event_stream_stop(osg_event_stream_t *);
/* A READY frame lives in caller storage unchanged until fully sent + frame_sent().
 * Prepare advances the cursor only when it successfully formats that frame. */
osg_event_stream_result_t osg_event_stream_prepare(osg_event_stream_t *,const osg_events_t *,
    uint32_t now,char *frame,size_t capacity,size_t *length);
void osg_event_stream_frame_sent(osg_event_stream_t *);
