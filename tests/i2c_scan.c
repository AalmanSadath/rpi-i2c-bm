#define _POSIX_C_SOURCE 199309L

#include "bcm2835_i2c.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define REG_READ(base, offset)       (*(volatile uint32_t *)((uint8_t *)(base) + (offset)))
#define REG_WRITE(base, offset, val) (*(volatile uint32_t *)((uint8_t *)(base) + (offset)) = (val))

int probe_address(bcm2835_i2c_t *i2c, uint8_t addr) {
    REG_WRITE(i2c->base, BSC_A, addr);
    REG_WRITE(i2c->base, BSC_S, BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);
    REG_WRITE(i2c->base, BSC_DLEN, 0);
    REG_WRITE(i2c->base, BSC_C, REG_READ(i2c->base, BSC_C) | BSC_C_CLEAR);
    REG_WRITE(i2c->base, BSC_C, BSC_C_I2CEN | BSC_C_ST);

    struct timespec ts = {.tv_sec = 0, .tv_nsec = 10000000};
    nanosleep(&ts, NULL);

    uint32_t status = REG_READ(i2c->base, BSC_S);
    REG_WRITE(i2c->base, BSC_S, BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE);

    if (!(status & BSC_S_DONE)) return -1;  // stuck
    return (status & BSC_S_ERR) ? 0 : 1;
}

int main(void) {
    printf("I2C Bus Scanner\n");

    bcm2835_i2c_t i2c = {0};
    i2c_result_t result = bcm2835_i2c_init(&i2c, I2C_CLOCK_STANDARD);
    if (result != I2C_SUCCESS) {
        printf("[FATAL] Cannot initialize I2C: %s\n", i2c_result_to_string(result));
        return EXIT_FAILURE;
    }

    printf("\nScanning I2C bus for devices...\n");
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\n");

    int found_count = 0;
    int stuck_count = 0;

    for (int row = 0; row < 8; row++) {
        printf("%02x: ", row * 16);
        for (int col = 0; col < 16; col++) {
            uint8_t addr = (row * 16) + col;
            if (addr < 0x03 || addr > 0x77) {
                printf("   ");
                continue;
            }
            int res = probe_address(&i2c, addr);
            if (res == 1) {
                printf("%02x ", addr);
                found_count++;
            } else if (res == -1) {
                printf("XX ");
                stuck_count++;
            } else {
                printf("-- ");
            }
        }
        printf("\n");
    }

    printf("\n");
    if (found_count > 0) {
        printf("Found %d device(s)\n", found_count);
        if (found_count == 1)
            printf("  If you see '08', your Arduino is responding!\n");
    } else if (stuck_count > 0) {
        printf("Bus appears stuck (%d addresses timed out)\n", stuck_count);
        printf("  Possible causes:\n");
        printf("    - Missing pull-up resistors\n");
        printf("    - SDA or SCL shorted to GND\n");
        printf("    - Slave holding clock low\n");
    } else {
        printf("No devices found on the bus\n");
        printf("  Possible causes:\n");
        printf("    - Arduino not powered\n");
        printf("    - Arduino not programmed with I2C slave code\n");
        printf("    - Wiring error (check SDA/SCL connections)\n");
        printf("    - Wrong Arduino I2C pins (Nano uses A4/A5, Leonardo uses Pin 2/3)\n");
    }

    printf("\n");
    bcm2835_i2c_close(&i2c);
    return (found_count > 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
