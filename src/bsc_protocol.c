#define _POSIX_C_SOURCE 199309L

#include "bsc_protocol.h"
#include "bcm2835_bsc_regs.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#define REG_READ(base, off)        (*(volatile uint32_t *)((uint8_t *)(base) + (off)))
#define REG_WRITE(base, off, val)  (*(volatile uint32_t *)((uint8_t *)(base) + (off)) = (val))

#define TIMEOUT_US  100000U

struct bsc_periph {
    volatile uint32_t *base;
    int                mem_fd;
    uint32_t           clock_div;
};

static void delay_us(uint32_t us) {
    struct timespec ts;
    ts.tv_sec  = us / 1000000;
    ts.tv_nsec = (long)(us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

static bool wait_for_status(bsc_periph_t *p, uint32_t mask, uint32_t timeout_us) {
    uint32_t elapsed = 0;
    while (elapsed < timeout_us) {
        if (REG_READ(p->base, BSC_S) & mask)
            return true;
        delay_us(10);
        elapsed += 10;
    }
    return false;
}

static void clear_fifo(bsc_periph_t *p) {
    REG_WRITE(p->base, BSC_C, REG_READ(p->base, BSC_C) | BSC_C_CLEAR);
}

static void clear_status(bsc_periph_t *p) {
    REG_WRITE(p->base, BSC_S, BSC_S_CLEAR);
}

bsc_periph_t *bsc_open(uint32_t clock_hz) {
    bsc_periph_t *p = malloc(sizeof(*p));
    if (!p) return NULL;

    p->mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (p->mem_fd < 0) {
        perror("Failed to open /dev/mem (root required)");
        free(p);
        return NULL;
    }

    uint32_t i2c_addr = BCM2711_PERI_BASE + BCM2835_I2C1_BASE_OFFSET;
    void *map = mmap(NULL, BCM2835_PAGE_SIZE, PROT_READ | PROT_WRITE,
                     MAP_SHARED, p->mem_fd, i2c_addr);
    if (map == MAP_FAILED) {
        perror("Failed to mmap I2C registers");
        close(p->mem_fd);
        free(p);
        return NULL;
    }
    p->base = (volatile uint32_t *)map;

    uint32_t gpio_addr = BCM2711_PERI_BASE + BCM2835_GPIO_BASE_OFFSET;
    void *gpio_map = mmap(NULL, BCM2835_PAGE_SIZE, PROT_READ | PROT_WRITE,
                          MAP_SHARED, p->mem_fd, gpio_addr);
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

    p->clock_div = BCM2835_CORE_CLK_HZ / clock_hz;
    REG_WRITE(p->base, BSC_DIV, p->clock_div);
    clear_fifo(p);
    clear_status(p);
    REG_WRITE(p->base, BSC_C, BSC_C_I2CEN | BSC_C_CLEAR);

    printf("[I2C] BSC1 at 0x%08X, divider=%u (~%u Hz)\n",
           i2c_addr, p->clock_div, BCM2835_CORE_CLK_HZ / p->clock_div);
    return p;
}

void bsc_close(bsc_periph_t *p) {
    if (!p) return;
    if (p->base) {
        REG_WRITE(p->base, BSC_C, 0);
        munmap((void *)p->base, BCM2835_PAGE_SIZE);
        p->base = NULL;
    }
    if (p->mem_fd >= 0) {
        close(p->mem_fd);
        p->mem_fd = -1;
    }
    free(p);
    printf("[I2C] BSC1 closed\n");
}

void bsc_set_addr(bsc_periph_t *p, uint8_t addr) {
    if (!p || !p->base) return;
    REG_WRITE(p->base, BSC_A, addr & 0x7FU);
}

volatile uint32_t *bsc_get_base(bsc_periph_t *p) {
    return p ? p->base : NULL;
}

i2c_result_t bsc_write(bsc_periph_t *p, const uint8_t *data, uint32_t len) {
    if (!p || !p->base || !data || len == 0)
        return I2C_ERROR_INVALID_PARAM;

    clear_fifo(p);
    clear_status(p);
    REG_WRITE(p->base, BSC_DLEN, len);
    REG_WRITE(p->base, BSC_C, BSC_C_I2CEN | BSC_C_ST);

    uint32_t written = 0;
    while (written < len) {
        if (!wait_for_status(p, BSC_S_TXD, TIMEOUT_US)) {
            clear_status(p);
            return I2C_ERROR_TIMEOUT;
        }
        REG_WRITE(p->base, BSC_FIFO, data[written++]);
    }

    if (!wait_for_status(p, BSC_S_DONE, TIMEOUT_US)) {
        clear_status(p);
        return I2C_ERROR_TIMEOUT;
    }

    uint32_t s = REG_READ(p->base, BSC_S);
    clear_status(p);
    if (s & BSC_S_ERR)  return I2C_ERROR_NACK;
    if (s & BSC_S_CLKT) return I2C_ERROR_CLKT;
    return I2C_SUCCESS;
}

i2c_result_t bsc_read(bsc_periph_t *p, uint8_t *data, uint32_t len) {
    if (!p || !p->base || !data || len == 0)
        return I2C_ERROR_INVALID_PARAM;

    clear_fifo(p);
    clear_status(p);
    REG_WRITE(p->base, BSC_DLEN, len);
    REG_WRITE(p->base, BSC_C, BSC_C_I2CEN | BSC_C_ST | BSC_C_READ);

    uint32_t nread = 0;
    while (nread < len) {
        if (!wait_for_status(p, BSC_S_RXD, TIMEOUT_US)) {
            clear_status(p);
            return I2C_ERROR_TIMEOUT;
        }
        data[nread++] = REG_READ(p->base, BSC_FIFO) & 0xFFU;
    }

    if (!wait_for_status(p, BSC_S_DONE, TIMEOUT_US)) {
        clear_status(p);
        return I2C_ERROR_TIMEOUT;
    }

    uint32_t s = REG_READ(p->base, BSC_S);
    clear_status(p);
    if (s & BSC_S_ERR)  return I2C_ERROR_NACK;
    if (s & BSC_S_CLKT) return I2C_ERROR_CLKT;
    return I2C_SUCCESS;
}

i2c_result_t bsc_write_read(bsc_periph_t *p,
                              const uint8_t *wdata, uint32_t wlen,
                              uint8_t *rdata, uint32_t rlen) {
    if (!p || !p->base || !wdata || wlen == 0 || !rdata || rlen == 0)
        return I2C_ERROR_INVALID_PARAM;

    /*
     * The BCM2835/BCM2711 BSC controller does not support hardware repeated
     * START in bare-metal mode: it always asserts STOP at the end of each
     * transfer.  True repeated START requires the kernel I2C_RDWR ioctl.
     * We implement write-then-read as two separate transactions with a short
     * inter-transaction gap.
     */
    i2c_result_t r = bsc_write(p, wdata, wlen);
    if (r != I2C_SUCCESS) return r;
    delay_us(100);
    return bsc_read(p, rdata, rlen);
}
