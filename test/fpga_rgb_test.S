# Simple RGB framebuffer test
# Draws a colorful gradient: R=x*4, G=y*4, B=(x+y)*2
.section .text
.globl _start
_start:
    li a2, 0x40040000       # FB base address
    li a3, 0                # pixel index

loop:
    # x = index & 0x3F
    andi t2, a3, 0x3F       # t2 = x
    # y = (index >> 6) & 0x3F
    srli t3, a3, 6          # t3 = y >> 6
    andi t3, t3, 0x3F       # t3 = y

    # R = x * 4
    slli s2, t2, 2          # s2 = R
    # G = y * 4
    slli s3, t3, 2          # s3 = G
    # B = (x + y) * 2
    add s4, t2, t3          # s4 = x + y
    slli s4, s4, 1          # s4 = B

    # Pack: 0x00RRGGBB
    slli s5, s2, 16         # R << 16
    slli t4, s3, 8          # G << 8
    or s5, s5, t4           # R | G
    or s5, s5, s4           # R | G | B

    # Write pixel and advance address
    sw s5, 0(a2)
    addi a2, a2, 4          # advance to next pixel address

    # Next pixel
    addi a3, a3, 1
    li t2, 6400
    blt a3, t2, loop

    # Success
    li a0, 0
    li a7, 93
    ecall
