# Test for 74HC595 Shift Register
# Pin layout (from PNR):
#   Pin 0: ds_in (input)
#   Pin 1: oe_n_in (input)
#   Pin 2: q0 (output)
#   ...
#   Pin 10: q7 (output)
.section .text
.globl _start
_start:
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Enable outputs (oe_n=0)
    li t0, 0x40000100
    sw zero, 0(t0)

    # Set DS=1, clock the FPGA by writing to pins
    # After one FPGA cycle, q0 should reflect the shift
    # Initial state: all registers are 0, so q0 should be 0
    li t0, 0x40000200
    lw t1, 0(t0)
    bne t1, zero, fail     # q0 should be 0 initially

    # Success
    li a0, 0
    li a7, 93
    ecall

fail:
    li a0, 1
    li a7, 93
    ecall

.section .rodata
bitstream_path:
    .string "/tmp/shift_74hc595.bit"
