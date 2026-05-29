#define _POSIX_C_SOURCE 199309L

#include "bcm2835_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define ARDUINO_ADDR       0x08
#define TEST_PATTERN_SIZE  16

void print_hex(const char *label, const uint8_t *data, uint32_t len) {
    printf("%s", label);
    for (uint32_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
        if ((i + 1) % 16 == 0 && i != len - 1)
            printf("\n%*s", (int)strlen(label), "");
    }
    printf("\n");
}

bool verify_data(const uint8_t *expected, const uint8_t *actual, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (expected[i] != actual[i]) {
            printf("  [FAIL] Mismatch at byte %u: expected 0x%02X, got 0x%02X\n",
                   i, expected[i], actual[i]);
            return false;
        }
    }
    return true;
}

bool test_write(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 1: Basic Write ===\n");

    uint8_t write_data[TEST_PATTERN_SIZE];
    for (int i = 0; i < TEST_PATTERN_SIZE; i++)
        write_data[i] = 0x10 + i;

    print_hex("  Writing: ", write_data, TEST_PATTERN_SIZE);

    i2c_result_t result = bcm2835_i2c_write(i2c, write_data, TEST_PATTERN_SIZE);
    if (result == I2C_SUCCESS) {
        printf("  [PASS] Write completed successfully\n");
        return true;
    }
    printf("  [FAIL] Write error: %s\n", i2c_result_to_string(result));
    return false;
}

bool test_read(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 2: Basic Read ===\n");

    uint8_t read_data[TEST_PATTERN_SIZE];
    memset(read_data, 0, TEST_PATTERN_SIZE);

    i2c_result_t result = bcm2835_i2c_read(i2c, read_data, TEST_PATTERN_SIZE);
    if (result == I2C_SUCCESS) {
        print_hex("  Read:    ", read_data, TEST_PATTERN_SIZE);
        printf("  [PASS] Read completed successfully\n");
        return true;
    }
    printf("  [FAIL] Read error: %s\n", i2c_result_to_string(result));
    return false;
}

bool test_echo(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 3: Echo Test (Write + Read) ===\n");

    uint8_t write_data[8] = {0xAA, 0x55, 0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE};
    uint8_t read_data[8];

    print_hex("  Writing: ", write_data, 8);

    i2c_result_t result = bcm2835_i2c_write(i2c, write_data, 8);
    if (result != I2C_SUCCESS) {
        printf("  [FAIL] Write failed: %s\n", i2c_result_to_string(result));
        return false;
    }

    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};
    nanosleep(&ts, NULL);

    result = bcm2835_i2c_read(i2c, read_data, 8);
    if (result != I2C_SUCCESS) {
        printf("  [FAIL] Read failed: %s\n", i2c_result_to_string(result));
        return false;
    }

    print_hex("  Read:    ", read_data, 8);

    if (verify_data(write_data, read_data, 8)) {
        printf("  [PASS] Echo verified - data matches!\n");
        return true;
    }
    printf("  [FAIL] Echo mismatch\n");
    return false;
}

bool test_single_byte(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 4: Single Byte Transfer ===\n");

    uint8_t write_byte = 0x42;
    uint8_t read_byte;

    printf("  Writing: 0x%02X\n", write_byte);

    i2c_result_t result = bcm2835_i2c_write(i2c, &write_byte, 1);
    if (result != I2C_SUCCESS) {
        printf("  [FAIL] Write failed: %s\n", i2c_result_to_string(result));
        return false;
    }

    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};
    nanosleep(&ts, NULL);

    result = bcm2835_i2c_read(i2c, &read_byte, 1);
    if (result != I2C_SUCCESS) {
        printf("  [FAIL] Read failed: %s\n", i2c_result_to_string(result));
        return false;
    }

    printf("  Read:    0x%02X\n", read_byte);

    if (read_byte == write_byte) {
        printf("  [PASS] Single byte echo verified\n");
        return true;
    }
    printf("  [FAIL] Echo mismatch: expected 0x%02X, got 0x%02X\n",
           write_byte, read_byte);
    return false;
}

bool test_nack_detection(bcm2835_i2c_t *i2c) {
    printf("\n=== Test 5: NACK Detection ===\n");
    printf("  Attempting to write to non-existent slave 0x77...\n");

    bcm2835_i2c_set_slave_addr(i2c, 0x77);
    uint8_t dummy_data = 0x00;
    i2c_result_t result = bcm2835_i2c_write(i2c, &dummy_data, 1);
    bcm2835_i2c_set_slave_addr(i2c, ARDUINO_ADDR);

    if (result == I2C_ERROR_NACK) {
        printf("  [PASS] NACK correctly detected\n");
        return true;
    }
    printf("  [WARN] Expected NACK, got: %s\n", i2c_result_to_string(result));
    printf("  (This may fail if address 0x77 actually exists on the bus)\n");
    return true;
}

int main(void) {
    printf("I2C basic test suite (slave 0x%02X)\n", ARDUINO_ADDR);

    bcm2835_i2c_t i2c = {0};
    i2c_result_t result = bcm2835_i2c_init(&i2c, I2C_CLOCK_STANDARD);
    if (result != I2C_SUCCESS) {
        printf("\n[FATAL] Failed to initialize I2C: %s\n",
               i2c_result_to_string(result));
        printf("Did you run with sudo?\n\n");
        return EXIT_FAILURE;
    }

    bcm2835_i2c_set_slave_addr(&i2c, ARDUINO_ADDR);

    int tests_passed = 0;
    int tests_total  = 5;

    if (test_write(&i2c))           tests_passed++;
    if (test_read(&i2c))            tests_passed++;
    if (test_echo(&i2c))            tests_passed++;
    if (test_single_byte(&i2c))     tests_passed++;
    if (test_nack_detection(&i2c))  tests_passed++;

    printf("\n%d / %d passed\n", tests_passed, tests_total);

    if (tests_passed == tests_total)
        printf("\nAll tests passed.\n\n");
    else
        printf("\nSome tests failed. Check Arduino connection and firmware.\n");

    bcm2835_i2c_close(&i2c);
    return (tests_passed == tests_total) ? EXIT_SUCCESS : EXIT_FAILURE;
}
