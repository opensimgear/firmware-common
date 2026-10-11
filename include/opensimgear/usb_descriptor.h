#pragma once
#include <stddef.h>
#include <stdint.h>
#include "tusb.h"

/* One HID interface and a CDC function (three interfaces total). */
tusb_desc_device_t osg_usb_device_descriptor(uint16_t vid, uint16_t pid, uint16_t revision);
size_t osg_usb_hid_cdc_configuration(uint8_t *buffer, size_t capacity, uint16_t report_length);
