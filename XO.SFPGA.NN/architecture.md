# Discrete 7400-Series FPGA - Architecture Design

## Overview
An FPGA emulator built from discrete 74HC-series logic ICs, modeled in C using HDL0 as the HDL. Designed to be synthesizable onto Lattice ICE40 (open-source FPGA) and to serve as a platform for modeling computer peripherals that interface with the rvemu (RISC-V emulator).

## Target Architecture: Lattice ICE40 Compatible

### LUT Slice (Basic Logic Element)
Based on the "dfpga" reference architecture (mnemocron/my-discrete-fpga), adapted for discrete ICs:

**Per CLB Slice:**
- **4x 74HC595** - 8-bit shift registers (32 bits total → LUT memory)
  - 2x LUT4 (4-input, 16-bit truth table) + 2x LUT3 (3-input, 8-bit truth table)
- **2x 74HC251** - 8:1 multiplexer (one per LUT4 or shared for LUT3)
- **2x 74HC125** - Quad tristate buffer (output enable for LUT outputs)
- **2x 74HC126** - Quad tristate buffer (input selection, feedback)
- **1x 74HC173** - 4-bit D register (synchronous operations / flip-flop)
- **1x 74HC4066** - Quad bilateral switch (bus switching, flexible routing)

### Signal Buses
- **6-bit interconnect bus** (4 data + 2 control/priority)
- **Carry chain** for arithmetic (propagates through 74HC173 / direct LUT cascading)
- **Clock** - single global clock line

### Bitstream Format
Open, text-based (HDL0-compatible):

```
.CLUSTER <id>
  .LUT <lut_id> <inputs> <truth_table>
  .REG <reg_id> <input_mux> <clock_enable>
.END
```

JSON or raw binary variants supported for different backends.

## IC Reference Specifications

### 74HC595 - 8-Bit Shift Register
- **Function**: Serial-in, parallel-out shift register with storage register
- **Pins**: SER(ser in), SRCLK(shift clock), ~SRCLR(async clear), ~RCLK(storage clk), Q0-Q7(outputs)
- **Operation**: Data shifts on SRCLK edge; transfers to storage on RCLK edge
- **FPGA Role**: LUT truth table memory (16-bit LUT4 = 2 cascaded 595s)

### 74HC251 - 8:1 Multiplexer
- **Function**: 8-channel data selector
- **Pins**: D0-D7(data inputs), S0-S2(select), ~E(active low enable), Y(output)
- **Operation**: Selects one of D0-D7 based on S2:S0, active when ~E=0
- **FPGA Role**: LUT input selector / truth table lookup

### 74HC125 - Quad Tristate Buffer
- **Function**: 4 independent buffers with active-low enable
- **Pins**: A1-A4(input), Y1-Y4(output), ~E1-E4(enable, active low)
- **Operation**: When enabled, Y follows A; when disabled, Y is high-Z
- **FPGA Role**: Output driver for LUT results, bus isolation

### 74HC126 - Quad Buffer (active HIGH enable)
- **Function**: Same as 74HC125 but with active-high enable
- **FPGA Role**: Input buffering, bus switching direction control

### 74HC173 - 4-Bit D-Type Register
- **Function**: 4-bit register with synchronous reset/preset
- **Pins**: D0-D3(data in), Q0-Q3(data out), CLK, ~CLR(async clear), ~OE(output enable)
- **Operation**: Captures D0-D3 on CLK edge; reset/preset async
- **FPGA Role**: Flip-flop element, synchronous state storage

### 74HC4066 - Quad Bilateral Switch
- **Function**: 4 independent bidirectional analog switches
- **Pins**: A1-B1, A2-B2, A3-B3, A4-B4 (switched pairs), +VDD/+VSS, controls
- **Operation**: When control HIGH, A-B connection made; when LOW, open
- **FPGA Role**: Crossbar switching, flexible interconnect routing

## Integration with rvemu Pipeline

### Use Case 1: Peripheral Emulation
```
rvemu (RISC-V CPU) ←→ [FPGA Emulator] ←→ [Virtual Peripherals]
                            ↑
                         HDL0 modules
                    (GPU, HDMI, UART, PS/2)
```

The FPGA emulator acts as a virtual peripheral bus:
- rvemu loads a .pal/.hdl0 file describing virtual hardware
- FPGA emulator instantiates the hardware logic
- rvemu's `mmio_read`/`mmio_write` calls are routed to FPGA I/O pins
- Virtual peripherals process signals and respond

### Use Case 2: Soft-Core RISC-V
The FPGA emulator can host a RISC-V soft core (rv1/rv16 scale):
- Soft core written in HDL0, compiled to our bitstream format
- Loaded into the discrete FPGA emulator
- rvemu (as a separate reference) runs the same program for comparison
- This validates both the FPGA fabric AND the soft core

### Integration Points in rvemu Codebase
- **`src/bus.c`**: Intercept MMIO addresses (0x4000_0000+) → forward to FPGA emulator
- **`src/syscall.c`**: Add `SYS_FPGA_LOAD` (syscall 10) to load .pal/.hdl0 into FPGA
- **`src/uart.c`**: Route serial I/O through FPGA's UART peripheral model
- **`src/virtio.c`**: Optionally route through FPGA-controlled virtio bridge

## Roadmap

### Phase 1: FPGA Emulator Core (Weeks 1-3)
- [ ] `fpga.h/c` - Core FPGA fabric model
- [ ] `lut.c` - 74HC595+74HC251 LUT implementation
- [ ] `switch_box.c` - 74HC4066/125 routing fabric
- [ ] `bitstream.c` - Parse/write our bitstream format
- [ ] `fpga_test.c` - Simple LUT truth table test

### Phase 2: HDL0 Compiler (Weeks 4-6)
- [ ] Extend existing HDL0 parser (`0.hdl0+prisc-bootstrap/`)
- [ ] Add syntax: `.LUT`, `.REG`, `.CONNECT`, `.PIN`
- [ ] Compile HDL0 → bitstream format
- [ ] Test: compile a 4-bit counter, run in FPGA emulator

### Phase 3: rvemu Integration (Weeks 7-8)
- [ ] Add FPGA bus bridge in rvemu
- [ ] Add `SYS_FPGA_LOAD` syscall to prisc+x
- [ ] Test: rvemu ↔ FPGA emulator communication
- [ ] Test: FPGA-hosted UART loop with prisc+x

### Phase 4: Soft-Core RISC-V (Weeks 9-12)
- [ ] Write rv16 (16-bit RISC-V subset) in HDL0
- [ ] Compile to our bitstream
- [ ] Run on FPGA emulator
- [ ] Compare with rvemu reference execution

### Phase 5: Real Hardware Path (Future)
- [ ] Map bitstream to Lattice ICE40 bitstream format
- [ ] Use Icestorm toolchain for synthesis/par/bitstream
- [ ] Physical implementation on ICE40-HX1K-TQ144

## File Structure

```
XO.SFPGA.NN/
  architecture.md    - This file (architecture + IC specs)
  ROADMAP.md         - Timeline, milestones, roadmap
  S.FPGA.nn.plan.txt - Original plan
  include/
    fpga.h           - FPGA fabric types
    lut.h            - LUT definitions
    bitstream.h      - Bitstream format
  src/
    fpga.c           - FPGA fabric model (main)
    lut.c            - LUT implementation (74HC595 + 74HC251)
    switchbox.c      - Routing fabric (74HC4066, 74HC125, 74HC126)
    register.c       - Flip-flop element (74HC173)
    bitstream.c      - Bitstream parser/emitter
    fpga_test.c      - Unit tests
  hdl0/
    components.hdl0  - IC-level models (74HC595.v → 74HC595.hdl0)
    luts.hdl0        - LUT primitive definitions
    counter4.hdl0    - 4-bit counter example
    uart.hdl0        - UART peripheral model
  test/
    test_lut.sh      - LUT truth table test
    test_counter.sh  - 4-bit counter test
    test_bridge.sh   - rvemu ↔ FPGA bridge test
```
