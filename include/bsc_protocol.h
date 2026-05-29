#ifndef BSC_PROTOCOL_H
#define BSC_PROTOCOL_H

#include <stdint.h>
#include "i2c_result.h"

typedef struct bsc_periph bsc_periph_t;

bsc_periph_t       *bsc_open(uint32_t clock_hz);
void                bsc_close(bsc_periph_t *p);
void                bsc_set_addr(bsc_periph_t *p, uint8_t addr);
volatile uint32_t  *bsc_get_base(bsc_periph_t *p);

i2c_result_t  bsc_write(bsc_periph_t *p, const uint8_t *data, uint32_t len);
i2c_result_t  bsc_read(bsc_periph_t *p, uint8_t *data, uint32_t len);
i2c_result_t  bsc_write_read(bsc_periph_t *p,
                              const uint8_t *wdata, uint32_t wlen,
                              uint8_t *rdata, uint32_t rlen);

#endif /* BSC_PROTOCOL_H */
