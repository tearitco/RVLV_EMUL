.section .text
    .globl _start
_start:
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Write 0x05 to input pin 0 (0x40000000)
    li t0, 0x40000000
    li t1, 0x05
    sw t1, 0(t0)

    # Write 0x0A to input pin 1 (0x40000100)
    li t0, 0x40000100
    li t1, 0x0A
    sw t1, 0(t0)

    # Write 0x03 to input pin 2 (0x40000200)
    li t0, 0x40000200
    li t1, 0x03
    sw t1, 0(t0)

    # Write 0x0C to input pin 3 (0x40000300)
    li t0, 0x40000300
    li t1, 0x0C
    sw t1, 0(t0)

    # Read output pin 4 (0x40000400) - should be 0x05
    li t0, 0x40000400
    lw t2, 0(t0)
    andi t2, t2, 0x0F
    li t3, 0x05
    bne t2, t3, fail

    # Read output pin 5 (0x40000500) - should be 0x0A
    li t0, 0x40000500
    lw t2, 0(t0)
    andi t2, t2, 0x0F
    li t3, 0x0A
    bne t2, t3, fail

    # Read output pin 6 (0x40000600) - should be 0x03
    li t0, 0x40000600
    lw t2, 0(t0)
    andi t2, t2, 0x0F
    li t3, 0x03
    bne t2, t3, fail

    # Read output pin 7 (0x40000700) - should be 0x0C
    li t0, 0x40000700
    lw t2, 0(t0)
    andi t2, t2, 0x0F
    li t3, 0x0C
    bne t2, t3, fail

    li a0, 0
    li a7, 93
    ecall

fail:
    li a0, 1
    li a7, 93
    ecall

    .section .rodata
bitstream_path:
    .string "/tmp/gpio_reg.bit"