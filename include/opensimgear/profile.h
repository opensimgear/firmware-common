#pragma once
#include "opensimgear/button.h"
#include "opensimgear/inputs.h"
#include <stddef.h>

enum { OSG_ADC_CHANNELS=8, OSG_DIGITAL_LINES=24, OSG_EXPANDERS=8,
       OSG_EXPANDER_INPUTS=16, OSG_INPUTS=152, OSG_MAX_BUTTONS=128,
       OSG_MAX_HATS=4, OSG_MAX_ENCODERS=8, OSG_MAX_SELECTORS=8,
       OSG_REPORT_CAPACITY=64, OSG_DESCRIPTOR_CAPACITY=256 };
/* Contact indexes 0..23 are board D0..D23; 24 + board*16 + pin selects
 * an active-low contact on TCA9555 address 0x20 + board. Disabled controls
 * are absent from these arrays, not dummy HID fields. */
typedef struct { uint8_t input; bool maintained; uint8_t exclusive_group; } osg_profile_button_t;
typedef struct { uint8_t channel, usage; uint16_t minimum, maximum, neutral; } osg_profile_axis_t;
typedef struct { uint8_t inputs[4]; } osg_profile_hat_t;
typedef struct { uint8_t a, b; } osg_profile_encoder_t;
typedef struct { uint8_t low, high; } osg_profile_selector_t;
typedef struct {
    const char *name;
    const osg_profile_button_t *buttons; uint8_t button_count;
    const osg_profile_axis_t *axes; uint8_t axis_count;
    const osg_profile_hat_t *hats; uint8_t hat_count;
    const osg_profile_encoder_t *encoders; uint8_t encoder_count;
    const osg_profile_selector_t *selectors; uint8_t selector_count;
    uint8_t expander_mask;
} osg_profile_t;
typedef struct {
    uint32_t digital_closed;
    uint16_t axes[OSG_ADC_CHANNELS], expansion_closed[OSG_EXPANDERS];
    uint8_t axes_fresh, expansion_available;
} osg_profile_sample_t;
typedef struct {
    uint8_t buttons, axes, hats, length, hat_offset, axis_offset, adc_mask;
    uint32_t digital_mask;
} osg_profile_layout_t;
typedef enum { OSG_PROFILE_OK, OSG_PROFILE_INVALID, OSG_PROFILE_OVERLAP,
               OSG_PROFILE_TOO_LARGE } osg_profile_result_t;
typedef struct {
    const osg_profile_t *definition; osg_profile_layout_t layout;
    osg_button_t buttons[OSG_MAX_BUTTONS];
    osg_contact_t maintained[OSG_MAX_BUTTONS], hats[OSG_MAX_HATS][4], selectors[OSG_MAX_SELECTORS][2];
    osg_encoder_t encoders[OSG_MAX_ENCODERS];
    uint16_t axes[OSG_ADC_CHANNELS]; uint32_t axis_at[OSG_ADC_CHANNELS];
    uint8_t axis_seen, axis_available, hat_armed, hat_conflict, selector_conflict;
    uint8_t expansion_available, expansion_faults;
    bool exclusive_blocked[256];
} osg_profile_state_t;
osg_profile_result_t osg_profile_validate(const osg_profile_t *, osg_profile_layout_t *);
size_t osg_profile_descriptor(const osg_profile_t *, uint8_t *, size_t);
bool osg_profile_init(osg_profile_state_t *, const osg_profile_t *, const osg_profile_sample_t *, uint32_t);
void osg_profile_rearm(osg_profile_state_t *, const osg_profile_sample_t *, uint32_t);
void osg_profile_neutral(const osg_profile_state_t *, uint8_t report[OSG_REPORT_CAPACITY]);
void osg_profile_update(osg_profile_state_t *, const osg_profile_sample_t *, uint32_t, bool, uint8_t report[OSG_REPORT_CAPACITY]);
void osg_profile_reported(osg_profile_state_t *, const uint8_t *, uint32_t);
