# RVEMU + PRISC+X + HDLb0 BOOTSTRAP ROADMAP

## Executive Summary

**Current State**: rvemu (QEMU virt RISC-V emulator) is working - fixed 1 real bug (`irq_enabled`), built xv6-riscv ELF kernel, selftest passes. Now focusing on the long-term vision: **bootstrapping a self-hosting RISC-V compiler from NAND gates up**.

---

## CURRENT STATE (rvemu)

| Component | Status |
|-----------|--------|
| rvemu (QEMU virt) | ✅ Working, selftest passes |
| Bug fixes | ✅ 1 real bug fixed (`src/trap.c:75` irq_enabled) |
| xv6-riscv ELF | ✅ Built, boots, zeroing free pages (~50M inst) |
| Cross-compiler | ✅ `riscv64-unknown-elf-gcc` installed |
| Alternative emulator | ❌ Removed (franzflasch - custom platform) |

### prisc+x.c Compilation Status
**Location**: `0.hdl0+prisc-bootstrap/prisc+x.c` (770 lines, 64KB)

**Dependencies** (requires full POSIX libc):
- `stdio.h`, `stdlib.h`, `string.h`, `stdint.h`, `ctype.h`, `signal.h`
- `unistd.h`, `dirent.h`, `sys/wait.h`, `fcntl.h`, `time.h`, `stdarg.h`
- Windows: `windows.h`, `direct.h`, `process.h`, `io.h`
- POSIX: `popen`/`pclose`, `asprintf`, `fork`/`exec`/`waitpid`, `pthread`

**Current blocker**: `riscv64-unknown-elf-gcc` package lacks C library (no newlib). Cannot compile prisc+x.c as bare-metal ELF.

**Path to running prisc+x.c on rvemu**:
1. **Short term**: Boot Linux on rvemu (`--linux`), run prisc+x.c as Linux binary
2. **Medium term**: Add syscall ABI to rvemu + link with newlib/musl for bare-metal
3. **Long term**: Port prisc+x.c to bare-metal (replace stdio with UART, remove pthreads/popen)

---

## NEAR-TERM (Next 1-2 weeks)

### 1. Complete xv6-riscv Boot to Shell
```bash
# Option A: Run with enough instructions (slow - byte-by-byte memset)
./build/main -k /tmp/xv6-riscv/kernel/kernel -f /tmp/xv6-riscv/fs.img --max-inst 100000000

# Option B: Patch xv6 kfree() to skip zeroing for fast testing
# Edit /tmp/xv6-riscv/kernel/kalloc.c, remove memset in kfree()
```

### 2. Run Standard Linux on rvemu (QEMU virt)
```bash
# Download prebuilt QEMU virt Linux images
wget https://github.com/riscv/home/releases/download/v2023.04/Image
wget https://github.com/riscv/home/releases/download/v2023.04/rootfs.img

# Or build from source:
# Linux: make qemu_riscv64_defconfig && make -j$(nproc)
# Rootfs: Buildroot with qemu_riscv64_virt_defconfig

./build/main --linux -k Image -f rootfs.img
```

### 3. Verify rvemu Completeness
- [ ] Timer interrupts work (CLINT)
- [ ] Disk I/O works (VirtIO block)
- [ ] Network works (VirtIO net)
- [ ] MMU/Sv39 paging works
- [ ] Multi-hart SMP works

---

## MEDIUM-TERM (1-3 months): PRISC+X INTEGRATION

### Goal: Run prisc+x.c VM on rvemu

**prisc+x.c** is a custom VM/PAL system (770 lines) with:
- 16 registers, 4096 memory cells
- Custom instruction set (ADDI, BEQ, LW, SW, JALR, J, HALT, CUSTOM)
- Custom op dispatch via `+x/` binaries
- Memory-mapped I/O (cli_tape.txt, ram_output_address.txt)

**Integration steps:**
1. Compile prisc+x.c for RISC-V (requires libc - see path below)
2. Run on rvemu as a user program
3. Implement `+x/` custom ops as rvemu-loaded ELF binaries
4. Use rvemu's VirtIO to persist `cli_tape.txt` / `ram_output_address.txt`

### Prerequisites:
- [ ] rvemu supports loading arbitrary ELF binaries (✅ done)
- [ ] rvemu has filesystem access (VirtIO 9p or disk image)
- [ ] Custom op protocol defined (stdin/stdout vs file-based)
- [ ] **rvemu syscall ABI implemented** (write, read, open, close, exit, brk, fstat, lseek)
- [ ] **newlib/musl cross-compiled for RISC-V** (for bare-metal ELF)

### Syscall Implementation Plan for rvemu
**Location**: `src/trap.c` (ecall handler) + new `src/syscall.c`

| Syscall # | Name | Args (a0-a6) | Purpose |
|-----------|------|--------------|---------|
| 64 | write | fd, buf, count | Output to UART/console |
| 63 | read | fd, buf, count | Input from UART |
| 57 | close | fd | Close file |
| 93 | exit | code | Terminate process |
| 214 | brk | addr | Heap management |
| 80 | fstat | fd, statbuf | File status |
| 62 | lseek | fd, offset, whence | File position |

**SBI vs Syscall**: rvemu currently handles SBI ecalls (firmware). User syscalls need separate path:
- Check `a7` for syscall number
- If SBI (a7 >= 0x100000), delegate to `sbi_ecall()`
- Else handle as Linux syscall ABI

---

## LONG-TERM VISION: FULL BOOTSTRAP FROM NAND GATES

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

### Why This Matters

**True bootstrapping**: No dependency on pre-existing compilers, binaries, or hardware. Every layer is built from the layer below, all the way down to NAND gates.

### The Components We Have

| Layer | Location | Status |
|-------|----------|--------|
| **NAND gate** | `0.hdl0+prisc-bootstrap/0.🧿️halo.c_spx=HDLb0]☮️]ff6/0.HDLb0.PIECE+3.9-🧿️/nand]z0]FIXD.c` | ✅ Working |
| **D-Latch / Register** | `playground/d_latch.hlo` (HDLb0) | ✅ Exists |
| **RV-I (1-bit) Blueprint** | `rv-1-32]a0/rv1/blueprint_rv1.txt` | ✅ Designed |
| **Full Adder** | `rv-1-32]a0/rv1/full_adder/` | 🔄 In progress |
| **RV2/RV4/RV8/RV16** | `rv-1-32]a0/rv2, rv4, rv8, rv16/` | 📋 Planned |
| **prisc+x VM** | `0.hdl0+prisc-bootstrap/prisc+x.c` | ✅ 770 lines, working |
| **rvemu (RISC-V emulator)** | `riscv_emulator-seulgi-v1/` | ✅ Running QEMU virt |

### The Bootstrapping Chain

```
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 0: PHYSICAL / LOGICAL FOUNDATION                        │
│  ─────────────────────────────────────────────────────────────  │
│  • NAND gate (C program simulating NAND)                       │
│  • HDLb0: Hardware Description Language (custom)               │
│  • Can simulate: gates, latches, adders, ALUs, CPUs            │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 1: RV-I (1-bit educational CPU)                         │
│  ─────────────────────────────────────────────────────────────  │
│  • 4 registers (r0-r3), 16 RAM, 4-bit PC                       │
│  • ISA: NAND, LOAD, STORE, JUMP                                │
│  • Built entirely from NAND gates via HDLb0                    │
│  • Memory map: RAM[16+] for user, RAM[0-15] reserved           │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 2: ITERATIVE SCALING (RV2 → RV4 → RV8 → RV16 → RV32I)   │
│  ─────────────────────────────────────────────────────────────  │
│  • Each stage: widen datapath, expand registers, scale ALU     │
│  • RV2: 2-bit, add ADD instruction (full adder)                │
│  • RV4: 4-bit, more registers, 4-bit ALU                       │
│  • RV8: 8-bit, bitwise ops (AND/OR/XOR), 16 registers          │
│  • RV16: 16-bit, addressing modes, 32 registers                │
│  • RV32I: Full 32-bit RISC-V base ISA                          │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 3: PRISC+X VM (Software Emulator / Interpreter)         │
│  ─────────────────────────────────────────────────────────────  │
│  • Runs on RV32I (or rvemu)                                    │
│  • Custom ISA + custom ops (+x/ binaries)                      │
│  • Used to write: assembler, linker, C compiler frontend       │
│  • Memory-mapped I/O via files (cli_tape.txt, ram_*.txt)       │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 4: SELF-HOSTING C COMPILER (written in prisc+x)         │
│  ─────────────────────────────────────────────────────────────  │
│  • Lexer/Parser for C subset                                   │
│  • Code generator → RV32I assembly                             │
│  • Assembler → ELF binary                                      │
│  • Linker for multi-file programs                              │
│  • Can compile itself (prisc+x.c → ELF)                        │
└─────────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│  LAYER 5: LINUX KERNEL + USERSPACE                             │
│  ─────────────────────────────────────────────────────────────  │
│  • Compile Linux with bootstrapped compiler                    │
│  • Build rootfs (busybox, init)                                │
│  • Boot on rvemu (or real hardware)                            │
└─────────────────────────────────────────────────────────────────┘
```

### Key Technical Decisions

#### 1. HDLb0 as the Hardware Layer
- Custom HDL (not Verilog/VHDL) - simpler, built for this project
- Simulates gates → chips → CPU
- NAND is the only primitive; everything else composed
- `nand]z0]FIXD.c` shows NAND as C program with file-based I/O

#### 2. rvemu as the Acceleration Layer
- Don't simulate full RV32I in HDLb0 (too slow)
- Use rvemu (native C emulator) to run RV32I at speed
- HDLb0 used for **verification** and **education**, not production execution
- rvemu runs the bootstrapped compiler, Linux, etc.

#### 3. prisc+x as the Software Bridge
- High-level VM with custom ops for I/O, filesystem, process control
- Custom ops (`+x/`) are ELF binaries loaded by rvemu
- Can write compiler tools in prisc+x assembly, then bootstrap to C

#### 4. Forward Compatibility (from roadmap)
- Each RV-N ISA is a subset of RV32I
- Programs written for RV-I run on RV32I
- Memory map consistent: user RAM starts at address 16

### Milestone Definitions

| Milestone | Criteria |
|-----------|----------|
| **M1: NAND Foundation** | NAND gate simulates correctly; D-latch, full adder work in HDLb0 |
| **M2: RV-I Running** | 1-bit CPU executes test program (NAND, LOAD, STORE, JUMP) in HDLb0 |
| **M3: RV4 Running** | 4-bit CPU runs simple program; adder works; control unit cycles |
| **M4: RV32I in HDLb0** | Full 32-bit CPU design complete in HDLb0 (slow, for verification) |
| **M5: rvemu RV32I** | rvemu passes RISC-V compliance tests; runs Linux |
| **M6: prisc+x on rvemu** | prisc+x.c compiles to RV32I, runs on rvemu, custom ops work |
| **M7: C Compiler in prisc+x** | prisc+x can compile C → RV32I assembly → ELF |
| **M8: Self-Hosting** | Compiler compiles itself (prisc+x.c → ELF → runs) |
| **M9: Linux Boot** | Bootstrapped compiler builds Linux; boots on rvemu |
| **M10: Full Circle** | Linux on rvemu runs the compiler that built it |

### Dependencies & Risks

| Risk | Mitigation |
|------|------------|
| HDLb0 simulation too slow for RV32I | Use rvemu for execution; HDLb0 only for verification |
| Custom op interface design | Start simple: file-based I/O, evolve to syscalls |
| Compiler complexity | Start with tiny C subset (no pointers, no structs) |
| Time scope | Incremental milestones; each layer validates the next |

### Repository Structure (Proposed)

```
/home/no/Desktop/github/
├── riscv_emulator-seulgi-v1/          # rvemu (QEMU virt) - WORKING
│   ├── build/main                     # RISC-V emulator binary
│   ├── src/                           # C source
│   └── INVESTIGATION_REPORT.md        # This investigation
│
├── 0.hdl0+prisc-bootstrap/            # BOOTSTRAP ROOT
│   ├── prisc+x.c                      # Shared VM (770 lines)
│   └── 0.🧿️halo.c_spx=HDLb0]☮️]ff6/   # HDLb0 hardware layer
│       └── 0.HDLb0.PIECE+3.9-🧿️/     # Current piece
│           ├── nand]z0]FIXD.c         # NAND gate implementation
│           ├── nand_only.txt          # NAND test vectors
│           └── rv-1-32]a0/            # RV-I through RV32I
│               ├── rvi-2-rv5_roadmap.txt  # This roadmap!
│               ├── rv1/               # 1-bit CPU (blueprint)
│               ├── rv2/               # 2-bit CPU
│               ├── rv4/               # 4-bit CPU
│               ├── rv8/               # 8-bit CPU
│               ├── rv16/              # 16-bit CPU
│               └── rv32/              # 32-bit CPU (final)
│
└── work/NNEST-12.00/                  # TPMOS / other projects
```

---

## IMMEDIATE NEXT ACTIONS

### This Week
1. [ ] Patch xv6 kfree() to skip zeroing → boot to shell in seconds
2. [ ] Download/build QEMU virt Linux Image + rootfs → run on rvemu
3. [ ] **Add syscall handling to rvemu** (trap.c + syscall.c for write/read/exit/brk)
4. [ ] **Cross-compile newlib or musl** for RISC-V bare-metal linking

### This Month
5. [ ] Compile prisc+x.c with newlib → run bare-metal on rvemu
6. [ ] Design custom op ABI for prisc+x on rvemu (file I/O, exit, spawn)
7. [ ] Implement 1 custom op (e.g., `read_tape` → reads cli_tape.txt)
8. [ ] Write assembler in prisc+x assembly

### This Quarter
9. [ ] Complete RV-I in HDLb0 (verify NAND → D-latch → full adder → CPU)
10. [ ] Scale to RV4 in HDLb0
11. [ ] Begin C compiler frontend in prisc+x

---

## PHILOSOPHY

> "We are not building an emulator. We are building a **civilization** that starts from a NAND gate and ends at a self-hosting Linux system. Every layer is its own proof."

The rvemu is the **trust anchor** - it lets us run RV32I at speed while we build the lower layers in HDLb0 for verification and education. The prisc+x VM is the **software bridge** - high enough to write compilers, low enough to map to RV32I.

**End goal**: A single repository where `make bootstrap` starts from `nand.c` and produces a running Linux system with a compiler that compiled itself.

---

*Generated from investigation of rvemu, prisc+x.c, and HDLb0 roadmap documents.*