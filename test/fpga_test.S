    .section .text
    .globl _start
_start:
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall
    
    li t0, 0x40000000
    lw t1, 0(t0)
    li t2, 1
    bne t1, t2, fail
    
    li a0, 0
    li a7, 93
    ecall

fail:
    li a0, 1
    li a7, 93
    ecall

    .section .rodata
bitstream_path:
    .string "/tmp/counter4.bit"
