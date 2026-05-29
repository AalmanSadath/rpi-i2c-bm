#ifndef I2C_RESULT_H
#define I2C_RESULT_H

typedef enum {
    I2C_SUCCESS = 0,
    I2C_ERROR_NACK,
    I2C_ERROR_CLKT,
    I2C_ERROR_TIMEOUT,
    I2C_ERROR_INVALID_PARAM,
    I2C_ERROR_HW_INIT,
} i2c_result_t;

#endif /* I2C_RESULT_H */
