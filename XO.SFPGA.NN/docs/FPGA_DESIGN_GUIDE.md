# FPGA Design Guide

## Overview

The SFPGA (Simple FPGA) is a lightweight FPGA fabric emulator that supports:
- 4-input LUTs (truth table with 8 or 16 entries)
- Byte-wide LUTs (256-entry truth table, full 8-bit index)
- 4-bit registers with init values
- Switch boxes
- Configurable I/O pins
- 74HC-series discrete IC models via HDL0 composition

## 74HC-Series Discrete IC Designs

The following HDL0 designs implement classic 74HC-series logic ICs using LUT4-based composition:

### 74HC595 - 8-bit Serial-In Parallel-Out Shift Register (`shift_74hc595.hdl0`)
- Inputs: `ds` (serial data), `oe_n` (output enable, active-low), `mr_n` (master reset, active-low)
- Outputs: `q0`-`q7` (parallel outputs)
- Shifts `ds` into bit 7 on each clock cycle; `oe_n=0` enables outputs

### 74HC251 - 8:1 Line Selector/Decoder (`mux_74hc251.hdl0`)
- Inputs: `d0`-`d7` (data), `s0`-`s2` (select), `g_n` (enable, active-low)
- Outputs: `y` (selected data), `y_bar` (complement)
- When `g_n=0`: `y = d_index`, `y_bar = ~y` where `index = s0|s1<<1|s2<<2`
- When `g_n=1`: `y=0`, `y_bar=1` (disabled)
- Built from LUT4-cascaded 2:1 mux tree

### 74HC125 - Quad 3-State Buffer (`tristate_74hc125.hdl0`)
- Inputs: `oe` (enable, active-high), `a0`-`a3` (data)
- Outputs: `y0`-`y3`
- When `oe=1`: `yn = an`; when `oe=0`: `yn = 0` (tri-stated)

### 74HC126 - Quad 3-State Buffer (`tristate_74hc126.hdl0`)
- Inputs: `oe_n` (enable, active-low), `a0`-`a3` (data)
- Outputs: `y0`-`y3`
- When `oe_n=0`: `yn = an`; when `oe_n=1`: `yn = 0` (tri-stated)

### 74HC4066 - Quad Bilateral Switch (`bus_74hc4066.hdl0`)
- Inputs: `en` (enable), `in0`-`in3` (data)
- Outputs: `out0`-`out3`
- When `en=1`: `outn = inn`; when `en=0`: `outn = 0` (switch open)

## HDL0 Format

### .LUT - 4-input LUT

```
.LUT name [in0, in1, in2, in3] = {tt0, tt1, ..., tt15}
```

- `name`: output signal name
- `in0..in3`: input signals (can use `0` or `1` for constants)
- Truth table: 16 entries (2^4), indexed by `in0 | (in1<<1) | (in2<<2) | (in3<<3)`

### .LUT_BYTE - Byte-wide LUT (256-entry)

```
.LUT_BYTE name [in0] = {tt0, tt1, ..., tt255}
```

- `name`: output signal name
- `in0`: single byte-wide input signal
- Truth table: 256 entries (2^8), indexed by the full byte value of `in0`
- **Key difference**: regular `.LUT` can only distinguish bit patterns (e.g., inputs 0x00-0x0F all map to index 0), while `.LUT_BYTE` can distinguish all 256 byte values

#### Example: Byte-wide identity and NOT

```
.LUT_BYTE id_byte [byte_in] = {0,1,2,...,255}
.LUT_BYTE not_byte [byte_in] = {255,254,253,...,0}
```

### .REG - Register

```
.REG name [d0, d1, d2, d3] init=i0,i1,i2,i3
```

- `d0..d3`: data inputs (each carries a 1-bit value in bit 0)
- `init=`: sets initial Q values at bitstream load

### .PIN - I/O Pin

```
.PIN name = signal
```

- Direction is inferred: if `name` matches an existing LUT input signal, pin is input (dir=0); otherwise output (dir=1)

### .CONNECT - Wire connection

```
.CONNECT src -> dst
```

### .FABRIC - Fabric dimensions

```
.FABRIC rows cols
```

### .CONFIG - Configuration

```
.CONFIG clk_div=N
```

## Bitstream Format

The PNR tool compiles HDL0 into a text-based bitstream (`.bit`) with the same directive names but resolved numeric wire IDs instead of signal names.

Wire ID assignment:
- 0: constant low
- 1: constant high
- 2: clock
- 3+: user signals (assigned sequentially by PNR)

## FPGA Syscalls (rvemu)

| Syscall | Number | Arguments |
|---------|--------|-----------|
| SYS_FPGA_LOAD | 10 | x12=path_addr, x13=load_addr (unused) |
| SYS_EXIT | 93 | x10=exit_code |

FPGA MMIO:
- Base: 0x40000000
- Pin N: 0x40000000 + N * 0x100

## Testing

Test programs in `test/` (compiled by `test/Makefile`):
- `fpga_alu_test.S` - 4-bit ALU test
- `fpga_alu32_test.S` - 32-bit ALU test (8-bit slices, AND operation)
- `fpga_bytelut_test.S` - Byte-wide LUT test
- `fpga_74hc125_test.S` - 74HC125 quad 3-state buffer test
- `fpga_74hc126_test.S` - 74HC126 quad 3-state buffer test
- `fpga_74hc251_test.S` - 74HC251 8:1 mux test
- `fpga_74hc4066_test.S` - 74HC4066 quad bilateral switch test
- `fpga_74hc595_test.S` - 74HC595 shift register test
- `fpga_gpio8_test.S` - GPIO write/read
- `fpga_prog_test.S` - rv16 programmable core
- `fpga_rgb_test.S` - RGB framebuffer gradient test
- `fpga_rv16_test.S` - rv16 hardcoded core
- `fpga_segdec_test.S` - 7-segment decoder
- `fpga_test.S` - Basic FPGA load + pin read
- `fpga_test_fail.S` - Negative test
- `fpga_write_test.S` - Pin write + readback

Run the full test suite:
```bash
test/run_tests.sh
```

All 16 tests should pass.

Standalone FPGA tests:
```bash
/tmp/fpga_sim --test
```
