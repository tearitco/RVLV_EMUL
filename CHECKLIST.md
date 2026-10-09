# RVLV_EMUL — Master Checklist

**Updated:** 2026-09-23  
**Status:** M5 (rvemu runs Linux) ✅ | M6 (prisc+x on rvemu) ✅

---

## ✅ PHASE 1: rvemu Foundation (M5 — COMPLETE)

| Task | Status | Location |
|------|--------|----------|
| rvemu builds (GCC direct) | ✅ | `./build.sh` |
| Self-test passes | ✅ | `./build/main --selftest` |
| irq_enabled() fix (S-mode timer) | ✅ | `src/trap.c:75` |
| MTIMECMP CSR support | ✅ | `src/csr.c` |
| WFI wake logic | ✅ | `src/cpu.c` |
| xv6-riscv boots to shell | ✅ | `xv6-riscv/kernel/kalloc.c` patched |
| Buildroot Linux images built | ✅ | `buildroot/output/images/` |
| virtio_blk version fix | ✅ | `src/virtio.c` (VIRTIO_F_VERSION_1) |
| Syscall ABI (9 syscalls) | ✅ | `src/syscall.c`, `src/cpu.c:946` |
| Linux boots on QEMU (with 9p) | ✅ | Verified 2026-09-22 |

**Syscalls implemented:** write(64), read(63), open(1024), close(57), fstat(80), lseek(62), brk(214), exit(93), spawn(400)

---

## ✅ PHASE 2: prisc+x on rvemu (M6 — COMPLETE)

### 2.1 Cross-compile musl for bare-metal RISC-V
- [x] Download musl 1.2.5
- [x] Configure for `riscv64-unknown-elf` with buildroot cross-compiler
- [x] Build static libc.a
- [x] Install to `musl-install/`
- [x] Verify: `musl-gcc -static test.c` → RISC-V ELF

### 2.2 Write prisc+x_bare.c (clean rewrite)
- [x] VM core: regs, memory, instruction loop (ADDI, BEQ, BNE, LW, SW, JALR, J, HALT)
- [x] String ops: SLIT, SCPY, SAPPEND, SFMT, SREAD, SSPLIT, SFIND, SLEN, SFOPEN, SFAPPEND, SWRITE, SFCLOSE, SBEQ, SBNE, STRIM, SATOI
- [x] ECALL syscalls: open, close, read, write, exit, brk, spawn_elf
- [x] Custom op dispatch via rvemu syscall (not shell)
- [x] Minimal libc: memcpy, memset, strlen, strcmp, strcpy, atoi, isspace, isdigit
- [x] UART I/O via sys_write/sys_read
- [x] Target: ~750 lines, no POSIX deps (includes all string ops)

### 2.3 Compile & run bare-metal
- [x] Compile: `buildroot/output/host/bin/riscv64-buildroot-linux-gnu-gcc -nostdlib -static -mcmodel=medany -T bare.ld -o prisc_x_bare.elf src/prisc_bare.c`
- [x] Run on rvemu: `./build/main -k prisc_x_bare.elf --max-inst 10000000`
- [x] Verify: arithmetic, jumps, string ops, ECALLs work (exit code 0)

### 2.4 Custom Op ABI
- [x] Design `spawn_elf(path, argv, envp)` syscall (SYS_SPAWN=400)
- [x] File descriptor passing (via string register fd)
- [x] Implement in rvemu (`src/syscall.c` do_spawn stub)
- [x] Implement in prisc_bare (`builtin_spawn` custom op)
- [x] Test: prisc+x custom op calls spawn syscall

---

## ⏳ PHASE 3: Bootstrap Tools in prisc+x (M7-M8)

### 3.1 RISC-V Assembler in prisc+x (`.pal`)
- [ ] Lexer for RV32I assembly
- [ ] Parser → AST
- [ ] Codegen → ELF binary (minimal: text + data + symtab)
- [ ] Test: assembles simple program → runs on rvemu

### 3.2 C Compiler in prisc+x (`.pal`)
- [ ] Lexer (C subset: int, char, pointers, functions, control flow)
- [ ] Parser → AST
- [ ] Codegen → RV32I assembly text
- [ ] Pipe to assembler → ELF
- [ ] Minimal linker (resolve symbols, layout sections)

### 3.3 Self-Hosting
- [ ] Compiler compiles `prisc+x_bare.c` → ELF
- [ ] Compiled VM runs `.pal` files identically to C version
- [ ] Compiler compiles itself → ELF

---

## 🔮 PHASE 4: Full Circle (M9-M10)

### 4.1 Linux from Bootstrap
- [ ] Bootstrapped compiler builds minimal Linux kernel
- [ ] Bootstrapped compiler builds busybox/toolchain
- [ ] System boots on rvemu

### 4.2 Verification
- [ ] Linux runs compiler that built it
- [ ] HDLb0 NAND → RV32I verified against rvemu

---

## 📋 QUICK COMMANDS

```bash
# Self-test
./build/main --selftest

# xv6 shell
./build/main -k xv6-riscv/kernel/kernel -f xv6-riscv/fs.img --max-inst 50000000

# Linux on QEMU (fastest)
cd RVLV_EMUL && qemu-system-riscv64 -nographic -machine virt -m 128M \
  -kernel buildroot/output/images/Image \
  -drive file=buildroot/output/images/rootfs.ext2,format=raw,id=hd0 \
  -device virtio-blk-device,drive=hd0 \
  -fsdev local,id=fsdev0,path=9p_share,security_model=none \
  -device virtio-9p-pci,fsdev=fsdev0,mount_tag=hostshare \
  -append "console=ttyS0,115200n8 root=/dev/vda rw rootwait init=/bin/sh"

# Bare-metal prisc+x on rvemu
./build/main -k prisc_x_bare.elf --max-inst 10000000

# Build rvemu after changes
cd RVLV_EMUL && ./build.sh

# Full test suite
./test_emulator.sh

# Backup
./rezip-rvem.sh
```

---

## 📁 KEY PATHS

| Component | Path |
|-----------|------|
| rvemu source | `RVLV_EMUL/src/` |
| rvemu binary | `RVLV_EMUL/build/main` |
| Buildroot | `RVLV_EMUL/buildroot/` |
| Buildroot output | `RVLV_EMUL/buildroot/output/images/` |
| prisc+x (Linux) | `RVLV_EMUL/0.hdl0+prisc-bootstrap/prisc+x.c` |
| prisc+x bare | `RVLV_EMUL/src/prisc_bare.c` |
| musl build | `RVLV_EMUL/musl-build/musl-1.2.5/` |
| musl install | `RVLV_EMUL/musl-install/` |
| Documentation | `RVLV_EMUL/docs/` |
| Kernel config | `RVLV_EMUL/kernel/.config` (next) |
| Backup script | `RVLV_EMUL/rezip-rvem.sh` |

---

## 🎯 NEXT ACTION

**Write minimal kernel `.config`** in `RVLV_EMUL/kernel/`:

```bash
mkdir -p kernel
# Create .config with:
# - tinyconfig base
# - CONFIG_VIRTIO_BLK=y
# - CONFIG_VIRTIO_CONSOLE=y
# - CONFIG_VIRTIO_NET=y
# - CONFIG_9P_FS=y
# - CONFIG_EXT2_FS=y
# - All modules = n (built-in only)
# Compile with buildroot cross-compiler:
make -C kernel O=../kernel/build ARCH=riscv CROSS_COMPILE=riscv64-buildroot-linux-gnu- \
  -j$(nproc)
# Test on rvemu:
./build/main --linux -k kernel/build/arch/riscv/boot/Image -f buildroot/output/images/rootfs.ext2 --max-inst 10000000
```