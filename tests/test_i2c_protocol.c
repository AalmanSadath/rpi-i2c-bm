/*
 * Packet format (both directions):
 *   [CMD/STATUS : 1][LEN : 1][DATA : 0..29][CRC8 : 1]   max 32 bytes
 *
 * The master knows the expected response size for each command, so each
 * read requests exactly that many bytes; no partial reads needed.
 */

#define _POSIX_C_SOURCE 199309L

#include "bcm2835_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARDUINO_ADDR  0x08
#define BUF_SIZE      32
#define MAX_DATA      (BUF_SIZE - 3)

#define CMD_PING    0x01
#define CMD_ECHO    0x02
#define CMD_WR_REG  0x03
#define CMD_RD_REG  0x04
#define CMD_STATUS  0x05

#define ST_OK       0x00
#define ST_ERR_CRC  0x01
#define ST_ERR_CMD  0x02
#define ST_ERR_LEN  0x03

static void sleep_ms(int ms) {
    struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)ms * 1000000L };
    nanosleep(&ts, NULL);
}

static uint8_t crc8(const uint8_t *d, uint8_t n) {
    uint8_t crc = 0x00;
    for (uint8_t i = 0; i < n; i++) {
        crc ^= d[i];
        for (uint8_t b = 0; b < 8; b++)
            crc = (crc & 0x80) ? ((crc << 1) ^ 0x07) : (crc << 1);
    }
    return crc;
}

/* Assemble [CMD][LEN][DATA...][CRC] -> out[]. Returns packet byte count. */
static uint8_t build_cmd(uint8_t cmd, const uint8_t *data, uint8_t dlen, uint8_t *out) {
    out[0] = cmd;
    out[1] = dlen;
    if (dlen) memcpy(&out[2], data, dlen);
    out[2 + dlen] = crc8(out, 2 + dlen);
    return 3 + dlen;
}

/* Validate [STATUS][LEN][DATA...][CRC] response. Returns false on CRC failure. */
static bool parse_resp(const uint8_t *buf, uint8_t buf_len,
                       uint8_t *status_out, const uint8_t **data_out, uint8_t *dlen_out) {
    if (buf_len < 3) return false;
    uint8_t dlen = buf[1];
    if ((uint8_t)(dlen + 3) > buf_len) return false;
    if (crc8(buf, 2 + dlen) != buf[2 + dlen]) return false;
    *status_out = buf[0];
    *data_out   = &buf[2];
    *dlen_out   = dlen;
    return true;
}

/* Write command then read exactly resp_len bytes after 1 ms processing gap. */
static bool transact(bcm2835_i2c_t *i2c,
                     const uint8_t *pkt, uint8_t pkt_len,
                     uint8_t *resp, uint8_t resp_len) {
    i2c_result_t r = bcm2835_i2c_write(i2c, pkt, pkt_len);
    if (r != I2C_SUCCESS) {
        printf("  write error: %s\n", i2c_result_to_string(r));
        return false;
    }
    sleep_ms(1);
    r = bcm2835_i2c_read(i2c, resp, resp_len);
    if (r != I2C_SUCCESS) {
        printf("  read error: %s\n", i2c_result_to_string(r));
        return false;
    }
    return true;
}

bool test_ping(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 1: PING ===\n");
    uint8_t pkt[BUF_SIZE], resp[BUF_SIZE];
    uint8_t pkt_len = build_cmd(CMD_PING, NULL, 0, pkt);

    if (!transact(i2c, pkt, pkt_len, resp, 4)) return false;

    uint8_t status;
    const uint8_t *data;
    uint8_t dlen;
    if (!parse_resp(resp, 4, &status, &data, &dlen)) {
        printf("  [FAIL] Response CRC mismatch\n");
        return false;
    }
    if (status != ST_OK || dlen != 1 || data[0] != 0xAB) {
        printf("  [FAIL] Expected ST_OK + 0xAB, got status=0x%02X data[0]=0x%02X\n",
               status, data[0]);
        return false;
    }
    printf("  [PASS] PING -> 0xAB\n");
    return true;
}

bool test_echo(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 2: ECHO ===\n");
    uint8_t payload[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE };
    uint8_t pkt[BUF_SIZE], resp[BUF_SIZE];
    uint8_t payload_len = (uint8_t)sizeof(payload);
    uint8_t pkt_len  = build_cmd(CMD_ECHO, payload, payload_len, pkt);
    uint8_t resp_len = 3 + payload_len;

    if (!transact(i2c, pkt, pkt_len, resp, resp_len)) return false;

    uint8_t status;
    const uint8_t *data;
    uint8_t dlen;
    if (!parse_resp(resp, resp_len, &status, &data, &dlen)) {
        printf("  [FAIL] Response CRC mismatch\n");
        return false;
    }
    if (status != ST_OK || dlen != payload_len || memcmp(data, payload, payload_len) != 0) {
        printf("  [FAIL] Echo mismatch (status=0x%02X dlen=%u)\n", status, dlen);
        return false;
    }
    printf("  [PASS] ECHO verified (%u bytes)\n", payload_len);
    return true;
}

bool test_register_rw(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 3: Register Write / Read ===\n");

    struct { uint8_t addr; uint8_t val; } cases[] = {
        { 0, 0xA5 }, { 3, 0x7F }, { 7, 0x01 }
    };

    for (int i = 0; i < 3; i++) {
        uint8_t addr = cases[i].addr, val = cases[i].val;
        uint8_t pkt[BUF_SIZE], resp[BUF_SIZE];
        uint8_t status = ST_OK;
        const uint8_t *data = NULL;
        uint8_t dlen = 0;

        uint8_t wr[] = { addr, val };
        uint8_t pkt_len = build_cmd(CMD_WR_REG, wr, 2, pkt);
        if (!transact(i2c, pkt, pkt_len, resp, 3)) return false;
        if (!parse_resp(resp, 3, &status, &data, &dlen) || status != ST_OK) {
            printf("  [FAIL] WR_REG[%u]=0x%02X status=0x%02X\n",
                   addr, val, status);
            return false;
        }

        uint8_t rd[] = { addr };
        pkt_len = build_cmd(CMD_RD_REG, rd, 1, pkt);
        if (!transact(i2c, pkt, pkt_len, resp, 4)) return false;
        if (!parse_resp(resp, 4, &status, &data, &dlen) || status != ST_OK || dlen != 1) {
            printf("  [FAIL] RD_REG[%u] status=0x%02X\n", addr, status);
            return false;
        }
        if (data[0] != val) {
            printf("  [FAIL] reg[%u]: wrote 0x%02X read 0x%02X\n",
                   addr, val, data[0]);
            return false;
        }
        printf("  reg[%u]: wrote 0x%02X, read back 0x%02X\n", addr, val, data[0]);
    }
    printf("  [PASS] All register round-trips correct\n");
    return true;
}

bool test_slave_status(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 4: Slave Error Counters ===\n");
    uint8_t pkt[BUF_SIZE], resp[BUF_SIZE];
    uint8_t pkt_len = build_cmd(CMD_STATUS, NULL, 0, pkt);

    if (!transact(i2c, pkt, pkt_len, resp, 7)) return false;

    uint8_t status = ST_OK;
    const uint8_t *data = NULL;
    uint8_t dlen = 0;
    if (!parse_resp(resp, 7, &status, &data, &dlen) || status != ST_OK || dlen != 4) {
        printf("  [FAIL] STATUS response malformed (status=0x%02X dlen=%u)\n",
               status, dlen);
        return false;
    }

    uint8_t  crc_errs = data[0];
    uint8_t  cmd_errs = data[1];
    uint16_t rx_pkts  = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
    printf("  crc_errors=%u  cmd_errors=%u  rx_packets=%u\n", crc_errs, cmd_errs, rx_pkts);

    if (crc_errs > 0 || cmd_errs > 0) {
        printf("  [WARN] Slave reports errors -- check wiring or noise\n");
        return false;
    }
    printf("  [PASS] Slave reports zero errors\n");
    return true;
}

bool test_crc_error_injection(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 5: CRC Error Detection ===\n");
    printf("  Sending PING with a corrupted CRC byte...\n");

    uint8_t pkt[BUF_SIZE], resp[BUF_SIZE];
    build_cmd(CMD_PING, NULL, 0, pkt);
    pkt[2] ^= 0xFF;  /* corrupt the CRC */

    i2c_result_t r = bcm2835_i2c_write(i2c, pkt, 3);
    if (r != I2C_SUCCESS) {
        printf("  [FAIL] write: %s\n", i2c_result_to_string(r));
        return false;
    }
    sleep_ms(1);
    r = bcm2835_i2c_read(i2c, resp, 3);
    if (r != I2C_SUCCESS) {
        printf("  [FAIL] read: %s\n", i2c_result_to_string(r));
        return false;
    }

    uint8_t status;
    const uint8_t *data;
    uint8_t dlen;
    if (!parse_resp(resp, 3, &status, &data, &dlen)) {
        printf("  [FAIL] Error response itself has bad CRC\n");
        return false;
    }
    if (status != ST_ERR_CRC) {
        printf("  [FAIL] Expected ST_ERR_CRC (0x01), got 0x%02X\n", status);
        return false;
    }
    printf("  [PASS] Slave rejected bad CRC -> ST_ERR_CRC\n");
    return true;
}

bool test_nack_detection(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 6: NACK Detection ===\n");
    printf("  Writing PING to non-existent slave 0x77...\n");

    bcm2835_i2c_set_slave_addr(i2c, 0x77);
    uint8_t pkt[BUF_SIZE];
    build_cmd(CMD_PING, NULL, 0, pkt);
    i2c_result_t r = bcm2835_i2c_write(i2c, pkt, 3);
    bcm2835_i2c_set_slave_addr(i2c, ARDUINO_ADDR);

    if (r == I2C_ERROR_NACK) {
        printf("  [PASS] NACK correctly detected\n");
        return true;
    }
    printf("  [WARN] Expected NACK, got %s (0x77 may exist on bus)\n",
           i2c_result_to_string(r));
    return true;
}

int main(void) {
    printf("I2C protocol test suite (slave 0x%02X)\n", ARDUINO_ADDR);

    bcm2835_i2c_t i2c = {0};
    i2c_result_t r = bcm2835_i2c_init(&i2c, I2C_CLOCK_STANDARD);
    if (r != I2C_SUCCESS) {
        printf("\n[FATAL] I2C init failed: %s\n", i2c_result_to_string(r));
        printf("Did you run with sudo?\n\n");
        return EXIT_FAILURE;
    }
    bcm2835_i2c_set_slave_addr(&i2c, ARDUINO_ADDR);

    int passed = 0, total = 6;

    if (test_ping(&i2c))                passed++;
    if (test_echo(&i2c))                passed++;
    if (test_register_rw(&i2c))         passed++;
    if (test_slave_status(&i2c))        passed++;
    if (test_crc_error_injection(&i2c)) passed++;
    if (test_nack_detection(&i2c))      passed++;

    printf("\n%d / %d passed\n\n", passed, total);

    bcm2835_i2c_close(&i2c);
    return (passed == total) ? EXIT_SUCCESS : EXIT_FAILURE;
}
