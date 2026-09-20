# COMPACTION.MD — Handoff Document for Fresh Context

## Project: rvemu (RISC-V 64-bit QEMU virt Emulator)

**Location**: `/home/no/Desktop/github/riscv_emulator-seulgi-v1/`

---

## CURRENT STATE (as of 2026-09-14)

### ✅ WORKING
- rvemu builds with `./build.sh` (direct GCC, no CMake)
- `--selftest` passes (all checks)
- **Bug fixed**: `src/trap.c:75` — `irq_enabled()` now correctly disables non-delegated interrupts in S-mode
- xv6-riscv ELF kernel boots (`--max-inst 5M` reaches scheduler)
- Cross-compiler: `riscv64-unknown-elf-gcc` installed via apt

### 🔄 IN PROGRESS
- **MTIMECMP CSR support** — Added to `src/csr.c` + `includes/csr.h` (CSR 0x14D)
  - Kernel sets MTIMECMP via `csrw 0x14d` but emulator wasn't handling it
  - CLINT's `mtimecmp` stayed at `~0ULL` → no timer interrupt ever fired
  - Fix: `csr_write_raw()` case `MTIMECMP` now updates `cpu->bus.clint.mtimecmp`
- xv6-riscv with patched `kfree()` (skips memset) — testing if timer interrupt fires

### ❌ NOT DONE
- Timer interrupt not yet verified to fire in xv6
- Linux boot (`--linux -k Image -f rootfs.img`) — no images downloaded
- Syscall ABI for rvemu (write/read/open/close/exit/brk/fstat/lseek)
- newlib/musl cross-compile for bare-metal ELF
- prisc+x.c compilation (needs libc)

---

## KEY FILES MODIFIED

| File | Change |
|------|--------|
| `src/trap.c:75` | Fixed `irq_enabled()`: `if (cpu->priv != PRIV_M) return 0;` |
| `src/csr.c` | Added `MTIMECMP` case (0x14D) → updates `cpu->bus.clint.mtimecmp` |
| `includes/csr.h` | Added `#define MTIMECMP 0x14D` (same as STIMECMP per spec) |
| `/tmp/xv6-riscv/kernel/kalloc.c` | Patched `kfree()` — commented out `memset(pa, 1, PGSIZE)` |

---

## DOCUMENTATION CREATED

| File | Purpose |
|------|---------|
| `TODO.md` | Task list with bugs, current state, near/medium/long-term |
| `ROADMAP.md` | 10-milestone bootstrap plan (NAND → Linux) |
| `INVESTIGATION_REPORT.md` | Technical bug analysis & xv6 boot details |
| `compaction.md` | **This file** — for fresh context handoff |

---

## ARCHITECTURE OVERVIEW

```
┌─────────────────────────────────────────────────────────────┐
│  rvemu (this repo) — QEMU virt RISC-V 64-bit emulator       │
│  • Implements: CPU (RV64GC), CLINT, PLIC, UART, VirtIO,    │
│    MMU (Sv39), SBI                                          │
│  • Entry: main.c → build/main                                │
└─────────────────────────────────────────────────────────────┘
                              ↑
                    Runs standard QEMU virt Linux
                              ↑
┌─────────────────────────────────────────────────────────────┐
│  0.hdl0+prisc-bootstrap/ — BOOTSTRAP ROOT                   │
│  • prisc+x.c (770 lines) — custom VM with custom ops       │
│  • HDLb0 hardware layer (NAND → CPU)                       │
│  • RV-I → RV32I scaling roadmap                            │
└─────────────────────────────────────────────────────────────┘
```

---

## IMMEDIATE NEXT STEPS (Priority Order)

### 1. Verify Timer Interrupt Works
```bash
cd /home/no/Desktop/github/riscv_emulator-seulgi-v1
./build.sh
./build/main -k /tmp/xv6-riscv/kernel/kernel -f /tmp/xv6-riscv/fs.img --max-inst 10000000 --trace-trap
```
Watch for `trap: timer interrupt` or similar. If xv6 reaches shell prompt → timer works.

### 2. Add Syscall ABI to rvemu
**Files**: `src/trap.c` (ecall handler) + new `src/syscall.c`
**Syscalls needed** (Linux RISC-V ABI):
| # | Name | Args |
|---|------|------|
| 64 | write | fd, buf, count |
| 63 | read | fd, buf, count |
| 57 | close | fd |
| 93 | exit | code |
| 214 | brk | addr |
| 80 | fstat | fd, statbuf |
| 62 | lseek | fd, offset, whence |

### 3. Cross-compile newlib or musl
```bash
# Option A: newlib (part of riscv-gnu-toolchain)
# Option B: musl (lightweight)
riscv64-unknown-elf-gcc -print-search-dirs  # Check library paths
```

### 4. Compile prisc+x.c with libc
```bash
# Once newlib/musl available:
riscv64-unknown-elf-gcc -o prisc.rv64 0.hdl0+prisc-bootstrap/prisc+x.c
./build/main -k prisc.rv64 --max-inst 100000
```

### 5. Boot Linux on rvemu
```bash
wget https://github.com/riscv/home/releases/download/v2023.04/Image
wget https://github.com/riscv/home/releases/download/v2023.04/rootfs.img
./build/main --linux -k Image -f rootfs.img
```

---

## PRISC+X.C STATUS

**File**: `0.hdl0+prisc-bootstrap/prisc+x.c` (770 lines, 64KB)

**Dependencies** (requires full POSIX libc):
- Headers: stdio.h, stdlib.h, string.h, unistd.h, dirent.h, sys/wait.h, fcntl.h, time.h, stdarg.h, signal.h, pthread.h
- Functions: popen/pclose, fork/exec/waitpid, asprintf, pthread_create, malloc/free

**Current blocker**: `riscv64-unknown-elf-gcc` package has **no C library** (no newlib).

**Three paths**:
| Path | Effort | Timeline |
|------|--------|----------|
| 1. Boot Linux on rvemu, run as Linux binary | Low | After step 5 |
| 2. Add syscall ABI + cross-compile newlib/musl | Medium | Steps 2-4 |
| 3. Port to bare-metal (UART I/O, no pthreads) | High | Later |

---

## BUGS SUMMARY (from INVESTIGATION_REPORT.md)

| Bug | Location | Status |
|-----|----------|--------|
| 1. mret MPP=0 | `src/cpu.c:973` | **NOT A BUG** — code correct |
| 2. stval for interrupts | `src/trap.c:29` | **NOT A BUG** — code correct |
| 3. irq_enabled in S-mode | `src/trap.c:75` | **FIXED** — was allowing non-delegated interrupts in S-mode |

---

## REPO STRUCTURE

```
/home/no/Desktop/github/
├── riscv_emulator-seulgi-v1/          # rvemu (WORKING)
│   ├── build/main                     # Emulator binary
│   ├── src/                           # C source
│   ├── includes/                      # Headers
│   ├── TODO.md
│   ├── ROADMAP.md
│   ├── INVESTIGATION_REPORT.md
│   └── compaction.md                  # THIS FILE
│
└── 0.hdl0+prisc-bootstrap/            # BOOTSTRAP ROOT
    ├── prisc+x.c                      # Shared VM (770 lines)
    └── 0.🧿️halo.c_spx=HDLb0]☮️]ff6/   # HDLb0 hardware
        └── 0.HDLb0.PIECE+3.9-🧿️/
            ├── nand]z0]FIXD.c         # NAND gate implementation
            └── rv-1-32]a0/            # RV-I → RV32I roadmap
```

---

## COMMANDS REFERENCE

```bash
# Build emulator
cd /home/no/Desktop/github/riscv_emulator-seulgi-v1 && ./build.sh

# Run self-test
./build/main --selftest

# Run xv6-riscv (patched kfree)
./build/main -k /tmp/xv6-riscv/kernel/kernel -f /tmp/xv6-riscv/fs.img --max-inst 10000000

# Trace traps
./build/main -k /tmp/xv6-riscv/kernel/kernel -f /tmp/xv6-riscv/fs.img --max-inst 10000 --trace-trap

# Dump registers
./build/main -k /tmp/xv6-riscv/kernel/kernel -f /tmp/xv6-riscv/fs.img --max-inst 5000 --dump
```

---

## LONG-TERM VISION (from ROADMAP.md)

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

**10 Milestones (M1-M10)** — see ROADMAP.md for details.

---

## KEY CONTACTS / REFERENCES

- **xv6-riscv source**: https://github.com/mit-pdos/xv6-riscv
- **RISC-V Linux images**: https://github.com/riscv/home/releases
- **RISC-V Privileged Spec**: MTIMECMP=0x14D, STIMECMP=0x14D (same CSR)
- **QEMU virt platform**: Standard device addresses (UART 0x10000000, CLINT 0x2000000, PLIC 0xc000000)

---

*Generated 2026-09-14 for fresh context handoff.*