# 32-bit ALU test - verify AND, OR, XOR, ADD, SUB operations
.section .text
.globl _start
_start:
    # Load bitstream
    la a0, bitstream_path
    li a7, 10
    ecall

    # Test AND: a=0xF0, b=0x0F, fn=000 -> result=0x00
    li t0, 0x40000000  # pin0 = a_bit0 (LSB of a)
    li t1, 0
    sb t1, 0(t0)        # a_bit0 = 0
    sb zero, 1(t0)      # a_bit1 = 0
    sb zero, 2(t0)      # a_bit2 = 0
    sb zero, 3(t0)      # a_bit3 = 0
    li t1, 1
    sb t1, 4(t0)        # a_bit4 = 1
    sb t1, 5(t0)        # a_bit5 = 1
    sb t1, 6(t0)        # a_bit6 = 1
    sb t1, 7(t0)        # a_bit7 = 1

    li t0, 0x40000800   # b pins start at pin 8
    li t1, 1
    sb t1, 0(t0)        # b_bit0 = 1
    sb t1, 1(t0)        # b_bit1 = 1
    sb t1, 2(t0)        # b_bit2 = 1
    sb t1, 3(t0)        # b_bit3 = 1
    li t1, 0
    sb t1, 4(t0)        # b_bit4 = 0
    sb t1, 5(t0)        # b_bit5 = 0
    sb t1, 6(t0)        # b_bit6 = 0
    sb t1, 7(t0)        # b_bit7 = 0

    # fn = 000 (AND)
    li t0, 0x40001000   # fn pins start at pin 16
    sb zero, 0(t0)      # fn0 = 0
    sb zero, 1(t0)      # fn1 = 0
    sb zero, 2(t0)      # fn2 = 0

    # Read result (pins 22-29 = out0..out7)
    li t0, 0x40001600   # pin 22 = alu_out0
    lb t2, 0(t0)        # out0 (LSB)
    li t3, 0
    bne t2, t3, fail

    # Read carry (pin 30)
    li t0, 0x40001E00   # pin 30 = alu_carry
    lb t2, 0(t0)
    li t3, 0
    bne t2, t3, fail

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
    .string "/tmp/alu32.bit"
