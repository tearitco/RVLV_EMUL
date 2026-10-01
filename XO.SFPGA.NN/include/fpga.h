#ifndef FPGA_H
#define FPGA_H

#include <stdint.h>
#include <stddef.h>

#define FPGA_NAME "xo-sfpga"
#define FPGA_VERSION "0.1.0"

#define LUT_SIZE_3    8
#define LUT_SIZE_4    16
#define BUS_WIDTH     6
#define BUS_DATA_BITS 4
#define BUS_CTRL_BITS 2

#define MAX_CLBS      256
#define MAX_LUTS_PER_CLB 4
#define MAX_REGS_PER_CLB 4

typedef struct {
    uint8_t truth_table[LUT_SIZE_4];
    uint8_t n_inputs;
    uint8_t inputs[BUS_WIDTH];
    uint8_t output;
} lut_t;

typedef struct {
    uint8_t d[4];
    uint8_t q[4];
    uint8_t clk_en;
    uint8_t clk;
} reg_t;

typedef struct {
    uint8_t values[BUS_WIDTH];
} bus_t;

typedef struct {
    lut_t  luts[MAX_LUTS_PER_CLB];
    reg_t  regs[MAX_REGS_PER_CLB];
    bus_t  north, south, east, west;
    uint8_t has_reg;
} clb_t;

typedef struct {
    clb_t  grid[MAX_CLBS];
    uint8_t n_rows;
    uint8_t n_cols;
    bus_t  io_pins[BUS_WIDTH];
} fpga_t;

void fpga_init(fpga_t *fpga, uint8_t rows, uint8_t cols);
void fpga_cycle(fpga_t *fpga);
void fpga_set_pin(fpga_t *fpga, uint8_t pin, uint8_t value);
uint8_t fpga_get_pin(fpga_t *fpga, uint8_t pin);

#endif
