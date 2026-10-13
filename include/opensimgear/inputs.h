#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool candidate, stable, ready;
    uint32_t changed_at;
} osg_contact_t;
void osg_contact_init(osg_contact_t *contact, bool closed, uint32_t now);
bool osg_contact_update(osg_contact_t *contact, bool closed, uint32_t now);
/* NESW bits 0..3; output 0..7 clockwise from north, 8 neutral/conflict. */
uint8_t osg_hat_position(uint8_t contacts);
uint16_t osg_axis_scale(uint16_t raw, uint16_t minimum, uint16_t maximum);

typedef struct {
    uint8_t previous, head, count, phase;
    int8_t steps, direction, queue[16];
    bool synchronized;
    uint32_t phase_at, dropped, invalid;
} osg_encoder_t;
/* Active-low mechanical quadrature contacts, both open = electrical AB 3.
 * One full four-transition cycle returning to AB 3 represents one detent. */
void osg_encoder_init(osg_encoder_t *encoder, uint8_t ab);
void osg_encoder_rearm(osg_encoder_t *encoder, uint8_t ab);
/* Decode a completed detent without queuing/acknowledging a HID pulse. */
int8_t osg_encoder_decode(osg_encoder_t *encoder, uint8_t ab);
void osg_encoder_sample(osg_encoder_t *encoder, uint8_t ab);
/* -1/0/+1 current direction button. Press and release each require a host
 * completion acknowledgement and at least 25 ms before advancing. */
int8_t osg_encoder_output(osg_encoder_t *encoder, uint32_t now);
void osg_encoder_reported(osg_encoder_t *encoder, int8_t direction, uint32_t now);
