#include "opensimgear/usb_descriptor.h"
#include <assert.h>
#include <string.h>

static void check_configuration(const uint8_t *output, size_t length, bool has_hid) {
    assert(length > 9 && length == (size_t)(output[2] | output[3] << 8));
    assert(length == TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + (has_hid ? TUD_HID_DESC_LEN : 0));
    unsigned interfaces = 0, hid = 0, cdc = 0, endpoints = 0;
    bool endpoint_seen[256] = {false}, interface_seen[256] = {false};
    for (size_t n = 0; n < length; n += output[n]) {
        assert(output[n] >= 2 && n + output[n] <= length);
        if (output[n + 1] == TUSB_DESC_INTERFACE) {
            uint8_t number = output[n + 2], device_class = output[n + 5];
            assert(!interface_seen[number]); interface_seen[number] = true;
            if (device_class == TUSB_CLASS_CDC) assert(number == 0);
            if (device_class == TUSB_CLASS_CDC_DATA) assert(number == 1);
            if (device_class == TUSB_CLASS_HID) assert(number == 2 && has_hid);
            interfaces++; hid += output[n + 5] == TUSB_CLASS_HID;
            cdc += output[n + 5] == TUSB_CLASS_CDC || output[n + 5] == TUSB_CLASS_CDC_DATA;
        }
        if (output[n + 1] == TUSB_DESC_ENDPOINT) {
            assert(!endpoint_seen[output[n + 2]]); endpoint_seen[output[n + 2]] = true; endpoints++;
        }
        if (output[n + 1] == HID_DESC_TYPE_HID) {
            assert(has_hid && (output[n + 7] | output[n + 8] << 8) == 37);
        }
    }
    assert(interfaces == output[4] && interfaces == (has_hid ? 3u : 2u));
    assert(cdc == 2 && hid == (has_hid ? 1u : 0u) && endpoints == (has_hid ? 4u : 3u));
    assert(interface_seen[0] && interface_seen[1] && interface_seen[2] == has_hid);
    assert(endpoint_seen[0x81] && endpoint_seen[0x02] && endpoint_seen[0x82]);
    assert(endpoint_seen[0x83] == has_hid);
}

int main(void) {
    tusb_desc_device_t d = osg_usb_device_descriptor(0x1234, 0x5678, 0x0102);
    assert(d.idVendor == 0x1234 && d.idProduct == 0x5678 && d.bcdDevice == 0x0102);
    assert(d.bNumConfigurations == 1 && d.iSerialNumber == 3);
    uint8_t output[256]; memset(output, 0xa5, sizeof output);
    assert(osg_usb_hid_cdc_configuration(output, 5, 37) == 0 && output[0] == 0xa5);
    assert(osg_usb_hid_cdc_configuration(NULL, sizeof output, 37) == 0);
    size_t length = osg_usb_hid_cdc_configuration(output, sizeof output, 37);
    check_configuration(output, length, true);
    assert(output[length] == 0xa5);
    uint8_t cdc_output[256]; memset(cdc_output, 0xa5, sizeof cdc_output);
    assert(osg_usb_cdc_configuration(NULL, sizeof cdc_output) == 0);
    assert(osg_usb_cdc_configuration(cdc_output, TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN - 1) == 0);
    for (size_t i = 0; i < sizeof cdc_output; i++) assert(cdc_output[i] == 0xa5);
    size_t cdc_length = osg_usb_cdc_configuration(cdc_output, sizeof cdc_output);
    check_configuration(cdc_output, cdc_length, false);
    assert(cdc_output[cdc_length] == 0xa5);
    /* Toggling HID leaves the CDC function, numbering and endpoints unchanged. */
    assert(memcmp(output + TUD_CONFIG_DESC_LEN, cdc_output + TUD_CONFIG_DESC_LEN, TUD_CDC_DESC_LEN) == 0);
    assert(osg_usb_cdc_configuration(cdc_output, cdc_length) == cdc_length);
    return 0;
}
