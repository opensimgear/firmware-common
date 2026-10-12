#include "opensimgear/usb_descriptor.h"
#include <assert.h>
#include <string.h>
int main(void) {
    tusb_desc_device_t d = osg_usb_device_descriptor(0x1234, 0x5678, 0x0102);
    assert(d.idVendor == 0x1234 && d.idProduct == 0x5678 && d.bcdDevice == 0x0102);
    assert(d.bNumConfigurations == 1 && d.iSerialNumber != 0);
    uint8_t output[256]; memset(output, 0xa5, sizeof output);
    assert(osg_usb_hid_cdc_configuration(output, 5, 37) == 0 && output[0] == 0xa5);
    assert(osg_usb_hid_cdc_configuration(NULL, sizeof output, 37) == 0);
    size_t length = osg_usb_hid_cdc_configuration(output, sizeof output, 37);
    assert(length > 9 && length == (size_t)(output[2] | output[3] << 8));
    unsigned interfaces = 0, hid = 0, cdc = 0, endpoints = 0;
    bool endpoint_seen[256] = {false}, interface_seen[256] = {false};
    for (size_t n = 0; n < length; n += output[n]) {
        assert(output[n] >= 2 && n + output[n] <= length);
        if (output[n + 1] == TUSB_DESC_INTERFACE) {
            assert(!interface_seen[output[n + 2]]); interface_seen[output[n + 2]] = true;
            interfaces++; hid += output[n + 5] == TUSB_CLASS_HID;
            cdc += output[n + 5] == TUSB_CLASS_CDC || output[n + 5] == TUSB_CLASS_CDC_DATA;
        }
        if (output[n + 1] == TUSB_DESC_ENDPOINT) {
            assert(!endpoint_seen[output[n + 2]]); endpoint_seen[output[n + 2]] = true; endpoints++;
        }
        if (output[n + 1] == HID_DESC_TYPE_HID) assert((output[n + 7] | output[n + 8] << 8) == 37);
    }
    assert(interfaces == output[4] && interfaces == 3 && cdc == 2 && hid == 1 && endpoints == 4);
    return 0;
}
