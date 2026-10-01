# FPGA ↔ rvemu Integration Guide

## Architecture Overview

```
rvemu (RISC-V CPU emulator)
    │   MMIO bus (0x4000_0000 - 0x4FFF_FFFF)
    │
    ├──> FPGA Emulator (XO.SFPGA.NN)
    │        │
    │        ├── LUT fabric (74HC595 + 74HC251)
    │        ├── Switch box (74HC4066)
    │        ├── Tristate buffers (74HC125/126)
    │        └── Registers (74HC173)
    │
    └──> Virtual peripherals (HDL0 models)
             ├── UART (8N1, 115200 baud simulated)
             ├── GPIO (8-bit output)
             ├── VGA controller (for HDMI display)
             └── PS/2 keyboard interface
```

## Integration Points

### 1. Syscall Interface (prisc+x)
Add to `0.hdl0+prisc-bootstrap/prisc+x.c`:

```c
#define SYS_FPGA_LOAD  10  /* x15=10, x12=path, x13=load_addr */
#define SYS_FPGA_READ  11  /* x15=11, x12=pin, x13=addr -> x12=value */
#define SYS_FPGA_WRITE 12  /* x15=12, x12=pin, x13=value -> x12=0 */
```

### 2. Bus Bridge (rvemu src/bus.c)
```c
/* FPGA MMIO region: 0x40000000 - 0x4FFFFFFF */
if (addr >= FPGA_MMIO_BASE && addr < FPGA_MMIO_END) {
    return fpga_bridge_read(addr - FPGA_MMIO_BASE);
}
```

### 3. Peripheral Modeling
Each virtual peripheral is an HDL0 module compiled to our bitstream:
- `hdl0/uart.hdl0` - Async serial TX/RX
- `hdl0/gpio.hdl0` - GPIO input/output registers
- `hdl0/vga.hdl0` - 640×480 framebuffer controller (text mode)
- `hdl0/ps2.hdl0` - PS/2 keyboard scan code decoder

### 4. Test Workflow
```bash
# 1. Compile HDL0 peripheral models
./fpga-compile hdl0/uart.hdl0 uart.bit

# 2. Run rvemu with FPGA peripheral
./build/main -k test.bin --fpga uart.bit

# 3. The FPGA emulator processes UART signals
#    rvemu console output → FPGA UART TX → host stdout
```

## RISC-V Soft-Core Integration

The FPGA emulator can host a RISC-V soft-core (rv16/rv32):

```
rvemu (host, reference)
  │
  │  (program + expected results)
  │
  ▼
FPGA Emulator
  ├── Soft RISC-V core (HDL0)
  │     ├── Instruction fetch
  │     ├── Register file (LUT-based)
  │     ├── ALU (LUT + carry chain)
  │     └── Control logic
  └── Virtual peripherals
        ├── UART → host console
        ├── GPIO → debug LEDs
        └── Memory → VM mem[]

Comparison: rvemu output ↔ FPGA core output
```

## rv16 ISA Mapping to 7400-series LUTs

| rv16 Operation | LUT Count | 74HC ICs |
|---|---|---|
| Instruction fetch (256B ROM) | 64 LUT4 | 64x 74HC595 |
| Register file (8×16b) | 32 LUT4 | 32x 74HC595 |
| ALU (16-bit) | 32 LUT4 | 32x 74HC251 |
| Control logic | 16 LUT4 | 16x 74HC251 |
| Carry chain | 1 LUT4 | 1x 74HC173 |
| **Total per rv16 core** | **~145 LUT** | **~145 74HC ICs** |

Note: For ICE40 mapping, ~145 LUT4s maps to a small ICE40-HX1K (1280 LUTs).
