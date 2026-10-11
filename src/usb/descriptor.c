#include "opensimgear/usb_descriptor.h"
#include <string.h>

tusb_desc_device_t osg_usb_device_descriptor(uint16_t vid, uint16_t pid, uint16_t revision) {
    return (tusb_desc_device_t){
        .bLength = sizeof(tusb_desc_device_t), .bDescriptorType = TUSB_DESC_DEVICE,
        .bcdUSB = 0x0200, .bDeviceClass = TUSB_CLASS_MISC,
        .bDeviceSubClass = MISC_SUBCLASS_COMMON, .bDeviceProtocol = MISC_PROTOCOL_IAD,
        .bMaxPacketSize0 = 64, .idVendor = vid, .idProduct = pid,
        .bcdDevice = revision, .iManufacturer = 1, .iProduct = 2,
        .iSerialNumber = 3, .bNumConfigurations = 1};
}

size_t osg_usb_hid_cdc_configuration(uint8_t *buffer, size_t capacity, uint16_t report_length) {
    const uint8_t descriptor[] = {
        TUD_CONFIG_DESCRIPTOR(1, 3, 0, TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN, 0, 100),
        TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64),
        TUD_HID_DESCRIPTOR(2, 5, HID_ITF_PROTOCOL_NONE, report_length, 0x83, 8, 1)
    };
    if (!buffer || capacity < sizeof(descriptor)) return 0;
    memcpy(buffer, descriptor, sizeof(descriptor));
    return sizeof(descriptor);
}
