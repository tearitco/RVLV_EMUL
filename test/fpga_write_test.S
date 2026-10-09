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
    
    # Read output pin 0 (0x40000200) - should be 0x05
    lui t0, %hi(0x40000200)
    lw t2, %lo(0x40000200)(t0)
    andi t2, t2, 0x0F
    li t3, 0x05
    bne t2, t3, fail
    
    # Read output pin 1 (0x40000300) - should be 0x0A
    lui t0, %hi(0x40000300)
    lw t2, %lo(0x40000300)(t0)
    andi t2, t2, 0x0F
    li t3, 0x0A
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
    .string "/tmp/gpio_echo.bit"