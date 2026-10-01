#include "fpga.h"
#include "bitstream.h"
#include <string.h>
#include <stdlib.h>

void lut_init(lut_t *lut) {
    memset(lut->truth_table, 0, LUT_SIZE_4);
    lut->n_inputs = 0;
    lut->output = 0;
}

uint8_t lut_eval(lut_t *lut, uint8_t input_values[]) {
    uint8_t index = 0;
    for (uint8_t i = 0; i < lut->n_inputs; i++) {
        if (lut->inputs[i] < BUS_WIDTH) {
            index |= (input_values[lut->inputs[i]] << i);
        }
    }
    lut->output = lut->truth_table[index];
    return lut->output;
}

void reg_init(reg_t *reg) {
    memset(reg->q, 0, 4);
    reg->clk_en = 0;
}

void reg_cycle(reg_t *reg, uint8_t d[], uint8_t clk, uint8_t clk_en) {
    if (clk && clk_en) {
        memcpy(reg->q, d, 4);
    }
}

void fpga_init(fpga_t *fpga, uint8_t rows, uint8_t cols) {
    memset(fpga, 0, sizeof(fpga_t));
    fpga->n_rows = rows;
    fpga->n_cols = cols;
}

void fpga_cycle(fpga_t *fpga) {
    for (uint8_t r = 0; r < fpga->n_rows; r++) {
        for (uint8_t c = 0; c < fpga->n_cols; c++) {
            clb_t *clb = &fpga->grid[r * fpga->n_cols + c];
            uint8_t inputs[BUS_WIDTH] = {0};
            memcpy(inputs, clb->north.values, sizeof(clb->north.values));

            for (uint8_t l = 0; l < MAX_LUTS_PER_CLB; l++) {
                lut_eval(&clb->luts[l], inputs);
            }
            for (uint8_t rg = 0; rg < MAX_REGS_PER_CLB; rg++) {
                reg_cycle(&clb->regs[rg], inputs, 1, clb->regs[rg].clk_en);
            }
        }
    }
}

void fpga_set_pin(fpga_t *fpga, uint8_t pin, uint8_t value) {
    if (pin < BUS_WIDTH) {
        fpga->io_pins[pin].values[0] = value;
    }
}

uint8_t fpga_get_pin(fpga_t *fpga, uint8_t pin) {
    if (pin < BUS_WIDTH) {
        return fpga->io_pins[pin].values[0];
    }
    return 0;
}
