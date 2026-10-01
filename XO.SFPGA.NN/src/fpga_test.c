#include "fpga.h"
#include <string.h>
#include <stdio.h>

void lut_init(lut_t *lut);
uint8_t lut_eval(lut_t *lut, uint8_t inputs[]);

void reg_init(reg_t *reg);
void reg_cycle(reg_t *reg, uint8_t *d, uint8_t clk, uint8_t en);

void fpga_init(fpga_t *fpga, uint8_t rows, uint8_t cols);
void fpga_cycle(fpga_t *fpga);
void fpga_set_pin(fpga_t *fpga, uint8_t pin, uint8_t value);
uint8_t fpga_get_pin(fpga_t *fpga, uint8_t pin);

int main(int argc, char **argv) {
    fpga_t fpga;
    fpga_init(&fpga, 4, 4);

    lut_t *lut = &fpga.grid[0].luts[0];
    lut->n_inputs = 2;
    lut->inputs[0] = 0;
    lut->inputs[1] = 1;
    lut->truth_table[0] = 0;
    lut->truth_table[1] = 0;
    lut->truth_table[2] = 0;
    lut->truth_table[3] = 1;

    uint8_t inputs[BUS_WIDTH] = {0, 0, 0, 0, 0, 0};
    uint8_t result = lut_eval(lut, inputs);
    if (result != 0) {
        printf("FAIL: AND(0,0) = %d, expected 0\n", result);
        return 1;
    }

    inputs[0] = 1; inputs[1] = 1;
    result = lut_eval(lut, inputs);
    if (result != 1) {
        printf("FAIL: AND(1,1) = %d, expected 1\n", result);
        return 1;
    }

    printf("PASS: LUT AND gate test\n");
    printf("FPGA emulator stub initialized\n");
    return 0;
}
