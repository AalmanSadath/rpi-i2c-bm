#ifndef ADXL345_H
#define ADXL345_H

#include <stdint.h>
#include "i2c_hal.h"

/* I2C addresses: SDO/ALT_ADDRESS pin LOW = 0x53, HIGH = 0x1D */
#define ADXL345_ADDR_LO  0x53U
#define ADXL345_ADDR_HI  0x1DU

#define ADXL345_DEVID    0xE5U

/* Register map */
#define ADXL345_REG_DEVID        0x00U
#define ADXL345_REG_BW_RATE      0x2CU
#define ADXL345_REG_POWER_CTL    0x2DU
#define ADXL345_REG_DATA_FORMAT  0x31U
#define ADXL345_REG_DATAX0       0x32U

/* POWER_CTL bits */
#define ADXL345_MEASURE   (1U << 3)

/* DATA_FORMAT bits */
#define ADXL345_FULL_RES  (1U << 3)
#define ADXL345_RANGE_2G  0x00U
#define ADXL345_RANGE_4G  0x01U
#define ADXL345_RANGE_8G  0x02U
#define ADXL345_RANGE_16G 0x03U

/* 3.9 mg per LSB in full-resolution mode */
#define ADXL345_SCALE_G   0.0039f

typedef struct {
    i2c_dev_t *i2c;
    uint8_t    addr;
} adxl345_t;

typedef struct {
    float x, y, z;  /* acceleration in g */
} adxl345_accel_t;

void          adxl345_init(adxl345_t *dev, i2c_dev_t *i2c, uint8_t addr);
i2c_result_t  adxl345_verify_id(adxl345_t *dev);
i2c_result_t  adxl345_start(adxl345_t *dev, uint8_t range);
i2c_result_t  adxl345_read_accel(adxl345_t *dev, adxl345_accel_t *out);
i2c_result_t  adxl345_stop(adxl345_t *dev);

#endif /* ADXL345_H */
