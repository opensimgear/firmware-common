#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    bool candidate, stable, armed;
    uint32_t changed_at;
    uint32_t debounce_ms;
} osg_button_t;

/* Startup/reconnect requires a debounced release before any press. */
void osg_button_rearm(osg_button_t *button, bool pressed, uint32_t now, uint32_t debounce_ms);
bool osg_button_update(osg_button_t *button, bool pressed, uint32_t now);
