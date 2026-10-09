# Directory Guide — Every Folder Explained

**Repo Root:** `/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL`

---

## Top-Level Structure

```
RVLV_EMUL/
├── build/                    # Build output (gitignored)
│   └── main                  # rvemu binary
├── buildroot/                # Buildroot build tree (gitignored mostly)
│   ├── output/images/        # Kernel + rootfs (what rvemu boots)
│   └── .config               # Buildroot config
├── src/                      # rvemu C source (15 files)
├── includes/                 # rvemu headers
├── 0.hdl0+prisc-bootstrap/   # Bootstrap root (NAND → compiler)
│   ├── prisc+x.c             # PAL VM (770 lines, POSIX)
│   └── 0.🧿️halo.c_spx=HDLb0/  # Hardware layer
├── xv6-riscv/                # Patched xv6-riscv (git submodule)
├── tests/                    # Test programs for rvemu
├── *.sh                      # Build/test scripts
└── *.md                      # Documentation
```

---

## Detailed Breakdown

### `src/` — rvemu Source Code (15 files)

| File | Purpose | Key Functions |
|------|---------|---------------|
| `main.c` | Entry point, arg parsing, boot modes | `main()`, arg parsing, `cpu_init` |
| `cpu.c` | Core CPU loop, instruction decode/exec | `cpu_step()`, `cpu_execute()`, `cpu_fetch()` |
| `trap.c` | Trap/interrupt handling | `take_trap()`, `check_interrupts()`, `irq_enabled()` |
| `syscall.c` | **NEW** Linux syscall ABI | `syscall_handle()`, `do_write/read/open/close/fstat/lseek/brk/exit` |
| `csr.c` | CSR read/write, SBI ecall | `csr_read()`, `csr_write()`, `sbi_ecall()` |
| `mmu.c` | Sv39 page table walk | `cpu_mmu_translate()` |
| `dram.c` | DRAM load/store | `dram_load()`, `dram_store()` |
| `bus.c` | System bus, device routing | `bus_load()`, `bus_store()` |
| `clint.c` | Core Local Interruptor (timer, IPI) | `clint_tick()`, MSIP/MTIP |
| `plic.c` | Platform-Level Interrupt Controller | `plic_set_irq()`, `plic_pending_ctx()` |
| `virtio.c` | VirtIO block/console/9p | `virtio_disk_access()`, `virtio_irq_pending()` |
| `uart.c` | 16550A UART emulation | `uart_poll()`, `uart_irq_pending()` |
| `fpu.c` | Floating-point emulation | `exec_fp()` |
| `selftest.c` | Built-in ISA tests | `selftest()` |
| `dtb.c` | Device Tree Blob generation | `generate_dtb()` |

### `includes/` — Headers

| File | Contents |
|------|----------|
| `cpu.h` | CPU struct, function prototypes, `syscall_handle()` |
| `bus.h` | BUS struct, device interfaces |
| `csr.h` | CSR indices, MSTATUS/MIP/MIE bit definitions |
| `trap.h` | Trap struct, exception/interrupt codes |
| `opcodes.h` | RISC-V opcode constants |

### `0.hdl0+prisc-bootstrap/` — Bootstrap Root

```
0.hdl0+prisc-bootstrap/
├── prisc+x.c                    # PAL VM (770 lines)
├── prisc+x_bare.c               # TODO: Bare-metal rewrite (~300 lines)
├── asm.pal                      # TODO: RISC-V assembler in prisc+x
├── cc/                          # TODO: C compiler in prisc+x
│   ├── lexer.pal
│   ├── parser.pal
│   ├── codegen.pal
│   └── linker.pal
└── 0.🧿️halo.c_spx=HDLb0]☮️]ff6/   # HDLb0 hardware layer
    └── 0.HDLb0.PIECE+3.9-🧿️/
        ├── nand]z0]FIXD.c       # NAND gate implementation
        ├── nand_only.txt        # NAND test vectors
        └── rv-1-32]a0/          # RV1 → RV32I CPU designs
            ├── rv1/             # 1-bit CPU blueprint
            ├── rv2/             # 2-bit CPU
            ├── rv4/             # 4-bit CPU
            ├── rv8/             # 8-bit CPU
            ├── rv16/            # 16-bit CPU
            └── rv32/            # 32-bit CPU (RV32I)
```

**prisc+x.c key facts:**
- 770 lines, single-file VM
- Custom ISA: ADDI, BEQ, BNE, LW, SW, JALR, J, HALT, ECALL, CUSTOM
- String ops: SLIT, SCPY, SAPPEND, SFMT, SREAD, SSPLIT, SFIND, SLEN, SFOPEN, SFAPPEND, SWRITE, SFCLOSE, SBEQ, SBNE, STRIM, SATOI
- ECALL syscalls: open, close, write, read, get_kv_int, set_kv_int
- Custom ops: `+x/` shell scripts (Linux only)
- Needs full POSIX libc (popen, fork, pthread, dirent, asprintf)

### `buildroot/` — Buildroot Build Tree

```
buildroot/
├── .config                     # qemu_riscv64_virt_defconfig
├── output/
│   ├── images/
│   │   ├── Image               # Linux kernel (27 MB)
│   │   ├── rootfs.ext2         # Rootfs (60 MB ext2)
│   │   ├── rootfs.tar          # Rootfs tarball
│   │   ├── fw_jump.elf/bin     # OpenSBI firmware
│   │   └── start-qemu.sh       # QEMU launch script
│   ├── build/                  # Build artifacts per package
│   │   ├── linux-6.18.7/       # Kernel build
│   │   ├── host-gcc-*/         # Cross-compiler build
│   │   └── ...                 # ~200 packages
│   └── host/                   # Host tools (riscv64-buildroot-linux-gnu-*)
└── dl/                         # Downloaded source tarballs (~2-3 GB)
```

**Config:** `qemu_riscv64_virt_defconfig` — Linux 6.18.7, glibc, busybox, ext2 rootfs

### `xv6-riscv/` — Patched xv6 (Git Submodule)

```
xv6-riscv/
├── kernel/
│   ├── kernel                  # ELF kernel (built)
│   ├── kalloc.c                # PATCHED: memset removed from kfree()
│   └── ...
├── fs.img                      # Filesystem image
└── Makefile
```

**Patch:** `kfree()` no longer zeroes freed pages — boots in seconds instead of hours.

### `tests/` — rvemu Test Programs

| File | Purpose |
|------|---------|
| `test_hello.c` | Basic printf test |
| `test_syscall.c` | Syscall ABI test (write, read, open, close, exit) |
| `test_mmu.c` | MMU/Sv39 test |
| `test_virtio.c` | VirtIO disk test |

### Build & Test Scripts

| Script | Purpose |
|--------|---------|
| `build.sh` | Direct GCC build of rvemu (no CMake) |
| `build_buildroot.sh` | Buildroot build (30-60 min) |
| `test_emulator.sh` | Full test suite runner |
| `install_deps.sh` | Dependency installer (Ubuntu/Debian) |
| `install_cross_compiler.sh` | Alternative cross-compiler build |

### Key Documentation Files

| File | Purpose |
|------|---------|
| `NEXT_AGENT_PROMPT.txt` | Handoff prompt for next agent |
| `ROADMAP.md` | 10-milestone bootstrap plan (M1-M10) |
| `TODO.md` | Task checklist with status |
| `INVESTIGATION_REPORT.md` | Technical bug analysis |
| `HANDOFF-s15.txt` | Previous session handoff |
| `ARCHITECTURE_DECISIONS.md` | Locked-in bootstrap decisions |
| `PROGRESS_REPORT.md` | Session progress with timestamps |

---

## Build Artifacts (Gitignored)

```
build/
buildroot/output/
buildroot/dl/
buildroot/output/build/
xv6-riscv/kernel/kernel
xv6-riscv/fs.img
*.log
```

---

## Where to Make Changes

| Goal | Where |
|------|-------|
| Add rvemu syscall | `src/syscall.c` + `src/cpu.c:946` |
| Fix VirtIO | `src/virtio.c` |
| Fix MMU | `src/mmu.c` |
| Fix Timer/CLINT | `src/clint.c` |
| Fix PLIC | `src/plic.c` |
| Add prisc+x instruction | `0.hdl0+prisc-bootstrap/prisc+x.c` |
| Write prisc+x assembly | `0.hdl0+prisc-bootstrap/*.pal` |
| Buildroot config | `buildroot/.config` → `make menuconfig` |
| xv6 kernel patch | `xv6-riscv/kernel/kalloc.c` |

---

## Disk Space Estimates

| Component | Size |
|-----------|------|
| rvemu source + build | ~50 MB |
| Buildroot downloads (dl/) | ~2-3 GB |
| Buildroot build (output/) | ~5-10 GB |
| xv6-riscv | ~50 MB |
| **Total** | **~8-15 GB** |

---

*See [Quick Start](QUICK_START.md) for reproduction steps, [Agent Onboarding](AGENT_ONBOARDING.md) for workflow.*