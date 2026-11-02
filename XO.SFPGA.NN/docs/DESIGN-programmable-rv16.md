# RV16 Programmable Core on FPGA — Design Document

## Objective
Extend rv16_core to be **programmable** — load RISC-V-like instructions via bitstream and execute them on the FPGA fabric.

## Current State
- `rv16_core.hdl0`: Hardcoded state machine (load x1=5, x2=3, add→x3=8, output, halt)
- FPGA fabric: 64 CLBs, each with 4 LUTs (4-bit truth tables) + 4 4-bit registers
- LUTs: 16-entry × 4-bit truth tables, up to 4 inputs (index masked to 0x0F for 4 inputs, 0x07 for ≤3)
- Registers: 4-bit, clocked with clk_en, load from d[0..3] inputs
- Wires: 256 total, indices 0=const_low, 1=const_high, 2=clk, then sequential

## Constraints
- **4-bit datapath**: LUT outputs are 4-bit (values 0–15), registers are 4-bit
- 256 LUTs max, 256 registers max
- Bus width between CLBs: 6 bits
- LUT index: `in0 | (in1<<1) | (in2<<2) | (in3<<3)`, masked to 4 bits (16 entries)

## Architecture

### Instruction Set (4-bit, packed in 4-bit registers)
Using 4-bit instructions with opcode in bits 0-1, operands in bits 2-3:

| Opcode | Name      | Format        | Description              |
|--------|-----------|---------------|--------------------------|
| 00     | LOD       | `00 imm3`     | Load constant (0-7)      |
| 01     | ADD       | `01 dst src`  | dst = dst + src          |
| 10     | OUT       | `10 reg 0`    | Output reg to pin0       |
| 11     | HLT       | `11 00`       | Halt the core            |

### Program Memory
- Use 4-bit registers as instruction memory (init values via bitstream)
- 16 instructions max (one per register, using half the CLB registers)
- PC is a 4-bit register counting 0→15, wraps around

### Register File
- 4 general-purpose registers (x0-x3), each 4-bit
- x0 = accumulator (implicit destination)
- x1-x3 = general purpose
- Implemented with .REG entries

### Control Flow
State machine per instruction cycle:

| State | Action                          |
|-------|---------------------------------|
| IF    | Read instruction from PC addr   |
| ID    | Decode opcode, select operands  |
| EX    | Execute ALU operation           |
| WB    | Write result back            |

## Implementation Plan

### 1. Extend rv16_core.hdl0 → rv16_prog.hdl0

```hdl0
# rv16_prog — Programmable 4-bit RISC core on FPGA

.FABRIC 8 8   # 64 CLBs (8x8), enough for 256 LUTs

# Clock and PC
.LUT pc_inc [pc_out, 1] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,0}
.REG pc [pc_inc.out] clk_en=1

# Program memory (16 instructions, 4-bit each)
# Uses 4 LUTs as ROM, indexed by PC
.LUT imem0 [pc_out] = {0,0,0,0, 01,01,10,11, ...}  # first 4 instr
.LUT imem1 [pc_out] = {...}
.LUT imem2 [pc_out] = {...}
.LUT imem3 [pc_out] = {...}

# Register file: x0-x3
.REG x0_reg [x0_next]
.REG x1_reg [x1_next]
.REG x2_reg [x2_next]
.REG x3_reg [x3_next]

# ALU
.LUT alu_add [x0_reg.out, x1_reg.out] = {0,1,2,3,...}  # truth table for add

# Output
.PIN result = alu_out.out

.END
```

### 2. Extend .REG syntax (pnr.c)

Add support for **init values** in .REG:

```
.REG name [d0,d1,d2,d3] init=0,1,0,1 clk_en=1 clk
```

This stores initial D input values that fpga_bs_apply loads at startup.

### 3. Extend bitstream format (fpga_hw.h, fpga.c)

Add `init_values[4]` field to `fpga_reg_t` and `bs_entry_t`:
- pnr.c parses `.REG ... init=val0,val1,val2,val3`
- fpga_bs_apply sets `reg->d[0..3]` to init values on apply (before first cycle)

### 4. Test program

Example HDL0 program to test:
```
# Load x0=3, x1=5, ADD → x0=8, OUT, HLT
# Instructions (PC 0-3):
# PC0: LOD 3      (instruction=0x03)
# PC1: LOD 5      (instruction=0x05)  
# PC2: ADD x0+x1  (instruction=0x1C)
# PC3: OUT         (instruction=0x08)
# PC4: HLT         (instruction=0x0C)
```

## Verification Flow

1. Write `rv16_prog.hdl0` with init values
2. Run `./pnr rv16_prog.hdl0 rv16_prog.bit`
3. Load via SYS_FPGA_LOAD in RISC-V test
4. Run on rvemu with FPGA
5. Check pin0 output = 8 (3+5)

## Files to Create/Modify

- [ ] `XO.SFPGA.NN/hdl0/rv16_prog.hdl0` — programmable core
- [ ] `XO.SFPGA.NN/hdl0/test_add.hdl0` — test program (3+5=8)
- [ ] `src/pnr.c` — add init= parsing for .REG
- [ ] `includes/fpga_hw.h` — add init_values to fpga_reg_t, bs_entry_t
- [ ] `XO.SFPGA.NN/src/fpga.c` — apply init values on bs_apply
- [ ] `test/fpga_prog_test.S` — RISC-V test to load+run on FPGA
- [ ] `test/run_tests.sh` — add new test

## Future: 32-bit and 64-bit Support

The 4-bit approach is a stepping stone. To support 32-bit and 64-bit RISC-V:

### 32-bit Path
- **Wider LUTs**: Pack 8 LUTs per 32-bit value (4 bits each, using LUTs as nibbles)
- **Wide registers**: Chain 8 × 4-bit registers for 32-bit storage
- **Instruction memory**: 8 LUTs per instruction (32 bits), PC = 32-bit counter
- **ALU**: Instantiate alu8.hdl0 (already supports 32-bit values)
- **LUTs needed**: ~256 LUTs for register file + ~128 for ALU + ~64 for control ≈ 448 LUTs
- **Fabric**: Need 8×8 or 16×16 grid (64-256 CLBs)

### 64-bit Path
- Same techniques but with 16 LUTs per 64-bit value
- Would benefit from a wider LUT primitive (8-bit or 16-bit LUTs)

### HDL0 Abstraction Layer
Introduce "wide LUT" and "wide register" directives that pnr.c expands:
```
.LUT32 alu_out [a[31:0], b[31:0]] = {truth table}
.REG32 pc [alu_out.out] init=0x00000000 clk_en=1
```
pnr.c expands each `.LUT32` into 8 `.LUT4` entries (one per nibble).
