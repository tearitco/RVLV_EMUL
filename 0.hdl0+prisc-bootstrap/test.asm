.global main
.align 2
main:
  addi sp, sp, -32
  sw ra, 28(sp)
  li x10, 5
  sw x10, -4(sp)
  li x10, 3
  sw x10, -8(sp)
  lw x10, -4(sp)
  lw x11, -8(sp)
  add x10, x10, x11
  sw x10, -20(sp)
  li x13, 268435456
  li x11, 13
  li x12, 10
  lw x10, -20(sp)
  mul x14, x10, x11
  srli x14, x14, 7
  mul x15, x14, x12
  sub x10, x10, x15
  beq x14, x0, skip_tens
  addi x14, x14, 48
  sw x14, 0(x13)
skip_tens:
  addi x10, x10, 48
  sw x10, 0(x13)
  lw x10, -4(sp)
  lw x11, -8(sp)
  sub x10, x10, x11
  sw x10, -20(sp)
  li x13, 268435456
  li x11, 13
  li x12, 10
  lw x10, -20(sp)
  mul x14, x10, x11
  srli x14, x14, 7
  mul x15, x14, x12
  sub x10, x10, x15
  beq x14, x0, skip_tens
  addi x14, x14, 48
  sw x14, 0(x13)
skip_tens:
  addi x10, x10, 48
  sw x10, 0(x13)
  lw x10, -4(sp)
  lw x11, -8(sp)
  mul x10, x10, x11
  sw x10, -20(sp)
  li x13, 268435456
  li x11, 13
  li x12, 10
  lw x10, -20(sp)
  mul x14, x10, x11
  srli x14, x14, 7
  mul x15, x14, x12
  sub x10, x10, x15
  beq x14, x0, skip_tens
  addi x14, x14, 48
  sw x14, 0(x13)
skip_tens:
  addi x10, x10, 48
  sw x10, 0(x13)
  li x10, 1
  li x17, 93
  ecall
  lw ra, 28(sp)
  addi sp, sp, 32
  ret
