# RISC-V Assembler in .pal Syntax - Project Roadmap

## Objective
Implement a RISC-V assembler in prisc+x `.pal` syntax, enabling self-hosting instruction encoding using R-type operations. This allows the prisc+x VM to generate RISC-V machine code for execution on real RISC-V hardware or simulators (rvemu, QEMU).

## Status Overview

### Phase 1: VM Core Extensions (Complete)
- [x] Add 7 R-type opcodes to `OpBase` enum (`OP_ADD`, `OP_SUB`, `OP_ANDI`, `OP_ORI`, `OP_XORI`, `OP_SLLI`, `OP_SRLI`)
- [x] Add 8 additional R-type opcodes (`OP_SLL`, `OP_SLT`, `OP_SLTU`, `OP_XOR`, `OP_OR`, `OP_SRL`, `OP_SRA`)
- [x] Add parser support for all new instructions (both `.pal` and `r%d` syntax)
- [x] Add executor implementations in both `prisc+x.c` and `prisc_bare.c`
- [x] Add `SYS_WRITE_MEM` (8) syscall to `prisc+x.c` - writes VM `mem[]` to host file descriptor
- [x] Add `SYS_EXIT` (9) syscall to `prisc+x.c` - halts VM
- [x] Add `vm_exit()` wrapper to `prisc_bare.c`
- [x] Pre-initialize `ecall_fds[0..2]` with stdin/stdout/stderr
- [x] Fix `exec_ecall` bug (undefined `running` variable)

### Phase 2: Assembler Implementation (Complete)
- [x] Create `asm.pal` - demonstrates encoding of all RISC-V instruction formats:
  - R-type: add, sub (with funct7)
  - I-type: addi, andi (with funct3)
  - S-type: sw (with imm split)
  - B-type: beq
  - U-type: lui
- [x] Create `asm_test.pal` - end-to-end test encoding a complete RISC-V program
- [x] Verify: `asm_test.pal` output is **byte-identical** to GCC-compiled RISC-V binary

### Phase 3: Testing & Verification (Complete)
- [x] Test all R-type instructions with `test_rtype.pal`
- [x] Verify `test.pal` now outputs "Hello from prisc+x!" (was silent before fd fix)
- [x] Verify `test_linux.pal` still works correctly
- [x] Compile `prisc_bare.c` with RISC-V cross-compiler (clean build)

### Phase 4: Documentation & Release (In Progress)
- [ ] Create `ROADMAP.md` (this file)
- [ ] Commit all changes to GitHub

## Instruction Coverage

### R-type (register-register)
| Instruction | Opcode | Funct3 | Funct7 | Status |
|---|---|---|---|---|
| add  | 0x33 | 0x0 | 0x00 | Implemented |
| sub  | 0x33 | 0x0 | 0x20 | Implemented |
| sll  | 0x33 | 0x1 | 0x00 | Implemented |
| slt  | 0x33 | 0x2 | 0x00 | Implemented |
| sltu | 0x33 | 0x3 | 0x00 | Implemented |
| xor  | 0x33 | 0x4 | 0x00 | Implemented |
| srl  | 0x33 | 0x5 | 0x00 | Implemented |
| or   | 0x33 | 0x6 | 0x00 | Implemented |
| sra  | 0x33 | 0x5 | 0x20 | Implemented |

### I-type (register-immediate)
| Instruction | Opcode | Funct3 | Status |
|---|---|---|---|
| addi  | 0x13 | 0x0 | Implemented |
| andi  | 0x13 | 0x7 | Implemented |
| ori   | 0x13 | 0x6 | Implemented |
| xori  | 0x13 | 0x4 | Implemented |
| slli  | 0x13 | 0x1 | Implemented |
| srli  | 0x13 | 0x5 | Implemented |
| lw    | 0x03 | 0x2 | Implemented |
| jalr  | 0x67 | 0x0 | Implemented |

### B-type (branches)
| Instruction | Opcode | Funct3 | Status |
|---|---|---|---|
| beq | 0x63 | 0x0 | Implemented |
| bne | 0x63 | 0x1 | Implemented |

### Other
| Instruction | Opcode | Status |
|---|---|---|
| j     | 0x6F | Implemented |
| sw    | 0x27 | Implemented |
| li    | 0x13 | Implemented (pseudo) |
| halt  | -    | Implemented |
| ecall | -    | Implemented |

## Future Enhancements
- Add `slti`, `sltiu`, `srai` I-type instructions
- Add `lb`, `lh`, `lbu`, `lhu` load variants
- Add `sb`, `sh` store variants
- Add `bne`, `blt`, `bge`, `bltu`, `bgeu` branch variants
- Add `jal` and `auipc` (U/J-type)
- Add M-extension (multiply/divide)
- Add CSR instructions for full prisc_bare.c compatibility
