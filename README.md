# i2c_bare_metal

Bare-metal I2C master driver for Raspberry Pi 4 (BCM2711). Accesses the BSC1 peripheral directly via `/dev/mem` — no Linux kernel I2C subsystem involved. Implements a three-layer HAL, a CRC-8 command/response protocol with an Arduino Leonardo slave, and an ADXL345 accelerometer driver.

The kernel I2C driver must be disabled to avoid register conflicts:
```
sudo raspi-config   # Interface Options → I2C → Disable
```
Or if already loaded: `sudo rmmod i2c_dev i2c_brcmstb`

## Hardware

See [WIRING.md](WIRING.md) for full wiring diagram.

Raspberry Pi 4 + Arduino Leonardo + ADXL345 accelerometer on a shared I2C bus.

**Wiring**

| Signal | RPi Pin | RPi GPIO | Arduino Leonardo | ADXL345 |
|--------|---------|----------|------------------|---------|
| SDA    | Pin 3   | GPIO 2   | Pin 2 (SDA)      | SDA     |
| SCL    | Pin 5   | GPIO 3   | Pin 3 (SCL)      | SCL     |
| 3.3 V  | Pin 1   | —        | —                | VCC, CS |
| GND    | Pin 6   | —        | GND              | GND     |

If `scan` shows `XX` on every address: pull-ups missing or a pin shorted to GND.

## Building and running

```
make
```

Binaries go into `build/`. All run targets require `sudo` for `/dev/mem` access. Build succeeds on x86 but binaries must run on the Pi.

```
sudo make scan           # probe bus — expect 08 (Arduino) and 53 (ADXL345)
sudo make test           # basic write/read/echo suite (Arduino)
sudo make test-protocol  # CRC-8 command/response protocol (Arduino)
sudo make adxl345        # ADXL345 live accelerometer demo
```

Recommended order: `scan` first to confirm devices, then test suites.

## Architecture

Three-layer HAL - hardware details are contained in the lowest layer and do not leak into application code.

```
include/i2c_result.h         Shared result codes (i2c_result_t enum)
include/bcm2835_bsc_regs.h   Layer 1 — hardware constants, register offsets, bit masks
include/bsc_protocol.h       Layer 2 — BSC peripheral interface (opaque bsc_periph_t*)
src/bsc_protocol.c           Layer 2 — mmap lifecycle, GPIO config, FIFO polling
include/i2c_hal.h            Layer 3 — public HAL API (opaque i2c_dev_t*)
src/i2c_hal.c                Layer 3 — thin wrappers over bsc_protocol
include/bcm2835_i2c.h        Backward-compat shim — maps old names to HAL
src/bcm2835_i2c.c            Original monolithic driver (reference only)
include/adxl345.h            ADXL345 driver API
src/adxl345.c                ADXL345 driver — sits entirely on top of i2c_hal.h
```

New code should use `i2c_hal.h`. The shim (`bcm2835_i2c.h`) exists only to keep legacy test files compiling without modification.

## Key constants

| Symbol | Value | Meaning |
|--------|-------|---------|
| `BCM2711_PERI_BASE` | `0xFE000000` | RPi 4 peripheral base address |
| `BCM2835_I2C1_BASE_OFFSET` | `0x804000` | BSC1 offset (GPIO 2/3) |
| `BCM2835_CORE_CLK_HZ` | `150000000` | BSC core clock (150 MHz) |
| `I2C_CLOCK_STANDARD` | `100000` | 100 kHz — clock divider 1500 |
| `TIMEOUT_US` | `100000` | Per-transfer polling timeout (100 ms) |
| `ARDUINO_ADDR` | `0x08` | Arduino Leonardo slave address |
| `ADXL345_ADDR_LO` | `0x53` | ADXL345 address when SDO LOW |
| `ADXL345_SCALE_G` | `0.0039` | mg/LSB in full-resolution mode |

## Notes

**No hardware repeated START.** The BCM2711 BSC controller always asserts STOP at the end of each transfer in bare-metal mode. `bsc_write_read` implements write-then-read as two separate transactions with a 100 µs gap. Works for register-based sensors (ADXL345, etc.) but is not strictly I2C compliant. True repeated START requires the kernel `I2C_RDWR` ioctl.

**ADXL345 always uses full-resolution mode.** 3.9 mg/LSB at all g-ranges. `adxl345_verify_id` reads the DEVID register (0x00, always 0xE5) and fails fast if wiring is wrong.

**Arduino Leonardo ISR safety.** `Serial.print()` inside Wire ISR callbacks blocks USB-CDC on the Leonardo, causing clock-stretch timeout (`BSC_S_CLKT`) on the RPi. The slave firmware defers all Serial output to `loop()` via a volatile flag.

**Polling mode only.** All transfers busy-wait on BSC status bits. Interrupt-driven mode is not implemented.
