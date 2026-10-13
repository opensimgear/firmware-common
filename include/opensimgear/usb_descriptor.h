#pragma once
#include <stddef.h>
#include <stdint.h>
#include "tusb.h"

/* Caller-owned USB identity is unchanged by either configuration topology. */
tusb_desc_device_t osg_usb_device_descriptor(uint16_t vid, uint16_t pid, uint16_t revision);
/* CDC control/data interfaces 0/1; no HID interface or input endpoint. */
size_t osg_usb_cdc_configuration(uint8_t *buffer, size_t capacity);
/* CDC interfaces 0/1 plus HID interface 2. */
size_t osg_usb_hid_cdc_configuration(uint8_t *buffer, size_t capacity, uint16_t report_length);
