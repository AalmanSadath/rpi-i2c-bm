#define _POSIX_C_SOURCE 199309L

#include "bcm2835_i2c.h"
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#define REG_READ(base, offset)       (*(volatile uint32_t *)((uint8_t *)(base) + (offset)))
#define REG_WRITE(base, offset, val) (*(volatile uint32_t *)((uint8_t *)(base) + (offset)) = (val))

#define TIMEOUT_US  100000U

static void delay_us(uint32_t us) {
    struct timespec ts;
    ts.tv_sec  = us / 1000000;
    ts.tv_nsec = (long)(us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

static bool wait_for_status(bcm2835_i2c_t *i2c, uint32_t bit_mask, uint32_t timeout_us) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_us) {
        if (REG_READ(i2c->base, BSC_S) & bit_mask)
            return true;
        delay_us(10);
        elapsed += 10;
    }
    return false;
}

static void clear_fifo(bcm2835_i2c_t *i2c) {
    REG_WRITE(i2c->base, BSC_C, REG_READ(i2c->base, BSC_C) | BSC_C_CLEAR);
}

static void clear_status(bcm2835_i2c_t *i2c) {
    REG_WRITE(i2c->base, BSC_S, BSC_S_CLEAR);
}

i2c_result_t bcm2835_i2c_init(bcm2835_i2c_t *i2c, uint32_t clock_hz) {
    if (!i2c)
        return I2C_ERROR_INVALID_PARAM;

    i2c->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (i2c->mem_fd < 0) {
        perror("Failed to open /dev/mem (root required)");
        return I2C_ERROR_HW_INIT;
    }

    uint32_t i2c_addr = BCM2711_PERI_BASE + BCM2835_I2C1_BASE_OFFSET;
    void *map = mmap(NULL, BCM2835_PAGE_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, i2c->mem_fd, i2c_addr);
    if (map == MAP_FAILED) {
        perror("Failed to mmap I2C registers");
        close(i2c->mem_fd);
        return I2C_ERROR_HW_INIT;
    }
    i2c->base = (volatile uint32_t *)map;

    uint32_t gpio_addr = BCM2711_PERI_BASE + BCM2835_GPIO_BASE_OFFSET;
    void *gpio_map = mmap(NULL, BCM2835_PAGE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, i2c->mem_fd, gpio_addr);
    if (gpio_map != MAP_FAILED) {
        volatile uint32_t *gpio = (volatile uint32_t *)gpio_map;
        uint32_t fsel = gpio[0];
        fsel &= ~(0x7U << (2 * 3));
        fsel &= ~(0x7U << (3 * 3));
        fsel |=  (4U   << (2 * 3));
        fsel |=  (4U   << (3 * 3));
        gpio[0] = fsel;
        printf("[GPIO] GPIO 2/3 configured for I2C (ALT0)\n");
        munmap(gpio_map, BCM2835_PAGE_SIZE);
    } else {
        printf("[WARN] Could not configure GPIO, continuing anyway\n");
    }

    i2c->clock_div = BCM2835_CORE_CLK_HZ / clock_hz;
    REG_WRITE(i2c->base, BSC_DIV, i2c->clock_div);
    clear_fifo(i2c);
    clear_status(i2c);
    REG_WRITE(i2c->base, BSC_C, BSC_C_I2CEN | BSC_C_CLEAR);

    printf("[I2C] BSC1 at 0x%08X, divider=%u (~%u Hz)\n",
           i2c_addr, i2c->clock_div, BCM2835_CORE_CLK_HZ / i2c->clock_div);
    return I2C_SUCCESS;
}

void bcm2835_i2c_close(bcm2835_i2c_t *i2c) {
    if (!i2c) return;
    if (i2c->base) {
        REG_WRITE(i2c->base, BSC_C, 0);
        munmap((void *)i2c->base, BCM2835_PAGE_SIZE);
        i2c->base = NULL;
    }
    if (i2c->mem_fd >= 0) {
        close(i2c->mem_fd);
        i2c->mem_fd = -1;
    }
    printf("[I2C] BSC1 closed\n");
}

void bcm2835_i2c_set_slave_addr(bcm2835_i2c_t *i2c, uint8_t slave_addr) {
    if (!i2c || !i2c->base) return;
    REG_WRITE(i2c->base, BSC_A, slave_addr & 0x7FU);
}

i2c_result_t bcm2835_i2c_write(bcm2835_i2c_t *i2c, const uint8_t *data, uint32_t len) {
    if (!i2c || !i2c->base || !data || len == 0)
        return I2C_ERROR_INVALID_PARAM;

    clear_fifo(i2c);
    clear_status(i2c);
    REG_WRITE(i2c->base, BSC_DLEN, len);
    REG_WRITE(i2c->base, BSC_C, BSC_C_I2CEN | BSC_C_ST);

    uint32_t written = 0;
    while (written < len) {
        if (!wait_for_status(i2c, BSC_S_TXD, TIMEOUT_US)) {
            clear_status(i2c);
            return I2C_ERROR_TIMEOUT;
        }
        REG_WRITE(i2c->base, BSC_FIFO, data[written++]);
    }

    if (!wait_for_status(i2c, BSC_S_DONE, TIMEOUT_US)) {
        clear_status(i2c);
        return I2C_ERROR_TIMEOUT;
    }

    uint32_t s = REG_READ(i2c->base, BSC_S);
    clear_status(i2c);
    if (s & BSC_S_ERR)  return I2C_ERROR_NACK;
    if (s & BSC_S_CLKT) return I2C_ERROR_CLKT;
    return I2C_SUCCESS;
}

i2c_result_t bcm2835_i2c_read(bcm2835_i2c_t *i2c, uint8_t *data, uint32_t len) {
    if (!i2c || !i2c->base || !data || len == 0)
        return I2C_ERROR_INVALID_PARAM;

    clear_fifo(i2c);
    clear_status(i2c);
    REG_WRITE(i2c->base, BSC_DLEN, len);
    REG_WRITE(i2c->base, BSC_C, BSC_C_I2CEN | BSC_C_ST | BSC_C_READ);

    uint32_t nread = 0;
    while (nread < len) {
        if (!wait_for_status(i2c, BSC_S_RXD, TIMEOUT_US)) {
            clear_status(i2c);
            return I2C_ERROR_TIMEOUT;
        }
        data[nread++] = REG_READ(i2c->base, BSC_FIFO) & 0xFFU;
    }

    if (!wait_for_status(i2c, BSC_S_DONE, TIMEOUT_US)) {
        clear_status(i2c);
        return I2C_ERROR_TIMEOUT;
    }

    uint32_t s = REG_READ(i2c->base, BSC_S);
    clear_status(i2c);
    if (s & BSC_S_ERR)  return I2C_ERROR_NACK;
    if (s & BSC_S_CLKT) return I2C_ERROR_CLKT;
    return I2C_SUCCESS;
}

i2c_result_t bcm2835_i2c_write_read(bcm2835_i2c_t *i2c,
                                     const uint8_t *write_data, uint32_t write_len,
                                     uint8_t *read_data, uint32_t read_len) {
    if (!i2c || !i2c->base || !write_data || write_len == 0 || !read_data || read_len == 0)
        return I2C_ERROR_INVALID_PARAM;

    clear_fifo(i2c);
    clear_status(i2c);

    REG_WRITE(i2c->base, BSC_DLEN, write_len);
    REG_WRITE(i2c->base, BSC_C, BSC_C_I2CEN | BSC_C_ST);

    /* Wait for TA to go high (address sent). Linux scheduling can cause us to
     * miss the window on short transfers, so also break on DONE. */
    uint32_t elapsed = 0;
    while (elapsed < TIMEOUT_US) {
        uint32_t s = REG_READ(i2c->base, BSC_S);
        if (s & BSC_S_TA)   break;
        if (s & BSC_S_DONE) break;
        delay_us(10);
        elapsed += 10;
    }
    if (elapsed >= TIMEOUT_US) {
        clear_status(i2c);
        return I2C_ERROR_TIMEOUT;
    }

    if (REG_READ(i2c->base, BSC_S) & BSC_S_ERR) {
        clear_status(i2c);
        return I2C_ERROR_NACK;
    }

    uint32_t written = 0;
    while (written < write_len) {
        if (!wait_for_status(i2c, BSC_S_TXD, TIMEOUT_US)) {
            clear_status(i2c);
            return I2C_ERROR_TIMEOUT;
        }
        REG_WRITE(i2c->base, BSC_FIFO, write_data[written++]);
    }

    /* Write ST|READ while TA=1 to issue repeated START instead of STOP.
     * The controller re-sends the address with R/W=1 once the write drains. */
    REG_WRITE(i2c->base, BSC_DLEN, read_len);
    REG_WRITE(i2c->base, BSC_C, BSC_C_I2CEN | BSC_C_ST | BSC_C_READ);

    uint32_t nread = 0;
    elapsed = 0;
    while (elapsed < TIMEOUT_US) {
        uint32_t s = REG_READ(i2c->base, BSC_S);
        while ((s & BSC_S_RXD) && nread < read_len) {
            read_data[nread++] = REG_READ(i2c->base, BSC_FIFO) & 0xFFU;
            s = REG_READ(i2c->base, BSC_S);
        }
        if (s & BSC_S_DONE) break;
        delay_us(10);
        elapsed += 10;
    }

    if (elapsed >= TIMEOUT_US) {
        clear_status(i2c);
        return I2C_ERROR_TIMEOUT;
    }

    uint32_t s = REG_READ(i2c->base, BSC_S);
    while ((s & BSC_S_RXD) && nread < read_len) {
        read_data[nread++] = REG_READ(i2c->base, BSC_FIFO) & 0xFFU;
        s = REG_READ(i2c->base, BSC_S);
    }

    if (s & BSC_S_ERR)  { clear_status(i2c); return I2C_ERROR_NACK; }
    if (s & BSC_S_CLKT) { clear_status(i2c); return I2C_ERROR_CLKT; }

    clear_status(i2c);
    return I2C_SUCCESS;
}

const char *i2c_result_to_string(i2c_result_t result) {
    switch (result) {
        case I2C_SUCCESS:             return "Success";
        case I2C_ERROR_NACK:          return "NACK error (slave did not acknowledge)";
        case I2C_ERROR_CLKT:          return "Clock stretch timeout";
        case I2C_ERROR_TIMEOUT:       return "Software timeout";
        case I2C_ERROR_INVALID_PARAM: return "Invalid parameter";
        case I2C_ERROR_HW_INIT:       return "Hardware initialization failed";
        default:                      return "Unknown error";
    }
}
