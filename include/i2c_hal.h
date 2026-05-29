#ifndef I2C_HAL_H
#define I2C_HAL_H

#include <stdint.h>
#include "i2c_result.h"

typedef struct i2c_dev i2c_dev_t;

i2c_dev_t          *i2c_open(uint32_t clock_hz);
void                i2c_close(i2c_dev_t *dev);
void                i2c_set_slave(i2c_dev_t *dev, uint8_t addr);
volatile uint32_t  *i2c_get_base(i2c_dev_t *dev);

i2c_result_t  i2c_write(i2c_dev_t *dev, const uint8_t *data, uint32_t len);
i2c_result_t  i2c_read(i2c_dev_t *dev, uint8_t *data, uint32_t len);
i2c_result_t  i2c_write_read(i2c_dev_t *dev,
                               const uint8_t *wdata, uint32_t wlen,
                               uint8_t *rdata, uint32_t rlen);

const char   *i2c_result_str(i2c_result_t r);

#endif /* I2C_HAL_H */
