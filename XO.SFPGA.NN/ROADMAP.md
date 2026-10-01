# Discrete FPGA - Development Roadmap

## Phase 1: FPGA Emulator Core (Current Focus)
**Goal**: Build a software emulator of the discrete 7400-series FPGA fabric

### Tasks
- [ ] Create `include/fpga.h` with core types:
  - `lut_t` - 16-bit truth table + output
  - `slice_t` - 2x LUT4 + LUT3 + register + switch matrix
  - `bus_t` - 6-bit interconnect bus
  - `fpga_t` - full fabric with N×M CLB grid
- [ ] Create `include/bitstream.h` with bitstream format definitions
- [ ] Implement `src/lut.c`:
  - `lut_init()` - load truth table from 74HC595 state
  - `lut_eval()` - select output using 74HC251 mux logic
  - Support 3-input (8-bit) and 4-input (16-bit) LUTs
- [ ] Implement `src/switchbox.c`:
  - `switch_eval()` - 74HC4066 bidirectional switch matrix
  - `tristate_eval()` - 74HC125/126 buffer/output-enable
- [ ] Implement `src/register.c`:
  - `reg_eval()` - 74HC173 4-bit D register with clock enable
  - `reg_cycle()` - clock edge behavior
- [ ] Implement `src/fpga.c`:
  - `fpga_init()` - initialize fabric from bitstream
  - `fpga_cycle()` - execute one clock cycle (all LUTs → registers)
  - `fpga_set_pin()` / `fpga_get_pin()` - I/O interface
- [ ] Implement `src/bitstream.c`:
  - `bitstream_load()` - parse HDL0/HDL0-binary format
  - `bitstream_save()` - emit bitstream
  - Support both text (.bit) and binary (.bin) formats
- [ ] Write `src/fpga_test.c`:
  - Test: AND gate (LUT2 with truth table 0xA8)
  - Test: 4-bit counter (4 LUTs cascaded via carry chain)
  - Test: mux (LUT with 3-select input)

### Expected Outcome
- FPGA emulator that can evaluate a 4-bit counter at 100MHz simulated clock
- Test suite showing correct LUT truth table evaluation

## Phase 2: HDL0 Compiler
**Goal**: Compile HDL0 to our bitstream format

### Tasks
- [ ] Extend `0.hdl0+prisc-bootstrap/` parser with FPGA-specific syntax
- [ ] Add `.LUT` directive:
  ```
  .LUT lut1 [a, b, c, 0] = {0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0}
  ```
- [ ] Add `.REG` directive:
  ```
  .REG r0 clk=en, d=lut1.out
  ```
- [ ] Add `.CONNECT` directive for routing:
  ```
  .CONNECT lut1.out -> clb3.in[0]
  ```
- [ ] Add `.PIN` for I/O:
  ```
  .PIN clk_in -> clk_bus
  .PIN output -> clb1.out
  ```
- [ ] Compile `hdl0/counter4.hdl0` → bitstream
- [ ] Load into FPGA emulator, verify counter output

### Expected Outcome
- HDL0 files compile to bitstream
- 4-bit counter runs correctly in FPGA emulator

## Phase 3: rvemu Integration
**Goal**: Bridge rvemu (RISC-V emulator) to the FPGA emulator for peripheral modeling

### Tasks
- [ ] In `src/bus.c`: Add FPGA MMIO address range (0x4000_0000 - 0x4FFF_FFFF)
- [ ] In `src/syscall.c`: Add `SYS_FPGA_LOAD` (syscall 10)
  - Params: x12=path to .bit/.hdl0, x13=load_addr
  - Loads bitstream into FPGA emulator
- [ ] Add `src/fpga_bridge.c` to rvemu:
  - `fpga_mmio_read(addr)` → reads FPGA output pin
  - `fpga_mmio_write(addr, val)` → writes FPGA input pin
- [ ] In `0.hdl0+prisc-bootstrap/prisc+x.c`: Add `fpga_load "path.bit"` op
- [ ] Test: FPGA-hosted UART loop
  - rvemu → FPGA_UART (RX pin) → prisc+x reads via SYS_UART_READ
  - prisc+x writes via SYS_UART_WRITE → FPGA_UART (TX pin) → rvemu console

### Expected Outcome
- rvemu and FPGA emulator communicate via shared bus
- Virtual UART peripheral works end-to-end

## Phase 4: Soft-Core RISC-V
**Goal**: Implement a RISC-V soft core in HDL0, run on FPGA emulator

### Tasks
- [ ] Design rv16 ISA (16-bit subset of RISC-V)
  - Registers: x0-x7 (3-bit)
  - Instructions: add, sub, load, store, branch, jal
  - Memory: 256 bytes
- [ ] Write `hdl0/rv16_core.hdl0`:
  - Instruction fetch (PC register)
  - Instruction decode (LUT-based decoder)
  - Register file (8x 4-bit registers using LUTs)
  - ALU (LUT-based arithmetic)
  - Control logic
- [ ] Compile to bitstream, run in FPGA emulator
- [ ] Cross-verify with prisc+x reference execution
- [ ] Extend to rv32 (full 32-bit) if time permits

### Expected Outcome
- rv16 core runs in FPGA emulator
- Same program produces identical results in rvemu and FPGA soft-core

## Phase 5: Real Hardware (Future)
**Goal**: Synthesize to Lattice ICE40 FPGA

### Tasks
- [ ] Map bitstream to ICE40 SB_LUT4 primitives
- [ ] Use Icestorm (yosys/arachne-pnr) for place & route
- [ ] Generate `.bin` for ICE40-HX1K-TQ144
- [ ] Physical test: program ICE40, verify counter output

## Phase 6: WiFi-SDR Interconnect (Long-term)
**Goal**: Enable chip-to-chip communication over emulated TCP/IP using WiFi-SDR protocol

### Vision
Two FPGA-hosted chips communicate via a simulated wireless link:
```
FPGA Chip A ←→ [WiFi-SDR Emulator] ←→ TCP/IP ←→ [WiFi-SDR Emulator] ←→ FPGA Chip B
```

### Components
- [ ] `hdl0/wifi_mac.hdl0` - WiFi MAC layer (CSMA/CA, frame format)
- [ ] `src/sdr.c` - Software-defined radio simulator (modulation/demodulation)
- [ ] TCP socket bridge between FPGA emulator and host network
- [ ] Frame encoding/decoding (802.11-like) in LUT fabric

### Tasks
- [ ] Define packet format for inter-chip communication
- [ ] Implement SDR modulation (BPSK/QPSK) in software
- [ ] TCP bridge: FPGA TX → host TCP → FPGA RX
- [ ] Test: two soft-cores exchange data packets

## Dependencies
- Phase 2 depends on Phase 1
- Phase 3 depends on Phases 1+2
- Phase 4 depends on Phase 3
- Phase 5 depends on Phase 4

## Current Status
Phase 1: Implemented — FPGA emulator core (`fpga.c`) compiles with 18/18 test cases passing.
Phase 2: Implemented — HDL0 compiler (`pnr.c`) compiles `.hdl0` → `.bit` format.
Verilog-to-HDL0 compiler (`verilog.c`) compiles a simple Verilog subset to HDL0 text.
Full pipeline tested: Verilog → HDL0 → bitstream → FPGA simulation.
