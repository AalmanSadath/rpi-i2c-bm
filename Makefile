ARCH := $(shell uname -m)

CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -O2 -I./include
LDFLAGS = -lm

SRC_DIR   = src
INC_DIR   = include
TEST_DIR  = tests
BUILD_DIR = build

BSC_PROTOCOL_SRC  = $(SRC_DIR)/bsc_protocol.c
I2C_HAL_SRC       = $(SRC_DIR)/i2c_hal.c
ADXL345_SRC       = $(SRC_DIR)/adxl345.c
TEST_BASIC_SRC    = $(TEST_DIR)/test_i2c_basic.c
TEST_PROTOCOL_SRC = $(TEST_DIR)/test_i2c_protocol.c
SCAN_SRC          = $(TEST_DIR)/i2c_scan.c
TEST_ADXL_SRC     = $(TEST_DIR)/test_adxl345.c

BSC_PROTOCOL_OBJ  = $(BUILD_DIR)/bsc_protocol.o
I2C_HAL_OBJ       = $(BUILD_DIR)/i2c_hal.o
DRIVER_OBJS       = $(BSC_PROTOCOL_OBJ) $(I2C_HAL_OBJ)
ADXL345_OBJ       = $(BUILD_DIR)/adxl345.o
TEST_BASIC_OBJ    = $(BUILD_DIR)/test_i2c_basic.o
TEST_PROTOCOL_OBJ = $(BUILD_DIR)/test_i2c_protocol.o
SCAN_OBJ          = $(BUILD_DIR)/i2c_scan.o
TEST_ADXL_OBJ     = $(BUILD_DIR)/test_adxl345.o

TEST_BASIC_BIN    = $(BUILD_DIR)/test_i2c_basic
TEST_PROTOCOL_BIN = $(BUILD_DIR)/test_i2c_protocol
SCAN_BIN          = $(BUILD_DIR)/i2c_scan
TEST_ADXL_BIN     = $(BUILD_DIR)/test_adxl345

ARM_CHECK = @if [ "$(ARCH)" != "aarch64" ] && [ "$(ARCH)" != "armv7l" ] && [ "$(ARCH)" != "armv6l" ]; then \
	echo "ERROR: must run on Raspberry Pi (ARM)"; exit 1; fi

all: directories $(TEST_BASIC_BIN) $(TEST_PROTOCOL_BIN) $(SCAN_BIN) $(TEST_ADXL_BIN)

directories:
	@mkdir -p $(BUILD_DIR)

$(BSC_PROTOCOL_OBJ): $(BSC_PROTOCOL_SRC) $(INC_DIR)/bsc_protocol.h $(INC_DIR)/bcm2835_bsc_regs.h $(INC_DIR)/i2c_result.h
	$(CC) $(CFLAGS) -c $< -o $@

$(I2C_HAL_OBJ): $(I2C_HAL_SRC) $(INC_DIR)/i2c_hal.h $(INC_DIR)/bsc_protocol.h $(INC_DIR)/i2c_result.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BASIC_OBJ): $(TEST_BASIC_SRC) $(INC_DIR)/bcm2835_i2c.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_PROTOCOL_OBJ): $(TEST_PROTOCOL_SRC) $(INC_DIR)/bcm2835_i2c.h
	$(CC) $(CFLAGS) -c $< -o $@

$(SCAN_OBJ): $(SCAN_SRC) $(INC_DIR)/bcm2835_i2c.h
	$(CC) $(CFLAGS) -c $< -o $@

$(ADXL345_OBJ): $(ADXL345_SRC) $(INC_DIR)/adxl345.h $(INC_DIR)/i2c_hal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_ADXL_OBJ): $(TEST_ADXL_SRC) $(INC_DIR)/adxl345.h $(INC_DIR)/i2c_hal.h
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_BASIC_BIN): $(TEST_BASIC_OBJ) $(DRIVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_PROTOCOL_BIN): $(TEST_PROTOCOL_OBJ) $(DRIVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(SCAN_BIN): $(SCAN_OBJ) $(DRIVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_ADXL_BIN): $(TEST_ADXL_OBJ) $(ADXL345_OBJ) $(DRIVER_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

.PHONY: test
test: $(TEST_BASIC_BIN)
	$(ARM_CHECK)
	sudo $(TEST_BASIC_BIN)

.PHONY: test-protocol
test-protocol: $(TEST_PROTOCOL_BIN)
	$(ARM_CHECK)
	sudo $(TEST_PROTOCOL_BIN)

.PHONY: scan
scan: $(SCAN_BIN)
	$(ARM_CHECK)
	sudo $(SCAN_BIN)

.PHONY: adxl345
adxl345: $(TEST_ADXL_BIN)
	$(ARM_CHECK)
	sudo $(TEST_ADXL_BIN)

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	@echo "Build artifacts cleaned"

.PHONY: help
help:
	@echo "Targets:"
	@echo "  all           build all (default)"
	@echo "  scan          probe I2C bus"
	@echo "  test          basic write/read/echo suite"
	@echo "  test-protocol CRC command/response suite"
	@echo "  adxl345       ADXL345 live accelerometer demo"
	@echo "  clean         remove build/"
	@echo ""
	@echo "All run targets require sudo and ARM hardware (/dev/mem)."
	@echo "Workflow: scan -> test"
