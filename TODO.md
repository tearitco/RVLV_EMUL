# TODO — rvemu investigation & fixes

## Done
- [x] Explored codebase (rvemu, C-based RV64GC QEMU virt emulator)
- [x] Created `build.sh` (direct GCC, no CMake/Ninja)
  - Added compat flags for GCC 11: `-Dnullptr=0 -Wno-error=format`
  - Build succeeds; `--selftest` passes (all checks passed)
- [x] Investigated xv6 — boots ("xv6 kernel is booting") then **hangs**
- [x] Disassembled xv6 kernel with Capstone (no RISC-V cross-toolchain / no sudo)
- [x] Root-caused two bugs preventing xv6 from running (see below)
- [x] Installed RISC-V cross-compiler (`riscv64-unknown-elf-gcc` via apt)
- [x] Built modern xv6-riscv ELF kernel + fs.img
- [x] Removed alternative emulator (franzflasch - custom platform, incompatible)

## Bugs found (confirmed via disassembly + tracing)

### Bug 1 — `mret` MPP=0 (U-mode) handling — `src/cpu.c:973`
```c
cpu->priv = (uint8_t)((m >> 11) & 3);
if (cpu->priv == 2)
    cpu->priv = PRIV_M;
```
**Analysis**: This logic is CORRECT for all valid MPP values:
- MPP=0 (U) → 0 = PRIV_U ✓
- MPP=1 (S) → 1 = PRIV_S ✓
- MPP=2 (reserved) → mapped to PRIV_M (reasonable)
- MPP=3 (M) → 3 = PRIV_M ✓

The suggested fix `(m >> 11 & 3) == 0 ? PRIV_U : PRIV_S` would BREAK M-mode returns.

### Bug 2 — `stval` for interrupts — `src/trap.c:29`
```c
cpu->csr[STVAL] = t.interrupt ? 0 : t.tval;
```
**Analysis**: This is CORRECT. The RISC-V spec requires stval/mtval = 0 for ALL interrupts (timer, software, external), and exception-specific value for exceptions. The condition `t.interrupt ? 0 : t.tval` handles this properly. No fix needed.

### Bug 3 — `irq_enabled` allows non-delegated interrupts in S-mode — `src/trap.c:75` — **FIXED**
```c
if (cpu->priv < PRIV_M)
    return 1;
```
**Analysis**: This was WRONG. Non-delegated interrupts (MTIP timer, MSIP software, MEIP external) should ONLY be taken in M-mode. In S-mode, they must be disabled (trap to M-mode). The condition `cpu->priv < PRIV_M` returns 1 (enabled) for both S-mode (1) and U-mode (0), but should only enable for M-mode (3).

**Fix applied**: Changed to `if (cpu->priv != PRIV_M) return 0;`

This was causing timer interrupts (MTIP, not delegated) to fire in S-mode where `stvec=0`, causing trap to address 0 loop.

## Current state — xv6-riscv ELF kernel runs but zeroes free pages during boot

xv6 prints "xv6 kernel is booting" then enters `memset` loop at `0x80000be8-0x80000bee` that zeroes free memory pages one byte at a time.

**What's happening**:
- The kernel's `kfree()` initializes each free page by zeroing it during allocator setup
- Each 4KB page takes ~12,000 instructions (byte-by-byte sb + addi + bne loop)
- With ~4000 free pages, that's ~50M+ instructions just for zeroing
- At 5M instructions: zeroing pages at `0x800c0000` to `0x800c1000`
- This is expected behavior for cold boot - not an emulator bug

**Register state in memset loop**:
- `a0` = destination (page start, increments 4KB per kfree call)
- `a1` = 0 (fill value)
- `a2` = 4096 (size per call = 4KB)
- `a5` = current byte pointer (increments)
- `a4` = end address (a0 + 4096)
- `ra` = `0x8000099c` (return to kfree)

**Emulator validation**:
- `--selftest` passes (all checks)
- Simple test programs run correctly
- xv6 ELF kernel loads and executes correctly
- The "hang" is just slow BSS/free page zeroing during first boot

## Near-term tasks (1-2 weeks)
- [ ] Patch xv6 `kfree()` to skip zeroing for fast boot testing
- [ ] Run xv6-riscv to shell prompt
- [ ] Download/build QEMU virt Linux `Image` + `rootfs.img`
- [ ] Boot Linux on rvemu: `./build/main --linux -k Image -f rootfs.img`
- [ ] **Add syscall ABI to rvemu** (trap.c + syscall.c: write, read, open, close, exit, brk, fstat, lseek)
- [ ] **Cross-compile newlib or musl** for RISC-V bare-metal linking
- [ ] Compile `prisc+x.c` with newlib → run bare-metal on rvemu
- [ ] Design custom op ABI for prisc+x on rvemu (file I/O, spawn)

## Medium-term tasks (1-3 months)
- [ ] Implement prisc+x custom ops as rvemu-loaded ELF binaries
- [ ] Write assembler in prisc+x assembly
- [ ] Write C compiler frontend in prisc+x
- [ ] Verify rvemu completeness: CLINT, PLIC, VirtIO disk/net, MMU, SMP

## prisc+x.c Compilation Blockers (NEW)
**File**: `0.hdl0+prisc-bootstrap/prisc+x.c` (770 lines, 64KB)

**Requires POSIX libc** (not available in `riscv64-unknown-elf-gcc` package):
- Headers: stdio.h, stdlib.h, string.h, unistd.h, dirent.h, sys/wait.h, fcntl.h, time.h, stdarg.h, signal.h, pthread.h
- Functions: popen/pclose, fork/exec/waitpid, asprintf, pthread_create, malloc/free

**Path to running on rvemu**:
| Approach | Effort | When |
|----------|--------|------|
| 1. Boot Linux on rvemu, run as Linux binary | Low | This week |
| 2. Add syscall ABI + link newlib/musl | Medium | This month |
| 3. Port to bare-metal (UART I/O, no pthreads) | High | Later |

## Long-term vision: FULL BOOTSTRAP FROM NAND GATES

See **ROADMAP.md** for complete 10-milestone plan.

### The Vision
```
NAND gates (HDLb0)
    ↓
1-bit RV-I CPU (RV1)
    ↓
2-bit RV2 → 4-bit RV4 → 8-bit RV8 → 16-bit RV16
    ↓
32-bit RV32I CPU (full RISC-V)
    ↓
Self-hosting RISC-V C Compiler (prisc+x → C)
    ↓
Linux Kernel
    ↓
Bootstrapped OS
```

### Components We Have
| Layer | Location | Status |
|-------|----------|--------|
| **NAND gate** | `0.hdl0+prisc-bootstrap/.../nand]z0]FIXD.c` | ✅ Working |
| **D-Latch / Register** | `playground/d_latch.hlo` (HDLb0) | ✅ Exists |
| **RV-I (1-bit) Blueprint** | `rv-1-32]a0/rv1/blueprint_rv1.txt` | ✅ Designed |
| **Full Adder** | `rv-1-32]a0/rv1/full_adder/` | 🔄 In progress |
| **RV2/RV4/RV8/RV16** | `rv-1-32]a0/rv2, rv4, rv8, rv16/` | 📋 Planned |
| **prisc+x VM** | `0.hdl0+prisc-bootstrap/prisc+x.c` | ✅ 770 lines |
| **rvemu (RISC-V emulator)** | `riscv_emulator-seulgi-v1/` | ✅ QEMU virt |

### Milestones (M1-M10)
| Milestone | Target |
|-----------|--------|
| **M1** | NAND Foundation: gates, latches, adders in HDLb0 |
| **M2** | RV-I Running: 1-bit CPU executes test program |
| **M3** | RV4 Running: 4-bit CPU with adder, control unit |
| **M4** | RV32I in HDLb0: Full 32-bit design (verification only) |
| **M5** | rvemu RV32I: Passes compliance, runs Linux |
| **M6** | prisc+x on rvemu: VM runs, custom ops work |
| **M7** | C Compiler in prisc+x: Compiles C → RV32I → ELF |
| **M8** | Self-Hosting: Compiler compiles itself |
| **M9** | Linux Boot: Bootstrapped compiler builds Linux |
| **M10** | Full Circle: Linux runs the compiler that built it |

### Key Architectural Decisions
1. **HDLb0** = Hardware layer (NAND → CPU), for verification/education
2. **rvemu** = Execution layer (QEMU virt), for speed/production
3. **prisc+x** = Software bridge (VM + custom ops), for compiler tools
4. **Forward compatibility**: Each RV-N ISA ⊂ RV32I, user RAM at addr 16+

### Repository Structure
```
/home/no/Desktop/github/
├── riscv_emulator-seulgi-v1/          # rvemu (QEMU virt) - WORKING
├── 0.hdl0+prisc-bootstrap/            # BOOTSTRAP ROOT
│   ├── prisc+x.c                      # Shared VM
│   └── 0.🧿️halo.c_spx=HDLb0]☮️]ff6/   # HDLb0 hardware
│       └── 0.HDLb0.PIECE+3.9-🧿️/
│           ├── nand]z0]FIXD.c         # NAND gate
│           └── rv-1-32]a0/            # RV-I → RV32I
```

---

*Full technical report: INVESTIGATION_REPORT.md*
*Complete roadmap: ROADMAP.md*