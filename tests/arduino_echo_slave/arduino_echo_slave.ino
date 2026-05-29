/*
 * Simple I2C echo slave for Arduino Leonardo
 * Address: 0x08
 * Used with test_i2c_basic — echoes received bytes back on request.
 */

#include <Wire.h>

#define I2C_ADDR  0x08
#define BUF_SIZE  32

static uint8_t buf[BUF_SIZE];
static uint8_t buf_len = 0;

void receiveHandler(int n) {
  buf_len = 0;
  while (Wire.available() && buf_len < BUF_SIZE)
    buf[buf_len++] = Wire.read();
}

void requestHandler() {
  Wire.write(buf, buf_len);
}

void setup() {
  Wire.begin(I2C_ADDR);
  Wire.onReceive(receiveHandler);
  Wire.onRequest(requestHandler);
}

void loop() {}
