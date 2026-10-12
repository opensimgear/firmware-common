#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Address is a seven-bit I2C address. Callbacks must have bounded execution;
 * their platform adapter owns timeouts and bus recovery. */
typedef bool (*osg_i2c_write_t)(void *, uint8_t, uint8_t, const uint8_t *, uint8_t);
typedef bool (*osg_i2c_read_t)(void *, uint8_t, uint8_t, uint8_t *, uint8_t);
typedef struct { void *context; osg_i2c_write_t write; osg_i2c_read_t read; } osg_i2c_t;
typedef struct {
    osg_i2c_t bus; uint8_t configured, available, next;
    uint32_t failures[8], updated_at[8], polled_at;
    uint16_t closed[8];
} osg_tca9555_t;
void osg_tca9555_init(osg_tca9555_t *, osg_i2c_t);
/* One address per call, suitable for a round-robin scheduler. Every failed
 * transaction invalidates the bank and forces configuration before retry. */
bool osg_tca9555_sample(osg_tca9555_t *, uint8_t bank, uint16_t *closed);
/* At most one enabled address per tick; healthy banks retain independent
 * freshness when other addresses are missing. Ages use wrap-safe subtraction. */
void osg_tca9555_poll(osg_tca9555_t *, uint8_t enabled_mask, uint32_t now);
uint8_t osg_tca9555_inputs(osg_tca9555_t *, uint32_t now, uint16_t closed[8]);
