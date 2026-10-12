#include "opensimgear/tca9555.h"
#include <string.h>
void osg_tca9555_init(osg_tca9555_t *device, osg_i2c_t bus) {
    memset(device,0,sizeof *device); device->bus=bus;
}
bool osg_tca9555_sample(osg_tca9555_t *d, uint8_t bank, uint16_t *closed) {
    if (!d || !closed || bank>=8 || !d->bus.write || !d->bus.read) return false;
    *closed=0;
    uint8_t address=(uint8_t)(0x20+bank), mask=(uint8_t)(1U<<bank), data[2];
    if (!(d->configured&mask)) {
        const uint8_t inputs[2]={0xff,0xff}, polarity[2]={0,0};
        if (!d->bus.write(d->bus.context,address,6,inputs,2) ||
            !d->bus.write(d->bus.context,address,4,polarity,2)) goto failed;
    }
    /* Verify direction/polarity before reading: a reset or another writer
     * cannot silently change the interpretation of contacts. */
    if (!d->bus.read(d->bus.context,address,6,data,2) || data[0]!=0xff || data[1]!=0xff ||
        !d->bus.read(d->bus.context,address,4,data,2) || data[0]!=0 || data[1]!=0 ||
        !d->bus.read(d->bus.context,address,0,data,2)) goto failed;
    *closed=(uint16_t)~((uint16_t)data[0]|(uint16_t)data[1]<<8);
    d->configured|=mask; return true;
failed:
    d->configured&=(uint8_t)~mask; d->failures[bank]++; return false;
}
void osg_tca9555_poll(osg_tca9555_t *d,uint8_t enabled,uint32_t now) {
    if((uint32_t)(now-d->polled_at)<1) return;
    d->polled_at=now; d->available&=enabled;
    for(unsigned n=0;n<8;n++) {
        uint8_t bank=d->next; d->next=(uint8_t)((d->next+1)%8);
        if(!(enabled&(1U<<bank))) continue;
        uint16_t contacts;
        if(osg_tca9555_sample(d,bank,&contacts)) {
            d->closed[bank]=contacts; d->updated_at[bank]=now; d->available|=(uint8_t)(1U<<bank);
        } else { d->closed[bank]=0; d->available&=(uint8_t)~(1U<<bank); }
        break;
    }
}
uint8_t osg_tca9555_inputs(osg_tca9555_t *d,uint32_t now,uint16_t closed[8]) {
    for(unsigned bank=0;bank<8;bank++) {
        if((uint32_t)(now-d->updated_at[bank])>=100) d->available&=(uint8_t)~(1U<<bank);
        closed[bank]=(d->available&(1U<<bank))?d->closed[bank]:0;
    }
    return d->available;
}
