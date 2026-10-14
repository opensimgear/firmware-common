#pragma once
#include "opensimgear/profile.h"
#include <stddef.h>

enum { OSG_EVENTS_RING_CAPACITY=64, OSG_EVENTS_PAGE_SIZE=16,
       OSG_EVENTS_REPLY_CAPACITY=4096, OSG_EVENTS_COMMAND_CAPACITY=384 };
typedef struct { uint32_t sequence; uint8_t control; bool pressed; } osg_input_event_t;
/* Product runtime calibrates this independent report, including inversion and
 * deadzone, before axis snapshots are captured. Never pass product HID state. */
typedef void (*osg_events_transform_fn)(void *context,const osg_profile_state_t *state,
                                       uint8_t report[OSG_REPORT_CAPACITY]);
typedef struct {
    /* Separate acquisition never consumes or acknowledges product HID state. */
    osg_profile_state_t acquisition;
    osg_encoder_t decoders[OSG_MAX_ENCODERS];
    osg_input_event_t ring[OSG_EVENTS_RING_CAPACITY];
    bool held[OSG_MAX_BUTTONS],valid,exhausted;
    uint32_t stream,head,overflow;
    uint32_t sampled_at;
    uint16_t axes[OSG_ADC_CHANNELS];
    osg_events_transform_fn transform; void *transform_context;
    uint8_t count,start;
} osg_events_t;
/* Boot scopes stream externally. Reinitialize with a new nonzero stream after
 * input mapping activation. Profile storage remains alive/immutable between
 * initializations; sample pointers are never retained. */
bool osg_events_init(osg_events_t *,const osg_profile_t *,const osg_profile_sample_t *,uint32_t now,uint32_t stream);
/* Optional callback; init clears it, so register again after every activation. */
void osg_events_set_transform(osg_events_t *,osg_events_transform_fn,void *context);
/* Sample independently of direct USB, disabled HID, leases and InputAck. */
void osg_events_observe(osg_events_t *,const osg_profile_sample_t *,uint32_t now);
/* Full `events state` / `events read <stream> <after>` command. Reads never
 * consume the ring. Caller retains immutable transport retry responses. */
bool osg_events_command(const osg_events_t *,const char *,char *reply,size_t capacity);
