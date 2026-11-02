# Test for 74HC4066 Quad Bilateral Switch
# Pin layout: pin0=en, pin1-4=in0-in3, pin5-8=out0-out3
.section .text
.globl _start
_start:
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Enable switch: en=1 (pin 0)
    li t0, 0x40000000
    li t1, 1
    sw t1, 0(t0)

    # Set in0=1 (pin 1)
    li t0, 0x40000100
    sw t1, 0(t0)

    # Read out0 (pin 5 at 0x40000500) - should be 1
    li t0, 0x40000500
    lw t2, 0(t0)
    li t3, 1
    bne t2, t3, fail

    # Disable: en=0
    li t0, 0x40000000
    sw zero, 0(t0)

    # Read out0 - should be 0
    li t0, 0x40000500
    lw t2, 0(t0)
    bne t2, zero, fail

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
    .string "/tmp/bus_74hc4066.bit"
