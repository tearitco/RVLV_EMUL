.section .text
    .globl _start
_start:
    # Load rv16_prog bitstream
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Read pin0 (result) multiple times to step through FPGA cycles
    # The rv16_prog core computes x1=3, x2=6, x3=x1+x2=9, outputs x3=9
    li t0, 0x40000000    # FPGA pin 0 address
    li t3, 0x08          # Expected result: 3+5=8
    li t4, 20            # Max cycles to try

check_loop:
    lw t2, 0(t0)         # Read pin0 (also advances FPGA clock)
    andi t2, t2, 0x0F    # Mask to 4 bits
    beq t2, t3, success  # If result == 9, success!
    addi t4, t4, -1      # Decrement counter
    bnez t4, check_loop  # Keep trying

    # Failed - result never became 9
    li a0, 1
    li a7, 93
    ecall

success:
    li a0, 0
    li a7, 93
    ecall

    .section .rodata
bitstream_path:
    .string "/tmp/rv16_prog.bit"
