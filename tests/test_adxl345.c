#define _POSIX_C_SOURCE 199309L

#include "adxl345.h"
#include "i2c_hal.h"
#include "bcm2835_bsc_regs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>

#define SAMPLE_INTERVAL_MS  100
#define BAR_WIDTH           20

static volatile int running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    running = 0;
}

static void sleep_ms(int ms) {
    struct timespec ts = { .tv_sec = 0, .tv_nsec = (long)ms * 1000000L };
    nanosleep(&ts, NULL);
}

static void print_bar(float val, float max) {
    int filled = (int)((val / max) * BAR_WIDTH);
    if (filled < 0)         filled = 0;
    if (filled > BAR_WIDTH) filled = BAR_WIDTH;
    putchar('[');
    for (int i = 0; i < BAR_WIDTH; i++)
        putchar(i < filled ? '#' : ' ');
    putchar(']');
}

static const char *tilt_axis(adxl345_accel_t *a) {
    float ax = a->x < 0 ? -a->x : a->x;
    float ay = a->y < 0 ? -a->y : a->y;
    float az = a->z < 0 ? -a->z : a->z;
    if (az >= ax && az >= ay) return (a->z > 0) ? "Z up  (flat)" : "Z down (upside down)";
    if (ax >= ay)             return (a->x > 0) ? "X up " : "X down";
    return                           (a->y > 0) ? "Y up " : "Y down";
}

int main(void) {
    printf("\n");
    printf("ADXL345 Accelerometer Demo\n");

    signal(SIGINT, handle_sigint);

    i2c_dev_t *i2c = i2c_open(I2C_CLOCK_STANDARD);
    if (!i2c) {
        fprintf(stderr, "[FATAL] I2C init failed. Run with sudo.\n");
        return EXIT_FAILURE;
    }

    adxl345_t sensor;
    adxl345_init(&sensor, i2c, ADXL345_ADDR_LO);

    printf("\n[1/3] Verifying ADXL345 device ID... ");
    fflush(stdout);
    i2c_result_t r = adxl345_verify_id(&sensor);
    if (r != I2C_SUCCESS) {
        printf("FAIL (%s)\n", i2c_result_str(r));
        printf("Check: SDO pin LOW -> address 0x53, SDO HIGH -> address 0x1D\n");
        i2c_close(i2c);
        return EXIT_FAILURE;
    }
    printf("OK\n");

    printf("[2/3] Starting measurement... ");
    fflush(stdout);
    r = adxl345_start(&sensor, ADXL345_RANGE_2G);
    if (r != I2C_SUCCESS) {
        printf("FAIL (%s)\n", i2c_result_str(r));
        i2c_close(i2c);
        return EXIT_FAILURE;
    }
    printf("OK\n");

    printf("[3/3] Reading acceleration -- Ctrl+C to stop\n\n\n\n\n");
    sleep_ms(10);  /* let device settle into measurement mode */

    while (running) {
        adxl345_accel_t a;
        r = adxl345_read_accel(&sensor, &a);
        if (r != I2C_SUCCESS) {
            printf("Read error: %s\n", i2c_result_str(r));
            break;
        }

        /* Overwrite same lines each iteration */
        printf("\033[4A");  /* move cursor up 4 lines */

        printf("  X: %+6.3f g  ", a.x);
        print_bar(a.x < 0 ? -a.x : a.x, 2.0f);
        printf("\n");

        printf("  Y: %+6.3f g  ", a.y);
        print_bar(a.y < 0 ? -a.y : a.y, 2.0f);
        printf("\n");

        printf("  Z: %+6.3f g  ", a.z);
        print_bar(a.z < 0 ? -a.z : a.z, 2.0f);
        printf("\n");

        printf("  Tilt: %-24s\n", tilt_axis(&a));

        fflush(stdout);
        sleep_ms(SAMPLE_INTERVAL_MS);
    }

    printf("\n\nStopping...\n");
    adxl345_stop(&sensor);
    i2c_close(i2c);
    return EXIT_SUCCESS;
}
