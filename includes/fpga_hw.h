#ifndef FPGA_HW_H
#define FPGA_HW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FPGA_MAX_WIRES     256
#define FPGA_MAX_PINS      16
#define FPGA_MAX_CLBS      64
#define FPGA_MAX_LUTS_PER_CLB  4
#define FPGA_MAX_REGS_PER_CLB  4
#define FPGA_LUT_SIZE      16
#define FPGA_BUS_WIDTH     6
#define FPGA_MAX_SWITCHBOXES 256
#define FPGA_SB_BUS_DIRECT 0xFF

#define OP_LUT3    0
#define OP_LUT4    1
#define OP_REG     2
#define OP_CONNECT 3
#define OP_PIN     4
#define OP_CONFIG  5

typedef struct {
    uint8_t truth_table[FPGA_LUT_SIZE];
    uint8_t n_inputs;
    uint8_t inputs[8];
    uint8_t output_wire;
    uint8_t output;
} fpga_lut_t;

typedef struct {
    uint8_t d[4];
    uint8_t q[4];
    uint8_t clk_en;
    uint8_t clk;
    uint8_t output_wire;
} fpga_reg_t;

typedef struct {
    uint8_t values[FPGA_BUS_WIDTH];
} fpga_bus_t;

typedef struct {
    uint8_t src_bus;
    uint8_t src_bit;
    uint8_t dst_bus;
    uint8_t dst_bit;
} fpga_switchbox_conn_t;

typedef struct {
    fpga_lut_t   luts[FPGA_MAX_LUTS_PER_CLB];
    fpga_reg_t   regs[FPGA_MAX_REGS_PER_CLB];
    fpga_bus_t   north, south, east, west;
    uint8_t      has_reg;
} fpga_clb_t;

typedef struct {
    fpga_switchbox_conn_t conns[FPGA_BUS_WIDTH];
    uint8_t n_conns;
} fpga_switchbox_t;

typedef struct {
    uint8_t op;
    uint8_t clb_row;
    uint8_t clb_col;
    uint8_t lut_idx;
    uint8_t n_inputs;
    uint8_t inputs[FPGA_BUS_WIDTH];
    uint8_t output_wire;
    uint8_t truth_table[16];
    uint8_t reg_idx;
    uint8_t d[4];
    uint8_t clk_en;
    uint8_t clk_src;
    uint8_t pin_id;
    uint8_t pin_dir;
    uint8_t src_wire;
    uint8_t dst_wire;
    int32_t config_value;
    char config_name[32];
} fpga_bs_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint16_t num_entries;
    uint8_t  rows;
    uint8_t  cols;
    fpga_bs_entry_t entries[256];
} fpga_bitstream_t;

typedef struct {
    fpga_clb_t    grid[FPGA_MAX_CLBS];
    uint8_t       n_rows;
    uint8_t       n_cols;
    fpga_bus_t    io_pins[FPGA_MAX_PINS];
    uint8_t       pin_dir[FPGA_MAX_PINS];
    uint8_t       wires[FPGA_MAX_WIRES];
    fpga_switchbox_t sb[FPGA_MAX_SWITCHBOXES];
    uint8_t       n_switchboxes;
    uint8_t       clk_div;
    uint8_t       clk_counter;
    uint8_t       clk;
} fpga_t;

#define FPGA_BITSTREAM_MAGIC 0x584F5346
#define FPGA_BITSTREAM_VERSION 1

int fpga_bs_load(fpga_bitstream_t *bs, const char *filename);
int fpga_bs_apply(fpga_t *fpga, const fpga_bitstream_t *bs);
void fpga_cycle(fpga_t *fpga);
void fpga_set_wire(fpga_t *fpga, uint8_t wire, uint8_t value);
uint8_t fpga_get_wire(fpga_t *fpga, uint8_t wire);
void fpga_set_pin(fpga_t *fpga, uint8_t pin, uint8_t value);
uint8_t fpga_get_pin(fpga_t *fpga, uint8_t pin);

#define FPGA_MMIO_BASE  0x40000000ULL
#define FPGA_MMIO_SIZE  0x10000000ULL

#define FPGA_PIN_ADDR(pin) (FPGA_MMIO_BASE + (pin) * 0x100)

#define SYS_FPGA_LOAD 10

#ifdef __cplusplus
}
#endif

#endif
