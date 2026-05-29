/* Backward-compatibility shim — wraps the three-layer HAL.
 * New code should use i2c_hal.h directly.
 * `base` is exposed for legacy callers that need direct register access. */

#ifndef BCM2835_I2C_H
#define BCM2835_I2C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "i2c_hal.h"
#include "bcm2835_bsc_regs.h"

typedef struct {
    volatile uint32_t *base;       /* register pointer — for legacy direct access */
    int                mem_fd;     /* always -1; lifetime managed by HAL           */
    uint32_t           clock_div;
    i2c_dev_t         *_hal;
} bcm2835_i2c_t;

static inline i2c_result_t bcm2835_i2c_init(bcm2835_i2c_t *i2c, uint32_t clock_hz) {
    if (!i2c) return I2C_ERROR_INVALID_PARAM;
    i2c->_hal = i2c_open(clock_hz);
    if (!i2c->_hal) return I2C_ERROR_HW_INIT;
    i2c->base      = i2c_get_base(i2c->_hal);
    i2c->clock_div = BCM2835_CORE_CLK_HZ / clock_hz;
    i2c->mem_fd    = -1;
    return I2C_SUCCESS;
}

static inline void bcm2835_i2c_close(bcm2835_i2c_t *i2c) {
    if (!i2c) return;
    i2c_close(i2c->_hal);
    i2c->_hal      = NULL;
    i2c->base      = NULL;
    i2c->mem_fd    = -1;
}

static inline void bcm2835_i2c_set_slave_addr(bcm2835_i2c_t *i2c, uint8_t addr) {
    if (i2c) i2c_set_slave(i2c->_hal, addr);
}

static inline i2c_result_t bcm2835_i2c_write(bcm2835_i2c_t *i2c,
                                               const uint8_t *data, uint32_t len) {
    if (!i2c) return I2C_ERROR_INVALID_PARAM;
    return i2c_write(i2c->_hal, data, len);
}

static inline i2c_result_t bcm2835_i2c_read(bcm2835_i2c_t *i2c,
                                              uint8_t *data, uint32_t len) {
    if (!i2c) return I2C_ERROR_INVALID_PARAM;
    return i2c_read(i2c->_hal, data, len);
}

static inline i2c_result_t bcm2835_i2c_write_read(bcm2835_i2c_t *i2c,
                                                    const uint8_t *wdata, uint32_t wlen,
                                                    uint8_t *rdata, uint32_t rlen) {
    if (!i2c) return I2C_ERROR_INVALID_PARAM;
    return i2c_write_read(i2c->_hal, wdata, wlen, rdata, rlen);
}

#define i2c_result_to_string(r)  i2c_result_str(r)

#endif /* BCM2835_I2C_H */
