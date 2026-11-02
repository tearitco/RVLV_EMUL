    .section .text
    .globl _start
_start:
    # Load the byte LUT bitstream
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Write 0xAA to input pin 0 (0x40000000)
    li t0, 0x40000000
    li t1, 0xAA
    sw t1, 0(t0)

    # Read output pin 1 (0x40000100) - identity LUT, should be 0xAA
    li t0, 0x40000100
    lw t2, 0(t0)
    li t3, 0xAA
    bne t2, t3, fail

    # Read output pin 2 (0x40000200) - NOT LUT, should be 0x55
    li t0, 0x40000200
    lw t2, 0(t0)
    li t3, 0x55
    bne t2, t3, fail

    # Write 0x3C to input pin 0
    li t0, 0x40000000
    li t1, 0x3C
    sw t1, 0(t0)

    # Read identity output - should be 0x3C
    li t0, 0x40000100
    lw t2, 0(t0)
    li t3, 0x3C
    bne t2, t3, fail

    # Read NOT output - should be 0xC3
    li t0, 0x40000200
    lw t2, 0(t0)
    li t3, 0xC3
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
    .string "/tmp/bytelut_test.bit"
