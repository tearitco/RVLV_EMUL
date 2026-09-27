# rvemu Linux Boot Status Notes

For agents/users working on getting Buildroot Linux to boot under rvemu
(`RVLV_EMUL`). Last verified Sat Sep 27 2026.

## Run commands
```
# rvemu only:
./build.sh && ./build/main --selftest

# Linux (from RVLV_EMUL):
./build/main --linux -k kernel/Image \
  -f buildroot/output/images/rootfs.ext2 --max-inst N
```

## Kernel config (important!)
There are **two** Linux trees; do not mix them:

1. `kernel/src/linux-6.18.7/` — the tree actually baked into `kernel/Image`.
   - `.config` at `kernel/.config` (NOT `kernel/src/linux-6.18.7/.config`!).
   - Has `CONFIG_FPU` disabled by default.
2. `buildroot/output/build/linux-6.18.7/` — Buildroot's copy (source is complete;
   `/proc`-able). Has `scripts/config`.

To enable the FPU (and rebuild `kernel/Image`):
```
cd buildroot/output/build/linux-6.18.7
./scripts/config --enable FPU --enable RISCV_ISA_D --enable RISCV_ISA_F
./scripts/config --enable BINFMT_ELF --enable BINFMT_SCRIPT
./scripts/config --set-str CMDLINE "console=ttyS0,115200n8 earlycon root=/dev/vda rw rootwait init=/sbin/init"
make -j$(nproc) ARCH=riscv CROSS_COMPILE=$(pwd)/../../host/bin/riscv64-linux- Image
cp arch/riscv/boot/Image $RVLV_EMUL/kernel/Image
```
Toolchain: `buildroot/output/host/bin/riscv64-linux-gcc`.

## Issue #1 — SIGILL in ld-linux (FIXED)
- Symptom: `init[1]: unhandled signal 4 ... ld-linux-riscv64-lp64d.so.1` at
  faulting instruction `0x06853827` = `fsd fs0,112(a0)` inside `setjmp`,
  kernel oops `cause: 00000002` (illegal instruction).
- Root cause: kernel `.config` had `# CONFIG_FPU is not set`, so `has_fpu()`
  (arch/riscv/kernel/process.c:147) is false and `start_thread()` never sets
  `mstatus.FS` -> FS stays `Off`. glibc's hard-float `ld-linux` issues an FP
  instruction, rvemu traps per spec.
- Fix (rvemu): `src/fpu.c:199-201` — when `!fp_enabled(cpu)` (FS off), instead
  of trapping, set `MSTATUS_FS_DIRTY | MSTATUS_SD` and execute the FP insn.
  Rationale: this kernel build never saves/restores FP state, so the FS
  lifecycle is never exercised; the emulator can therefore treat the FPU as
  always-present. If you rebuild the kernel WITH `CONFIG_FPU`, this workaround
  becomes unnecessary and can be reverted:
  ```c
  if (!fp_enabled(cpu))
      return trap_ex(EX_ILLEGAL_INST, inst);
  ```

## Issue #2 — U-mode page fault / SIGTRAP in ld-linux (OPEN)
After the SIGILL fix, boot reaches `Run /sbin/init as init process`, then:
```
trap ex cause=12 pc=3fad2a24cc ... priv=0   (instruction page fault)
trap ex cause=13 pc=3fad2a2f44 ... priv=0   (load page fault)
trap ex cause=15 pc=3fad2a2f68 ... priv=0   (store page fault)
...
trap ex cause=3  pc=3fad2a827c ... priv=0   (ebreak -> SIGTRAP)
Kernel panic - not syncing: Attempted to kill init! exitcode=0x00000005
```
`3fad28a000` is the `ld-linux` ELF base. Offset `0x1e27c` is the `ebreak`
inside glibc's `_Exit` fallback (`ld-linux .../_exit.c`): glibc calls
`exit_group` (a7=94); if the ecall does not terminate the process it loops
into the `ebreak`.

### Root cause: ecall interception bypass for non-M mode (FIXED)
`src/cpu.c:947` (original) dispatched **any** non-M-mode `ecall` whose `a7 <
0x100000` to rvemu's custom bare-metal syscall ABI. Linux syscalls (openat=56,
exit_group=94, ...) are < 0x100000, so they were swallowed by rvemu instead of
being delivered to the Linux kernel as `EX_ECALL_S`/`EX_ECALL_U`. The earlier
"page fault / ebreak storm" was downstream: glibc's `exit_group` ecall returned
`-ENOSYS`, glibc's `_Exit` loop hit `ebreak` (cause 3), and the kernel killed
init (SIGTRAP).

Fix: only use the custom `syscall_handle` for the real `SYS_SPAWN` number:
```c
// src/cpu.c
#define SYS_SPAWN 400
...
if (cpu->priv != PRIV_M && a7 == SYS_SPAWN) {
    uint64_t ret = syscall_handle(cpu, a7, ...);
    cpu->regs[10] = ret; cpu->pc = next; break;
}
```

### Syscall coverage (status)
`src/syscall.c:syscall_handle` still only implements `write, read, open, close,
fstat, lseek, brk, spawn(400), exit(93)`. A full glibc userspace additionally
needs `mmap, openat, getdents64, rt_sigaction, getpid, getrandom, uname, ...`.
Not required for the boot smoke test; needed for a real interactive login.

### rvemu MMU notes (sanity-checked, appears OK)
- `src/mmu.c:cpu_mmu_translate` — SV39 walk; A/D bit auto-update via
  `bus_store` (return ignored). Superpage alignment check at line 176-179 uses
  `mask = (1ull << (9*level)) - 1` — correct for RISC-V leaf check on ppn
  low bits.
- `EX_BREAKPOINT = 3` (see `includes/trap.h`).

### Working boot smoke test (verified Sat Sep 27)
ttyS0 console doesn't attach to a real console, so observe userside via
`/dev/kmsg` (appears in dmesg). With `/sbin/start` as `init=` and
`exec > /dev/kmsg 2>&1`:
```
START_INIT
remount rc=0
RVEMU_LINUX_OK kernel=Linux version 6.18.7 ... #17 ...
cpu=processor	: 0
reboot: Power down
```
This proves kernel -> ld.so -> dynamically-linked busybox sh runs under rvemu.
- `dram_in_range` / `dram_load` fine.

## Other rvemu quirks seen (not currently blocking)
- Trap-print budget in `src/trap.c:10` is `< 40`; raise to inspect faults.
- `--trace-trap` prints to stderr; kernel console output also goes there for
  rvemu UART.

## Quick test for FPU regressions
`RVLV_EMUL/bare_test.elf` exercises `fsd`/`fld` in M-mode and passes rc=0 with
the current `src/fpu.c` fix in place.
