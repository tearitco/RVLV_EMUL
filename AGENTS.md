# Development Commands

## Building rvemu (standalone, x86_64)

Note: `src/bare_test.c` and `src/prisc_bare.c` contain RISC-V inline asm
and will fail to compile on x86_64. Use `main.c` instead:

```bash
gcc -O2 -std=gnu11 \
    -I"XO.SFPGA.NN/src" -Iincludes \
    -o /tmp/rvemu \
    main.c src/bus.c src/clint.c src/cpu.c src/csr.c src/dram.c \
    src/dtb.c src/fpu.c src/mmu.c src/plic.c src/sbi.c \
    src/selftest.c src/syscall.c src/trap.c src/uart.c src/virtio.c \
    XO.SFPGA.NN/src/fpga.c -lm
```

## Building PNR

```bash
gcc -Wall -Wextra \
    -I"XO.SFPGA.NN/src" -Iincludes \
    -o /tmp/pnr XO.SFPGA.NN/src/pnr.c
```

## Building standalone FPGA tests

```bash
gcc -DFPGA_STANDALONE \
    -I"includes" \
    -o /tmp/fpga_sim XO.SFPGA.NN/src/fpga.c -lm
/tmp/fpga_sim --test
```

## Compiling HDL0 bitstreams

```bash
/tmp/pnr XO.SFPGA.NN/hdl0/counter4.hdl0 /tmp/counter4.bit
/tmp/pnr XO.SFPGA.NN/hdl0/rv16_prog.hdl0 /tmp/rv16_prog.bit
```

Available HDL0 designs:
- `counter4.hdl0` - 4-bit ripple counter (counter8.hdl0 for 8-bit)
- `alu4.hdl0` - 4-bit ALU (AND, OR, XOR, NOT, ADD, SUB, SLT)
- `alu8.hdl0` - 8-bit ALU (same operations)
- `rv16_prog.hdl0` - Programmable rv16 core: x1=3, x2=5, x3=x1+x2=8
- `rv16_core.hdl0` - Hardcoded state machine (x1=5, x2=3, x3=8)
- `gpio_echo.hdl0` - 4-bit GPIO echo buffer
- `gpio_reg.hdl0` - 8-bit GPIO register
- `counter_74hc161.hdl0` - 74HC161 4-bit counter (load/enable/reset/carry)
- `latch_74hc373.hdl0` - 74HC374 8-bit D flip-flop with OE
- `decoder_74hc138.hdl0` - 74HC138 3-to-8 decoder with enable
- `uart.hdl0` - UART TX/RX (asynchronous serial)
- `bytelut_test.hdl0` - Byte-wide LUT test (identity + NOT operations using .LUT_BYTE)
- `alu32.hdl0` - 32-bit ALU design (AND, OR, XOR, ADD, SUB, SLT on 8-bit slices)

## FPGA key types

- `fpga_t` - FPGA fabric (wires, LUTs, registers, pins)
- `fpga_lut_t` - LUT entry (truth_table[256], n_inputs, inputs[8], output_wire, is_byte_lut)
- `fpga_lut_t` byte-wide LUT: uses full 8-bit input as index into 256-entry truth table (is_byte_lut=1)
- `fpga_reg_t` - Register entry (q[4], d[4], clk_en, clk_src, init_values[4], has_init)
- LUT index: `index = in0 | (in1<<1) | (in2<<2) | (in3<<3)`, masked to 0x07 (≤3 inputs) or 0x0F (>3 inputs)
- Byte-wide LUT (`.LUT_BYTE`): uses full 8-bit input byte as index into 256-entry truth table (no bit-masking)
- Register init: `init=0,0,0,0` sets initial Q values at bitstream load

## Running the full FPGA test suite

```bash
test/run_tests.sh
```

Prerequisites:
- `/tmp/rvemu` (built as above)
- `/tmp/pnr` (built as above)
- `riscv64-unknown-elf-gcc` (RISC-V cross-compiler)

## rvemu FPGA Syscalls

- `SYS_FPGA_LOAD = 10` (x15=10, x12=path_addr, x13=load_addr): Load .bit bitstream into FPGA
- `SYS_FPGA_READ = 11` (x15=11, x12=pin_id): Read FPGA pin value
- `SYS_FPGA_WRITE = 12` (x15=12, x12=pin_id, x13=value): Write FPGA pin value
- `SYS_EXIT = 93`

## Test binaries

Test programs in `test/` are compiled by `test/Makefile`:
- `fpga_alu_test.S` - ALU add operation via FPGA
- `fpga_gpio8_test.S` - GPIO write/read
- `fpga_prog_test.S` - rv16 programmable core (x1=3, x2=5, x3=8)
- `fpga_rv16_test.S` - rv16 hardcoded core
- `fpga_test.S` - Basic FPGA load + pin read
- `fpga_test_fail.S` - Negative test (expects exit code 1)
- `fpga_write_test.S` - FPGA pin write + readback
- `fpga_bytelut_test.S` - Byte-wide LUT test (identity and NOT via .LUT_BYTE)
- `fpga_segdec_test.S` - 7-segment display decoder test
- `fpga_alu32_test.S` - 32-bit ALU test (AND operation)

## Key paths

Two filesystem overlay paths exist:
- `1.RVEM/` (with dot) = actual repo files
- `1/RVEM/` (with slash) = different overlay layer (do NOT use)

Always use the `1.RVEM` path. Use `tee`/`cat >` (not write tool) when editing files in the overlay.

## Related Projects

### TEAR-IX ISO Project

A custom Ubuntu 22.04 derivative with RPG fantasy branding, BusyBox init,
UEFI-x86 support, and RISC-V port using rvemu stubs.

**Handoff instructions**: See `TEAR-IX/AGENT_HANDOFF.md` for full details.

**Quick start**:
```bash
cd ../TEAR-IX
cat AGENTS.md          # Agent workflow & constraints
cat AGENT_HANDOFF.md   # Handoff instructions
make iso               # Build x86_64 UEFI ISO
make riscv             # Build/run RISC-V variant
```

Note: The GitHub repo `git@github.com:tearitco/tear-ix.git` does not yet exist
on the remote. Create it before attempting `git push --set-upstream origin main`.
