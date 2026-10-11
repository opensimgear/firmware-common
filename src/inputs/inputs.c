#include "opensimgear/inputs.h"
#include <string.h>

void osg_contact_init(osg_contact_t *c, bool closed, uint32_t now) {
    *c = (osg_contact_t){.candidate = closed, .changed_at = now};
}
bool osg_contact_update(osg_contact_t *c, bool closed, uint32_t now) {
    if (closed != c->candidate) { c->candidate = closed; c->changed_at = now; }
    if ((uint32_t)(now - c->changed_at) >= 10) { c->stable = c->candidate; c->ready = true; }
    return c->stable;
}
uint8_t osg_hat_position(uint8_t contacts) {
    switch (contacts) {
        case 1: return 0; case 3: return 1; case 2: return 2; case 6: return 3;
        case 4: return 4; case 12: return 5; case 8: return 6; case 9: return 7;
        default: return 8;
    }
}
uint16_t osg_axis_scale(uint16_t raw, uint16_t minimum, uint16_t maximum) {
    if (minimum >= maximum) return 32768;
    if (raw <= minimum) return 0;
    if (raw >= maximum) return 65535;
    uint32_t span = (uint32_t)maximum - minimum;
    return (uint16_t)(((uint32_t)(raw - minimum) * 65535U + span / 2U) / span);
}
enum { IDLE, WAIT_PRESS, HOLD, WAIT_RELEASE, GAP };
void osg_encoder_rearm(osg_encoder_t *e, uint8_t ab) {
    e->previous = ab & 3; e->synchronized = e->previous == 3;
    e->head = e->count = e->phase = e->steps = e->direction = 0;
    e->phase_at = 0;
}
void osg_encoder_init(osg_encoder_t *e, uint8_t ab) {
    memset(e, 0, sizeof *e); osg_encoder_rearm(e, ab);
}
void osg_encoder_sample(osg_encoder_t *e, uint8_t ab) {
    static const int8_t delta[16] = {0,1,-1,0, -1,0,0,1, 1,0,0,-1, 0,-1,1,0};
    ab &= 3;
    uint8_t previous = e->previous; e->previous = ab;
    if ((previous ^ ab) == 3) {
        e->invalid++; e->steps = 0; e->synchronized = ab == 3; return;
    }
    if (!e->synchronized) { if (ab == 3) e->synchronized = true; return; }
    e->steps += delta[previous * 4 + ab];
    if (ab != 3) return;
    int8_t direction = e->steps == 4 ? 1 : e->steps == -4 ? -1 : 0;
    e->steps = 0;
    if (!direction) return;
    if (e->count == sizeof e->queue) { e->dropped++; return; }
    e->queue[(e->head + e->count) % sizeof e->queue] = direction; e->count++;
}
int8_t osg_encoder_output(osg_encoder_t *e, uint32_t now) {
    if (e->phase == HOLD && (uint32_t)(now - e->phase_at) >= 25) e->phase = WAIT_RELEASE;
    if (e->phase == GAP && (uint32_t)(now - e->phase_at) >= 25) e->phase = IDLE;
    if (e->phase == IDLE && e->count) {
        e->direction = e->queue[e->head]; e->head = (e->head + 1) % sizeof e->queue;
        e->count--; e->phase = WAIT_PRESS;
    }
    return e->phase == WAIT_PRESS || e->phase == HOLD ? e->direction : 0;
}
void osg_encoder_reported(osg_encoder_t *e, int8_t direction, uint32_t now) {
    if (e->phase == WAIT_PRESS && direction == e->direction) { e->phase = HOLD; e->phase_at = now; }
    else if (e->phase == WAIT_RELEASE && direction == 0) { e->phase = GAP; e->phase_at = now; }
}
