---
name: rvlv-emul-bootstrap
description: Continue the in-house RVLV_EMUL bootstrap from rvemu and prisc+x through the .pal assembler, compiler, and self-hosting stack.
---

# RVLV_EMUL Bootstrap

## Operating Model

- Repository: `git@github.com:tearitco/RVLV_EMUL.git`
- Worktree: `/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL`
- Build with `./build.sh`; verify with `./build/main --selftest`.
- Bootstrap code is C, Bash, or `.pal`. Host GCC, Buildroot, musl, QEMU, `readelf`, and `objdump` are scaffolding/verification tools only.
- Preserve existing prisc+x syntax and behavior when extending it.
- Make each layer runnable and testable on rvemu before building the next layer.
- Do not claim self-hosting while a host-side converter is required.

## Verified State

- rvemu is an RV64 design: registers and PC are `uint64_t` (`includes/cpu.h:17-20`), and the decoder includes RV64 integer, compressed, M/A, F/D, CSR, AMO, trap, CLINT, PLIC, MMU, SBI, UART, and virtio paths.
- The DTB advertises `rv64imafdc` (`src/dtb.c:208`). Do not describe the emulator as RV32-only.
- `./build/main --selftest` passes.
- Buildroot Linux and a minimal in-tree kernel config are present; the minimal kernel boots with UART, virtio-blk, and rootfs mounting.
- The Buildroot userspace currently SIGILLs in the dynamic linker. Investigate the exact instruction and toolchain assumptions; do not assume vector support is the cause.
- `src/prisc_bare.c` is the bare-metal prisc+x VM and has been run on rvemu.
- rvemu has write/read/open/close/fstat/lseek/brk/exit syscalls plus a placeholder custom spawn syscall (`src/syscall.c:12-20`, `src/syscall.c:159-174`).

## Dirty Worktree Warning

Before editing, run `git status --short`. Preserve unrelated changes unless explicitly requested:

```text
M  TODO.md
M  buildroot
M  src/bare_test.c
?? 9p_share/
```

Never commit or push those files as part of assembler work without reviewing them.

## Next Milestone

Implement `0.hdl0+prisc-bootstrap/asm.pal` as an in-house RV64I assembler that runs on `prisc_x_bare.elf` and emits a real ELF64 RISC-V `PT_LOAD` image at `0x80000000`.

Minimum scope:

1. Comments, labels, `x0`–`x31`, decimal/hex immediates.
2. `li`/`addi`, `ecall`, `j`/`jal`, conditional branches, loads/stores, and basic R-type operations.
3. Two-pass label resolution and clear unsupported-mnemonic errors.
4. Raw ELF byte output. The existing string-write primitive cannot safely emit NUL-containing ELF bytes, so add a narrowly scoped raw-file-write primitive to both prisc+x implementations if needed.
5. Verify with `readelf`/`objdump`, then run the emitted ELF directly on rvemu.

## Commands

```bash
./build.sh
./build/main --selftest

buildroot/output/host/bin/riscv64-buildroot-linux-gnu-gcc.br_real \
  -nostdlib -static -mcmodel=medany -T bare.ld \
  -o prisc_x_bare.elf src/prisc_bare.c
./build/main -k prisc_x_bare.elf --max-inst 10000000

readelf -h -l -s output.elf
riscv64-buildroot-linux-gnu-objdump -d output.elf
```

## Documentation

- Current handoff: `compaction.md`
- Architecture decisions: `docs/ARCHITECTURE_DECISIONS.md`
- Progress: `PROGRESS_REPORT.md`
- Checklist: `CHECKLIST.md`
