.section .text
    .globl _start
_start:
    lui a0, %hi(bitstream_path)
    addi a0, a0, %lo(bitstream_path)
    li a7, 10
    ecall

    # Write operand A = 5 (0101) to pin 0
    # Pin 0: A bit 0 = 1, Pin 1: A bit 1 = 0, Pin 2: A bit 2 = 1, Pin 3: A bit 3 = 0
    # Actually, we write byte values, not individual bits
    # Pin 0 carries A[0:3] in a single byte? No - each pin is a separate byte
    # 
    # Wait - looking at the FPGA design, each PIN maps to a single wire
    # The ALU takes 4-bit inputs on pins 0-3 (one pin per bit)
    # But each pin carries a full uint8_t value
    # 
    # The LUT for bitwise0 is:
    #   index = pin0_in (a_bit0) | (pin4 << 1) | (fn0 << 2) | (fn1 << 3)
    # With pin0_in=5, pin4=3, fn0=0, fn1=0
    # index = 5 | (3<<1) | 0 | 0 = 5 | 6 = 7
    # tt[7] for bitwise0 = 1 (for AND: a&b, but with multi-bit values this doesn't work)
    
    # Actually the FPGA processes byte values through LUTs
    # Each LUT takes 3-4 byte-wide inputs and produces a byte-wide output
    # The truth table is indexed by: in0 | (in1 << 1) | (in2 << 2) | (in3 << 3)  (masked to 4 bits)
    # So with pin0_in=5, pin4=3: index = 5 | (3<<1) = 5|6 = 7 (masked to 4 bits = 7)
    # tt[7] = 1 for AND operation
    
    # But we want a=5, b=3, op=AND → result should be 5&3 = 1
    # Pin 0 = A bit 0 = 1 (just write 1 to pin 0)
    # Pin 1 = A bit 1 = 0 (write 0)
    # Pin 2 = A bit 2 = 1 (write 1)
    # Pin 3 = A bit 3 = 0 (write 0)
    # Pin 4 = B bit 0 = 1 (write 1)
    # Pin 5 = B bit 1 = 1 (write 1)
    # Pin 6 = B bit 2 = 0 (write 0)
    # Pin 7 = B bit 3 = 0 (write 0)
    # Then A = 0101 = 5, B = 0011 = 3
    # AND: 5 & 3 = 0001 = 1
    
    # Write A bits to pins 0-3
    li t0, 0x40000000
    li t1, 1
    sb t1, 0(t0)      # A[0] = 1

    li t0, 0x40000100
    li t1, 0
    sb t1, 0(t0)      # A[1] = 0

    li t0, 0x40000200
    li t1, 1
    sb t1, 0(t0)      # A[2] = 1

    li t0, 0x40000300
    li t1, 0
    sb t1, 0(t0)      # A[3] = 0

    # Write B bits to pins 4-7
    li t0, 0x40000400
    li t1, 1
    sb t1, 0(t0)      # B[0] = 1

    li t0, 0x40000500
    li t1, 1
    sb t1, 0(t0)      # B[1] = 1

    li t0, 0x40000600
    li t1, 0
    sb t1, 0(t0)      # B[2] = 0

    li t0, 0x40000700
    li t1, 0
    sb t1, 0(t0)      # B[3] = 0

    # fn = 000 (AND), so pins 8,9,10 should already be 0

    # Read result from pins 11-14
    # Pin 11 = result[0]
    li t0, 0x40000B00
    lbu t2, 0(t0)
    li t3, 1
    bne t2, t3, fail

    # All checks passed
    li a0, 0
    li a7, 93
    ecall

fail:
    li a0, 1
    li a7, 93
    ecall

    .section .rodata
bitstream_path:
    .string "/tmp/alu4.bit"
