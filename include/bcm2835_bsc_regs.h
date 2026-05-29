#ifndef BCM2835_BSC_REGS_H
#define BCM2835_BSC_REGS_H

/* BCM2711 (Raspberry Pi 4) peripheral base */
#define BCM2711_PERI_BASE         0xFE000000U

/* BSC1 (I2C1) register block offset — GPIO 2/3 */
#define BCM2835_I2C1_BASE_OFFSET  0x804000U

/* GPIO register block offset */
#define BCM2835_GPIO_BASE_OFFSET  0x200000U

/* mmap granularity */
#define BCM2835_PAGE_SIZE         4096U

/* BSC register offsets (byte offsets from BSC base) */
#define BSC_C     0x00U
#define BSC_S     0x04U
#define BSC_DLEN  0x08U
#define BSC_A     0x0CU
#define BSC_FIFO  0x10U
#define BSC_DIV   0x14U
#define BSC_DEL   0x18U
#define BSC_CLKT  0x1CU

/* BSC_C bit fields */
#define BSC_C_I2CEN  (1U << 15)
#define BSC_C_INTR   (1U << 10)
#define BSC_C_INTT   (1U <<  9)
#define BSC_C_INTD   (1U <<  8)
#define BSC_C_ST     (1U <<  7)
#define BSC_C_CLEAR  (1U <<  4)
#define BSC_C_READ   (1U <<  0)

/* BSC_S bit fields */
#define BSC_S_CLKT  (1U <<  9)
#define BSC_S_ERR   (1U <<  8)
#define BSC_S_RXF   (1U <<  7)
#define BSC_S_TXE   (1U <<  6)
#define BSC_S_RXD   (1U <<  5)
#define BSC_S_TXD   (1U <<  4)
#define BSC_S_RXR   (1U <<  3)
#define BSC_S_TXW   (1U <<  2)
#define BSC_S_DONE  (1U <<  1)
#define BSC_S_TA    (1U <<  0)

/* Write 1 to clear error/done flags */
#define BSC_S_CLEAR  (BSC_S_CLKT | BSC_S_ERR | BSC_S_DONE)

/* I2C core clock (BCM2835/2711 default: 150 MHz) */
#define BCM2835_CORE_CLK_HZ   150000000U

/* Standard I2C bus speeds (Hz) */
#define I2C_CLOCK_STANDARD    100000U
#define I2C_CLOCK_FAST        400000U
#define I2C_CLOCK_FAST_PLUS   1000000U

/* Precomputed clock dividers */
#define I2C_DIV_100K  (BCM2835_CORE_CLK_HZ / I2C_CLOCK_STANDARD)
#define I2C_DIV_400K  (BCM2835_CORE_CLK_HZ / I2C_CLOCK_FAST)

/* BSC FIFO depth (bytes) */
#define BSC_FIFO_SIZE  16U

#endif /* BCM2835_BSC_REGS_H */
