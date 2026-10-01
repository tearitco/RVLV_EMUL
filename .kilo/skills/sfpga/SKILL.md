---
name: sfpga-7400-fpga
description: Implement a discrete 7400-series FPGA emulator using LUT-style logic ICs (74HC595 shift regs, 74HC251 muxes, 74HC125/126 tristates, 74HC173 registers, 74HC4066 bus switches) with HDL0 compilation, bitstream generation, and rvemu integration.
---

# SFPGA - Discrete 7400-Series FPGA

## Overview
Build a software FPGA fabric using discrete 74HC-series logic ICs, compatible with Lattice ICE40 for eventual hardware synthesis.

## Architecture
- **LUT slices**: 74HC595 (truth table memory) + 74HC251 (8:1 mux for lookup)
- **Routing**: 74HC4066 (bidirectional switches) for interconnect
- **Tristate**: 74HC125/126 for output enable and input buffering
- **Registers**: 74HC173 for flip-flop elements
- **6-bit interconnect bus** (4 data + 2 control)

## Directory
`XO.SFPGA.NN/` - all work confined to this directory

## Coordination
- Do NOT modify files outside `XO.SFPGA.NN/` without coordinating
- Build/test with `bash XO.SFPGA.NN/test/test_fpga.sh`
- Communicate rvemu integration needs via shared board
