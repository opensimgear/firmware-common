#include "opensimgear/usb_descriptor.h"
#include "device/dcd.h"
#include <assert.h>
#include <string.h>

/* Run the pinned device stack against a controller stub. This tests driver
 * binding and reset; it does not emulate a physical USB bus or Windows. */
static bool enabled_hid;
static bool opened[256];
static unsigned stalls, hid_reports;
static uint8_t last_hid_report[64];
static uint16_t last_hid_length;
static const uint8_t report_descriptor[] = {
    0x05,0x01,0x09,0x04,0xa1,0x01,0x05,0x09,0x19,0x01,0x29,0x01,
    0x15,0x00,0x25,0x01,0x75,0x01,0x95,0x01,0x81,0x02,
    0x75,0x07,0x95,0x01,0x81,0x03,0xc0
};

uint32_t tusb_time_millis_api(void) { return 0; }
bool dcd_init(uint8_t port,const tusb_rhport_init_t *init) { (void)port; (void)init; return true; }
void dcd_int_handler(uint8_t port) { (void)port; }
void dcd_int_enable(uint8_t port) { (void)port; }
void dcd_int_disable(uint8_t port) { (void)port; }
void dcd_set_address(uint8_t port,uint8_t address) { (void)address; dcd_edpt_xfer(port,0x80,NULL,0,false); }
void dcd_remote_wakeup(uint8_t port) { (void)port; }
void dcd_sof_enable(uint8_t port,bool enable) { (void)port; (void)enable; }
void dcd_edpt_close_all(uint8_t port) { (void)port; memset(opened,0,sizeof opened); }
bool dcd_edpt_open(uint8_t port,const tusb_desc_endpoint_t *endpoint) {
    assert(port==0 && !opened[endpoint->bEndpointAddress]);
    opened[endpoint->bEndpointAddress]=true; return true;
}
bool dcd_edpt_xfer(uint8_t port,uint8_t endpoint,uint8_t *buffer,uint16_t length,bool is_isr) {
    (void)is_isr;
    if(endpoint==0x83) {
        assert(opened[endpoint] && length<=sizeof last_hid_report);
        memcpy(last_hid_report,buffer,length); last_hid_length=length; hid_reports++;
        dcd_event_xfer_complete(port,endpoint,length,XFER_RESULT_SUCCESS,false);
    } else if((endpoint&0x7f)==0) {
        dcd_event_xfer_complete(port,endpoint,length,XFER_RESULT_SUCCESS,false);
    }
    return true;
}
void dcd_edpt_stall(uint8_t port,uint8_t endpoint) { (void)port; (void)endpoint; stalls++; }
void dcd_edpt_clear_stall(uint8_t port,uint8_t endpoint) { (void)port; (void)endpoint; }
bool dcd_edpt_iso_alloc(uint8_t port,uint8_t endpoint,uint16_t length) {
    (void)port; (void)endpoint; (void)length; return false;
}
bool dcd_edpt_iso_activate(uint8_t port,const tusb_desc_endpoint_t *endpoint) {
    (void)port; (void)endpoint; return false;
}

const uint8_t *tud_descriptor_device_cb(void) {
    static tusb_desc_device_t descriptor;
    descriptor=osg_usb_device_descriptor(0xffff,0xffff,0x0100); return (const uint8_t *)&descriptor;
}
const uint8_t *tud_descriptor_configuration_cb(uint8_t index) {
    static uint8_t descriptor[TUD_CONFIG_DESC_LEN+TUD_CDC_DESC_LEN+TUD_HID_DESC_LEN];
    if(index) return NULL;
    size_t length=enabled_hid ? osg_usb_hid_cdc_configuration(descriptor,sizeof descriptor,sizeof report_descriptor) :
                               osg_usb_cdc_configuration(descriptor,sizeof descriptor);
    assert(length); return descriptor;
}
const uint16_t *tud_descriptor_string_cb(uint8_t index,uint16_t langid) { (void)index; (void)langid; return NULL; }
const uint8_t *tud_hid_descriptor_report_cb(uint8_t instance) {
    assert(enabled_hid && instance==0); return report_descriptor;
}
uint16_t tud_hid_get_report_cb(uint8_t instance,uint8_t id,hid_report_type_t type,uint8_t *buffer,uint16_t length) {
    (void)instance; (void)id; (void)type; (void)buffer; (void)length; return 0;
}
void tud_hid_set_report_cb(uint8_t instance,uint8_t id,hid_report_type_t type,const uint8_t *buffer,uint16_t length) {
    (void)instance; (void)id; (void)type; (void)buffer; (void)length;
}

static void enumerate(bool has_hid) {
    enabled_hid=has_hid;
    memset(opened,0,sizeof opened);
    dcd_event_bus_reset(0,TUSB_SPEED_FULL,false);
    tud_task_ext(0,false);
    assert(!tud_mounted() && !tud_hid_ready());
    const tusb_control_request_t request={.bmRequestType=0,.bRequest=TUSB_REQ_SET_CONFIGURATION,.wValue=1};
    dcd_event_setup_received(0,(const uint8_t *)&request,false);
    tud_task_ext(0,false);
    assert(tud_mounted() && tud_cdc_ready());
    assert(opened[0x81] && opened[0x02] && opened[0x82]);
    assert(opened[0x83]==has_hid && tud_hid_ready()==has_hid && stalls==0);
    if(has_hid) {
        const uint8_t input=1;
        assert(tud_hid_report(1,&input,sizeof input));
        tud_task_ext(0,false);
        assert(tud_hid_ready() && last_hid_length==2 && last_hid_report[0]==1 && last_hid_report[1]==input);
    }
}

int main(void) {
    const tusb_rhport_init_t init={.role=TUSB_ROLE_DEVICE,.speed=TUSB_SPEED_FULL};
    assert(tusb_init(0,&init));
    enumerate(false);
    assert(hid_reports==0);
    enumerate(true);
    assert(hid_reports==1);
    enumerate(false);
    assert(hid_reports==1);
    enumerate(true);
    assert(hid_reports==2);
    return 0;
}
