  addi sp, sp, -32
  sw ra, 28(sp)
  li x10, 5
  sw x10, -4(sp)
  li x10, 0
  sw x10, -12(sp)
loop_start:
  lw x10, -12(sp)
  li x11, 3
  slt x12, x10, x11
  bne x12, x0, loop_body
  j loop_end
loop_body:
  lw x10, -12(sp)
  addi x10, x10, 48
  li x11, 268435456
  sw x10, 0(x11)
  lw x10, -12(sp)
  li x11, 1
  add x10, x10, x11
  sw x10, -12(sp)
  j loop_start
loop_end:
  li x10, 10
  sw x10, -4(sp)
  lw x10, -4(sp)
  li x11, 5
  slt x12, x11, x10
  beq x12, x0, if_else
  li x10, 1
  addi x10, x10, 48
  li x11, 268435456
  sw x10, 0(x11)
  j if_end
if_else:
  li x10, 0
  addi x10, x10, 48
  li x11, 268435456
  sw x10, 0(x11)
if_end:
  li x10, 1
  li x17, 93
  ecall
  lw ra, 28(sp)
  addi sp, sp, 32
  ret
