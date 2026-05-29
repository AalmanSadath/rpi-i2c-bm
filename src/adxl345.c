#include "adxl345.h"
#include <stddef.h>

static i2c_result_t write_reg(adxl345_t *dev, uint8_t reg, uint8_t val) {
    uint8_t buf[2] = { reg, val };
    i2c_set_slave(dev->i2c, dev->addr);
    return i2c_write(dev->i2c, buf, 2);
}

static i2c_result_t read_reg(adxl345_t *dev, uint8_t reg, uint8_t *out, uint32_t len) {
    i2c_set_slave(dev->i2c, dev->addr);
    return i2c_write_read(dev->i2c, &reg, 1, out, len);
}

void adxl345_init(adxl345_t *dev, i2c_dev_t *i2c, uint8_t addr) {
    dev->i2c  = i2c;
    dev->addr = addr;
}

i2c_result_t adxl345_verify_id(adxl345_t *dev) {
    uint8_t id = 0;
    i2c_result_t r = read_reg(dev, ADXL345_REG_DEVID, &id, 1);
    if (r != I2C_SUCCESS) return r;
    return (id == ADXL345_DEVID) ? I2C_SUCCESS : I2C_ERROR_NACK;
}

i2c_result_t adxl345_start(adxl345_t *dev, uint8_t range) {
    i2c_result_t r;

    /* Full-resolution mode + requested range */
    r = write_reg(dev, ADXL345_REG_DATA_FORMAT, ADXL345_FULL_RES | (range & 0x03U));
    if (r != I2C_SUCCESS) return r;

    /* Enable measurement mode */
    return write_reg(dev, ADXL345_REG_POWER_CTL, ADXL345_MEASURE);
}

i2c_result_t adxl345_read_accel(adxl345_t *dev, adxl345_accel_t *out) {
    uint8_t buf[6];
    i2c_result_t r = read_reg(dev, ADXL345_REG_DATAX0, buf, 6);
    if (r != I2C_SUCCESS) return r;

    int16_t raw_x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t raw_y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t raw_z = (int16_t)((buf[5] << 8) | buf[4]);

    out->x = raw_x * ADXL345_SCALE_G;
    out->y = raw_y * ADXL345_SCALE_G;
    out->z = raw_z * ADXL345_SCALE_G;
    return I2C_SUCCESS;
}

i2c_result_t adxl345_stop(adxl345_t *dev) {
    return write_reg(dev, ADXL345_REG_POWER_CTL, 0x00);
}
