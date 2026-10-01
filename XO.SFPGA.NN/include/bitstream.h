#ifndef BITSTREAM_H
#define BITSTREAM_H

#include <stdint.h>
#include "fpga.h"

#define BITSTREAM_MAGIC 0x584F5346
#define BITSTREAM_VERSION 1

typedef enum {
    OP_LUT3,
    OP_LUT4,
    OP_REG,
    OP_CONNECT,
    OP_PIN,
} bitstream_op_t;

typedef struct {
    bitstream_op_t op;
    uint8_t lut_id;
    uint8_t inputs[BUS_WIDTH];
    uint8_t n_inputs;
    uint8_t truth_table[16];
    uint8_t reg_id;
    uint8_t reg_input_lut;
    uint8_t clk_en;
    uint8_t pin_id;
    uint8_t pin_dir;
    uint8_t src_clb, src_lut;
    uint8_t dst_clb, dst_lut;
} bitstream_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t num_entries;
    uint8_t  rows;
    uint8_t  cols;
    bitstream_entry_t entries[256];
} bitstream_t;

int bitstream_load(bitstream_t *bs, const char *filename);
int bitstream_apply(fpga_t *fpga, const bitstream_t *bs);

#endif
