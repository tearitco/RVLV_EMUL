#include "fpga.h"
#include "bitstream.h"
#include <stdio.h>
#include <string.h>

extern void lut_init(lut_t *lut);
extern uint8_t lut_eval(lut_t *lut, uint8_t inputs[]);
extern void reg_init(reg_t *reg);
extern void reg_cycle(reg_t *reg, uint8_t *d, uint8_t clk, uint8_t en);

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
        printf("FAIL: AND(0,0) = %d\n", result);
        return 1;
    }

    inputs[0] = 1; inputs[1] = 0;
    result = lut_eval(lut, inputs);
    if (result != 0) {
        printf("FAIL: AND(1,0) = %d\n", result);
        return 1;
    }

    inputs[0] = 1; inputs[1] = 1;
    result = lut_eval(lut, inputs);
    if (result != 1) {
        printf("FAIL: AND(1,1) = %d\n", result);
        return 1;
    }

    printf("PASS: LUT AND gate test\n");

    reg_t *reg = &fpga.grid[0].regs[0];
    reg_init(reg);
    uint8_t d[4] = {1, 0, 1, 0};
    reg_cycle(reg, d, 1, 1);
    if (reg->q[0] != 1 || reg->q[2] != 1) {
        printf("FAIL: Register cycle test\n");
        return 1;
    }
    printf("PASS: Register test\n");

    printf("All FPGA emulator tests passed\n");
    return 0;
}
