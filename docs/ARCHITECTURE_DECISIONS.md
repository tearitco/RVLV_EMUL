# RVLV_EMUL Architecture Decisions

**Date:** 2026-09-21  
**Session:** Bootstrap planning discussion

---

## Core Philosophy: Bootstrap-First / In-House Stack

**We build our own stack. External dependencies are scaffolding, not foundation.**

| Layer | In-House | External (Scaffolding) |
|-------|----------|------------------------|
| Execution | **rvemu** (our RISC-V emulator) | QEMU (verification only) |
| VM / Bridge | **prisc+x** (C → bare-metal) | — |
| Assembler | **prisc+x assembly (.pal)** | — |
| C Compiler | **prisc+x assembly (.pal)** | — |
| Linker | **prisc+x assembly (.pal)** | — |
| Libc | **minimal inline (bare-metal)** | musl (cross-compile host only) |
| Kernel | **bootstrapped compiler builds it** | Buildroot Linux (test harness) |
| Hardware | **HDLb0** (NAND → RV32I) | — |

**Rule:** Every external tool must have an in-house replacement planned. Buildroot/musl/QEMU are temporary bootstrapping aids — they exit the stack once the in-house layer works.

---

## Core Decisions (Locked In)

### 1. prisc+x VM = Trusted Base
- prisc+x interprets `.pal` assembly files directly — no separate bytecode assembler needed
- The VM (770 lines C) is the foundation; everything above is written in `.pal`
- **Self-hosting path**: prisc+x VM (C) → Compiler (.pal) → prisc+x VM (ELF) → Compiler (ELF)

### 2. RISC-V Assembler Written in prisc+x (`.pal`)
- Not in C, not in Rust — in prisc+x assembly (`.pal` files)
- Runs on prisc+x VM (C version first, then bare-metal)
- Output: RV32I ELF binaries
- Enables full bootstrap: assembler assembles compiler, compiler compiles assembler

### 3. Custom Ops (`+x/`) = rvemu-Loaded ELF Binaries
- On Linux: shell out via `fork/exec` (current prisc+x.c)
- On rvemu bare-metal: **syscall to rvemu** to load/execute ELF
- Custom op protocol: `spawn_elf(path, args...)` syscall + file-based I/O (VirtIO 9p/disk)
- No shell, no `popen`, no `pthread`

### 4. Bare-Metal prisc+x = Clean Rewrite (`prisc+x_bare.c`)
**Why not incremental `#ifdef BARE_METAL`:**
- `popen`/`fork`/`pthread` semantics don't map to bare-metal → fake stubs, divergent behavior
- 770 lines → 1500+ with `#ifdef` soup → un-auditable, not a "trusted base"
- Trusted base must be **small, provably correct, portable**

**Clean rewrite targets ~300-400 lines:**
- VM core: 16 regs, 4K memory, instruction loop (ADDI, BEQ, BNE, LW, SW, JALR, J, HALT)
- String ops: SLIT, SCPY, SAPPEND, SFMT, SREAD, SSPLIT, SFIND, SLEN, SFOPEN, SFAPPEND, SWRITE, SFCLOSE, SBEQ, SBNE, STRIM, SATOI
- ECALL syscalls: open, close, read, write, exit, brk, spawn_elf
- Custom op dispatch: syscall to rvemu (not shell)
- No POSIX deps: no stdio, no dirent, no pthread, no asprintf, no popen

### 5. No Stage0 Hex Bootstrap
- prisc+x VM (C) is the trusted base
- Stage0 (hex → assembler → C) unnecessary — we have a working VM
- Bootstrap starts at M6: prisc+x on rvemu

---

## Bootstrap Milestones (Revised)

| Milestone | Target | How |
|-----------|--------|-----|
| **M5** | rvemu runs Linux | Buildroot + syscall ABI (THIS WEEK) |
| **M6** | prisc+x_bare on rvemu | musl cross-compile + bare-metal ELF |
| **M7** | C Compiler in prisc+x | Lexer/parser/codegen in `.pal` files |
| **M8** | Self-hosting | Compiler compiles prisc+x_bare.c → ELF |
| **M9** | Linux from bootstrap | Bootstrapped compiler builds Linux |
| **M10** | Full circle | Linux runs compiler that built it |

---

## Immediate Implementation Order

### Week 1: Syscall ABI + musl + prisc+x_bare
1. **Syscall ABI in rvemu** (`src/syscall.c` + `src/trap.c`)
   - Linux syscall numbers: write=64, read=63, exit=93, brk=214, open=1024, close=57, fstat=80, lseek=62
   - SBI detection: `a7 >= 0x100000` → SBI, else → Linux syscall
2. **musl cross-compile** for `riscv64-unknown-elf`
3. **prisc+x_bare.c** (clean rewrite, ~300-400 lines)
4. **Test**: Compile prisc+x_bare.c with musl → run bare-metal on rvemu

### Week 2: Custom Op ABI + Assembler in prisc+x
5. **Custom op syscall**: `spawn_elf(path, argv, envp)` + file descriptor passing
6. **RISC-V assembler in `.pal`**: Parse RV32I asm → emit ELF
7. **Test**: Assembler assembles simple program → runs on rvemu

### Month 1: C Compiler in prisc+x
8. **Lexer/parser** in `.pal` (C subset: int, char, pointers, functions, control flow)
9. **Codegen** → RV32I assembly (text) → pipe to assembler
10. **Linker** in `.pal` (minimal: resolve symbols, layout sections)

### Month 2-3: Self-Hosting
11. **Compiler compiles prisc+x_bare.c** → ELF → runs on rvemu
12. **Verify**: Compiled VM runs `.pal` files identically to C version

---

## File Locations

| Component | Location |
|-----------|----------|
| rvemu syscall ABI | `RVLV_EMUL/src/syscall.c` (new), `RVLV_EMUL/src/trap.c` (modify) |
| musl build | `RVLV_EMUL/musl/` (build dir) |
| prisc+x_bare.c | `RVLV_EMUL/src/prisc_bare.c` (new) |
| prisc+x assembler | `RVLV_EMUL/0.hdl0+prisc-bootstrap/asm.pal` (new) |
| prisc+x C compiler | `RVLV_EMUL/0.hdl0+prisc-bootstrap/cc/` (new dir) |

---

## Constraints

- **Languages**: Bash, C, prisc+x (`.pal`) only — no Rust, no Python for bootstrap code
- **Host tools**: GCC, make, git allowed for building the bootstrap chain
- **Verification**: Each layer must be testable on rvemu before depending on it

---

## Future Consideration: Minimal Linux Kernel for Testing

**Proposed:** Build a minimal Linux kernel (tinyconfig + only virtio drivers) for fast boot testing.

**Requirements (per house conventions):**
- No header dependencies between components — use `fork/exec` for inter-process communication
- Each component is a standalone ELF binary loaded by rvemu
- Communication via file descriptors / VirtIO 9p / custom syscalls
- Matches the prisc+x custom op model: `spawn_elf(path, args...)` + fd passing

**Benefits:**
- Boots in seconds (not minutes) on rvemu/QEMU
- Minimal attack surface / complexity
- Can serve as "test harness" for rvemu syscall ABI development
- If successful, could evolve into the "M9 Linux" target (bootstrapped compiler builds this kernel)

**Relationship to xv6:**
- xv6 is a teaching OS (RISC-V port exists, already works on rvemu)
- Minimal Linux would be a *real* Linux kernel with only essential drivers
- Both serve as test targets; xv6 for bare-metal debugging, minimal Linux for syscall ABI

**Implementation approach:**
```bash
# In buildroot:
make qemu_riscv64_virt_defconfig
make linux-menuconfig
# -> General setup: tinyconfig base
# -> Device drivers: only VIRTIO_BLK, VIRTIO_CONSOLE, VIRTIO_NET
# -> File systems: EXT2, 9P (for 9p share)
# -> Strip all else
```

**Decision deferred:** This is a "nice to have" for M5-M6 timeframe, not a current blocker.
---

## Our Minimal Kernel Strategy (In-House)

**Proposed:** Build a minimal Linux kernel (tinyconfig + only virtio drivers) for fast boot testing.

**Our In-House Kernel Approach (not Buildroot):**
- **Don't use Buildroot's kernel config** — write our own minimal `.config` in `RVLV_EMUL/kernel/`
- **Target**: ~2-3 MB kernel, boots in <5 sec on rvemu
- **Drivers only**: virtio_blk, virtio_console, virtio_net, 9p (for file I/O)
- **No modules** — everything built-in
- **Boot args**: `console=ttyS0 root=/dev/vda rw rootwait init=/bin/sh`
- **Purpose**: Fast test harness for rvemu syscall ABI + prisc+x bare-metal
- **End goal**: Bootstrapped C compiler builds *this exact kernel config* (M9)

**Implementation:**
```bash
# In RVLV_EMUL/ (not buildroot/):
mkdir -p kernel
# Write kernel/.config manually (tinyconfig + virtio only)
# Compile with bootstrapped compiler once ready
# Until then: use buildroot cross-compiler as scaffold
```

**Decision:** Build our own minimal kernel config in-tree, not via Buildroot menuconfig. Buildroot stays for rootfs/initramfs scaffolding only.

**Status (2026-09-24):** Minimal kernel config working — boots on rvemu with UART output, virtio-blk detection, and rootfs mount. Kernel is 3.3 MB (vs 27 MB full). Rootfs compatibility issue (SIGILL in dynamic linker) requires ISA fix but kernel itself is functional.
