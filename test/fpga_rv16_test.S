.section .text
    .globl _start
_start:
    # Load rv16 bitstream
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Read pin0 (result) multiple times to step through FPGA cycles
    # The rv16 core runs: load x1=5, load x2=3, add, output x3=8
    # After ~6 clock cycles (each read advances the clock), pin0 should be 8
    li t0, 0x40000000    # FPGA pin 0 address
    li t3, 0x08          # Expected result
    li t4, 20            # Max cycles to try
    li t5, 0             # Found flag

check_loop:
    lw t2, 0(t0)         # Read pin0 (also advances FPGA clock)
    andi t2, t2, 0x0F    # Mask to 4 bits
    beq t2, t3, success  # If result == 8, success!
    addi t4, t4, -1      # Decrement counter
    bnez t4, check_loop  # Keep trying

    # Failed - result never became 8
    li a0, 1
    li a7, 93
    ecall

success:
    li a0, 0
    li a7, 93
    ecall

    .section .rodata
bitstream_path:
    .string "/tmp/rv16_core.bit"
