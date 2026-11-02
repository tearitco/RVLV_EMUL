    .section .text
    .globl _start
_start:
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall
    li t0, 0x40000000
    li t1, 0x30
    sw t1, 0(t0)
    li t0, 0x40000100
    lw t2, 0(t0)
    li t3, 0x3F
    bne t2, t3, fail
    li t0, 0x40000000
    li t1, 0x31
    sw t1, 0(t0)
    li t0, 0x40000100
    lw t2, 0(t0)
    li t3, 0x06
    bne t2, t3, fail
    li t0, 0x40000000
    li t1, 0x41
    sw t1, 0(t0)
    li t0, 0x40000100
    lw t2, 0(t0)
    li t3, 0x77
    bne t2, t3, fail
    li t0, 0x40000000
    li t1, 0x46
    sw t1, 0(t0)
    li t0, 0x40000100
    lw t2, 0(t0)
    li t3, 0x71
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
    .string "/tmp/seg_decoder.bit"
