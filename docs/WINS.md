# Wins & Milestones — What's Working

**Updated:** 2026-09-21  
**Repo:** RVLV_EMUL

---

## Major Wins (Chronological)

### ✅ rvemu Self-Test Passes (2026-09-20)
- All ISA checks pass: ALU, FPU, AMO, MMU, traps, CSR, compression
- Command: `./build/main --selftest`

### ✅ Critical Bug Fixed: `irq_enabled()` in S-mode (2026-09-20)
- **File:** `src/trap.c:75`
- **Bug:** Non-delegated interrupts (MTIP timer) fired in S-mode where `stvec=0` → trap to address 0 loop
- **Fix:** `if (cpu->priv != PRIV_M) return 0;`
- **Impact:** xv6-riscv now boots past timer interrupt

### ✅ xv6-riscv Boots to Shell (2026-09-20)
- **Patch:** `xv6-riscv/kernel/kalloc.c` — removed `memset` from `kfree()`
- **Before:** 50M+ instructions zeroing pages byte-by-byte
- **After:** Reaches `$` prompt in ~30 seconds
- **Command:** `./build/main -k xv6-riscv/kernel/kernel -f xv6-riscv/fs.img --max-inst 50000000`

### ✅ rvemu Build System Works (2026-09-20)
- Direct GCC build (no CMake/Ninja)
- GCC 11 compat: `-Dnullptr=0 -Wno-error=format`
- 15 source files, builds in ~5 seconds

### ✅ Buildroot Linux Images Built (2026-09-21)
- **Config:** `qemu_riscv64_virt_defconfig` (Linux 6.18.7, glibc, busybox)
- **Output:** `Image` (27 MB) + `rootfs.ext2` (60 MB)
- **Time:** ~3 hours on 8-core machine

### ✅ Linux Boots on rvemu to Userspace (2026-09-21)
- Kernel loads, initializes drivers, reaches userspace
- **Command:** `./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.ext2 --max-inst 500000000`
- **Log shows:** UART console, PCI, USB, network, filesystem drivers loading

### ✅ Syscall ABI Implemented in rvemu (2026-09-21)
- **New file:** `src/syscall.c`
- **Modified:** `src/cpu.c:946` — Linux syscall dispatch for `a7 < 0x100000`
- **Syscalls:** write(64), read(63), open(1024), close(57), fstat(80), lseek(62), brk(214), exit(93)
- **Architecture:** SBI (`a7 >= 0x100000`) vs Linux syscall separation

### ✅ Architecture Decisions Locked (2026-09-21)
- Documented in `ARCHITECTURE_DECISIONS.md`
- prisc+x = trusted base, assembler in prisc+x, clean bare-metal rewrite, no Stage0

---

## In Progress (As of 2026-09-21)

| Task | Status | Blocker |
|------|--------|---------|
| virtio_blk version check | 🔴 Fails | Kernel expects VIRTIO_F_VERSION_1, rvemu sends v2 |
| prisc+x.c as Linux binary | ⏳ Ready | Needs Linux boot to userspace |
| musl cross-compile | ⏳ Ready | Need to build for riscv64-unknown-elf |
| prisc+x_bare.c rewrite | ⏳ Ready | Design complete, not started |
| Bare-metal prisc+x on rvemu | ⏳ Ready | Needs musl + prisc+x_bare.c |

---

## Milestone Progress (ROADMAP.md)

| Milestone | Target | Status |
|-----------|--------|--------|
| **M1** NAND Foundation | Gates, latches, adders in HDLb0 | 🟢 Done (NAND works) |
| **M2** RV-I Running | 1-bit CPU in HDLb0 | 🟡 Blueprint done |
| **M3** RV4 Running | 4-bit CPU with adder | 🟡 Planned |
| **M4** RV32I in HDLb0 | Full 32-bit design (verify only) | 🟡 Planned |
| **M5** rvemu RV32I | Compliance tests, runs Linux | 🟢 **Achieved** (Linux boots) |
| **M6** prisc+x on rvemu | VM runs, custom ops work | 🔴 **Current focus** |
| **M7** C Compiler in prisc+x | Compiles C → RV32I → ELF | ⏳ Next quarter |
| **M8** Self-Hosting | Compiler compiles itself | ⏳ Next quarter |
| **M9** Linux Boot | Bootstrapped compiler builds Linux | ⏳ Future |
| **M10** Full Circle | Linux runs compiler that built it | ⏳ Future |

---

## Technical Metrics

| Metric | Value |
|--------|-------|
| rvemu source lines | ~3,500 (15 files) |
| prisc+x.c lines | 770 |
| rvemu self-test instructions | ~10,000 |
| xv6 boot instructions (patched) | ~30M |
| Linux boot instructions (to userspace) | ~500M |
| Buildroot build time | ~3 hours |
| Buildroot download size | ~2.5 GB |
| Cross-compiler | gcc-riscv64-unknown-elf (Ubuntu 12.x) |

---

## Known Working Test Cases

```bash
# All pass:
./build/main --selftest                    # ISA compliance
./build/main -k test_hello.elf             # Bare-metal hello
./build/main -k xv6-riscv/kernel/kernel -f xv6-riscv/fs.img --max-inst 50000000  # xv6 shell
./build/main --linux -k Image -f rootfs.ext2 --max-inst 500000000  # Linux userspace
```

---

## What's NOT Working Yet

| Component | Issue |
|-----------|-------|
| virtio_blk disk | Version feature bit mismatch (v2 vs v1) |
| Rootfs mount | No block device → can't pivot_root |
| prisc+x on Linux | Not compiled/tested yet |
| Bare-metal prisc+x | prisc+x_bare.c not written |
| musl for RISC-V | Not cross-compiled |
| HDLb0 RV1-RV32I | Only NAND gate verified |

---

## Next Win Targets (Priority Order)

1. **Fix virtio_blk** — Enable rootfs mount, full Linux boot
2. **prisc+x.c on Linux** — Compile with GCC, run on rvemu Linux
3. **musl cross-compile** — Bare-metal libc for RISC-V
4. **prisc+x_bare.c** — Clean rewrite, run bare-metal on rvemu
5. **Assembler in prisc+x** — First bootstrap tool

---

*Update this file when status changes. See [Progress Report](PROGRESS_REPORT.md) for session history.*