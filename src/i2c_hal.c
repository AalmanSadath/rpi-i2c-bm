#include "i2c_hal.h"
#include "bsc_protocol.h"
#include <stdlib.h>

struct i2c_dev {
    bsc_periph_t *periph;
};

i2c_dev_t *i2c_open(uint32_t clock_hz) {
    bsc_periph_t *p = bsc_open(clock_hz);
    if (!p) return NULL;
    i2c_dev_t *dev = malloc(sizeof(*dev));
    if (!dev) { bsc_close(p); return NULL; }
    dev->periph = p;
    return dev;
}

void i2c_close(i2c_dev_t *dev) {
    if (!dev) return;
    bsc_close(dev->periph);
    free(dev);
}

void i2c_set_slave(i2c_dev_t *dev, uint8_t addr) {
    if (dev) bsc_set_addr(dev->periph, addr);
}

volatile uint32_t *i2c_get_base(i2c_dev_t *dev) {
    return dev ? bsc_get_base(dev->periph) : NULL;
}

i2c_result_t i2c_write(i2c_dev_t *dev, const uint8_t *data, uint32_t len) {
    if (!dev) return I2C_ERROR_INVALID_PARAM;
    return bsc_write(dev->periph, data, len);
}

i2c_result_t i2c_read(i2c_dev_t *dev, uint8_t *data, uint32_t len) {
    if (!dev) return I2C_ERROR_INVALID_PARAM;
    return bsc_read(dev->periph, data, len);
}

i2c_result_t i2c_write_read(i2c_dev_t *dev,
                              const uint8_t *wdata, uint32_t wlen,
                              uint8_t *rdata, uint32_t rlen) {
    if (!dev) return I2C_ERROR_INVALID_PARAM;
    return bsc_write_read(dev->periph, wdata, wlen, rdata, rlen);
}

const char *i2c_result_str(i2c_result_t r) {
    switch (r) {
        case I2C_SUCCESS:             return "Success";
        case I2C_ERROR_NACK:          return "NACK error (slave did not acknowledge)";
        case I2C_ERROR_CLKT:          return "Clock stretch timeout";
        case I2C_ERROR_TIMEOUT:       return "Software timeout";
        case I2C_ERROR_INVALID_PARAM: return "Invalid parameter";
        case I2C_ERROR_HW_INIT:       return "Hardware initialization failed";
        default:                      return "Unknown error";
    }
}
