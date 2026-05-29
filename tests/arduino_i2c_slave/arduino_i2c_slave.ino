/*
 * I2C protocol slave for Arduino Leonardo
 * Address: 0x08
 * Packet format: [CMD][LEN][DATA...][CRC8], max 32 bytes
 *
 * Serial logging is deferred to loop() — never called inside Wire ISR
 * because USB-CDC blocks in interrupt context on the Leonardo.
 */

#include <Wire.h>
#include <string.h>

#define I2C_ADDR  0x08
#define BUF_SIZE  32
#define MAX_DATA  (BUF_SIZE - 3)
#define NUM_REGS  8

// Commands
#define CMD_PING    0x01
#define CMD_ECHO    0x02
#define CMD_WR_REG  0x03
#define CMD_RD_REG  0x04
#define CMD_STATUS  0x05

// Status codes
#define ST_OK       0x00
#define ST_ERR_CRC  0x01
#define ST_ERR_CMD  0x02
#define ST_ERR_LEN  0x03

static uint8_t rxBuf[BUF_SIZE];
static uint8_t txBuf[BUF_SIZE];
static volatile uint8_t rxLen;
static volatile uint8_t txLen;

static uint8_t  regFile[NUM_REGS];
static uint8_t  crcErrCnt;
static uint8_t  cmdErrCnt;
static uint16_t rxPktCnt;

static volatile bool    logPending;
static volatile uint8_t logCmd;
static volatile uint8_t logStatus;

static uint8_t crc8(const uint8_t *d, uint8_t n) {
  uint8_t crc = 0x00;
  for (uint8_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (uint8_t b = 0; b < 8; b++)
      crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
  }
  return crc;
}

static void buildResp(uint8_t status, const uint8_t *data, uint8_t dlen) {
  txBuf[0] = status;
  txBuf[1] = dlen;
  for (uint8_t i = 0; i < dlen; i++) txBuf[2 + i] = data[i];
  txBuf[2 + dlen] = crc8(txBuf, 2 + dlen);
  txLen = 3 + dlen;
}

void receiveHandler(int numBytes) {
  rxLen = 0;
  while (Wire.available() && rxLen < BUF_SIZE)
    rxBuf[rxLen++] = Wire.read();

  if (rxLen < 3) {
    buildResp(ST_ERR_LEN, NULL, 0);
    logCmd = 0x00; logStatus = ST_ERR_LEN; logPending = true;
    return;
  }

  uint8_t cmd  = rxBuf[0];
  uint8_t dlen = rxBuf[1];

  if (rxLen != (uint8_t)(dlen + 3)) {
    crcErrCnt++;
    buildResp(ST_ERR_LEN, NULL, 0);
    logCmd = cmd; logStatus = ST_ERR_LEN; logPending = true;
    return;
  }

  if (crc8(rxBuf, 2 + dlen) != rxBuf[2 + dlen]) {
    crcErrCnt++;
    buildResp(ST_ERR_CRC, NULL, 0);
    logCmd = cmd; logStatus = ST_ERR_CRC; logPending = true;
    return;
  }

  uint8_t *data = &rxBuf[2];
  static uint8_t resp[MAX_DATA];
  uint8_t respLen = 0;
  uint8_t status  = ST_OK;

  switch (cmd) {
    case CMD_PING:
      resp[0] = 0xAB;
      respLen = 1;
      break;

    case CMD_ECHO:
      if (dlen > MAX_DATA) { status = ST_ERR_LEN; break; }
      memcpy(resp, data, dlen);
      respLen = dlen;
      break;

    case CMD_WR_REG:
      if (dlen != 2 || data[0] >= NUM_REGS) { status = ST_ERR_LEN; break; }
      regFile[data[0]] = data[1];
      break;

    case CMD_RD_REG:
      if (dlen != 1 || data[0] >= NUM_REGS) { status = ST_ERR_LEN; break; }
      resp[0] = regFile[data[0]];
      respLen = 1;
      break;

    case CMD_STATUS:
      resp[0] = crcErrCnt;
      resp[1] = cmdErrCnt;
      resp[2] = (uint8_t)(rxPktCnt & 0xFF);
      resp[3] = (uint8_t)(rxPktCnt >> 8);
      respLen = 4;
      break;

    default:
      cmdErrCnt++;
      status = ST_ERR_CMD;
      break;
  }

  buildResp(status, resp, respLen);
  logCmd = cmd; logStatus = status;
  rxPktCnt++;
  logPending = true;
}

void requestHandler() {
  Wire.write(txBuf, txLen);
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 3000);

  memset(regFile, 0x00, sizeof(regFile));
  buildResp(ST_OK, NULL, 0);

  Wire.begin(I2C_ADDR);
  Wire.onReceive(receiveHandler);
  Wire.onRequest(requestHandler);

  Serial.println("I2C slave ready at 0x08");
  Serial.println("Commands: PING=01 ECHO=02 WR_REG=03 RD_REG=04 STATUS=05");
}

void loop() {
  if (logPending) {
    logPending = false;
    Serial.print("pkt #");
    Serial.print(rxPktCnt);
    Serial.print("  cmd=0x");
    if (logCmd < 0x10) Serial.print('0');
    Serial.print(logCmd, HEX);
    Serial.print("  status=0x");
    Serial.print(logStatus, HEX);
    if (logStatus != ST_OK) Serial.print("  ERROR");
    Serial.println();
  }

  static unsigned long lastBlink;
  if (millis() - lastBlink >= 1000) {
    lastBlink = millis();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
}
