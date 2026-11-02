# Test for 74HC251 8:1 Mux
# Pin layout (from PNR):
#   Pin 0: d0_in (input)
#   Pin 1: d1_in (input)
#   ...
#   Pin 7: d7_in (input)
#   Pin 8: s0_in (input)
#   Pin 9: s1_in (input)
#   Pin 10: s2_in (input)
#   Pin 11: g_n_in (input)
#   Pin 12: y (output)
#   Pin 13: y_bar (output)
.section .text
.globl _start
_start:
    # Load bitstream
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Set g_n=0 (enable), all selects=0 -> select d0
    # Pin 11 (g_n) at 0x40000B00
    li t0, 0x40000B00
    li t1, 0
    sw t1, 0(t0)           # g_n=0 (enabled)

    # Set d0=1, rest=0
    li t0, 0x40000000      # pin 0 (d0_in)
    li t1, 1
    sw t1, 0(t0)           # d0=1

    # Read y (pin 12 at 0x40000C00) - should be 1
    li t0, 0x40000C00
    lw t2, 0(t0)
    li t3, 1
    bne t2, t3, fail       # y should be 1

    # Read y_bar (pin 13 at 0x40000D00) - should be 0
    li t0, 0x40000D00
    lw t2, 0(t0)
    bne t2, zero, fail     # y_bar should be 0

    # Now select d3 (s0=1, s1=1, s2=0): index = 3
    # Pin 8 (s0) at 0x40000800
    li t0, 0x40000800
    li t1, 1
    sw t1, 0(t0)           # s0=1

    # Pin 9 (s1) at 0x40000900
    li t0, 0x40000900
    sw t1, 0(t0)           # s1=1

    # Set d3=1
    li t0, 0x40000300      # pin 3 (d3_in)
    sw t1, 0(t0)           # d3=1

    # Read y - should be 1 (d3 selected)
    li t0, 0x40000C00
    lw t2, 0(t0)
    bne t2, t3, fail       # y should be 1

    # Set d3=0, check y=0
    li t0, 0x40000300
    sw zero, 0(t0)         # d3=0
    li t0, 0x40000C00
    lw t2, 0(t0)
    bne t2, zero, fail     # y should be 0

    # Set g_n=1 (disable), check y=0
    li t0, 0x40000B00
    li t1, 1
    sw t1, 0(t0)           # g_n=1 (disabled)
    li t0, 0x40000C00
    lw t2, 0(t0)
    bne t2, zero, fail     # y should be 0 when disabled

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
    .string "/tmp/mux_74hc251.bit"
