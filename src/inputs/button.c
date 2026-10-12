#include "opensimgear/button.h"

void osg_button_rearm(osg_button_t *b, bool pressed, uint32_t now, uint32_t debounce_ms) {
    *b = (osg_button_t){.candidate = pressed, .stable = pressed, .armed = false,
                       .changed_at = now, .debounce_ms = debounce_ms};
}

bool osg_button_update(osg_button_t *b, bool pressed, uint32_t now) {
    if (pressed != b->candidate) {
        b->candidate = pressed;
        b->changed_at = now;
    }
    if ((uint32_t)(now - b->changed_at) >= b->debounce_ms) {
        b->stable = b->candidate;
        if (!b->stable) b->armed = true;
    }
    return b->armed && b->stable;
}
