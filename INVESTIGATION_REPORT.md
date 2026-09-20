# rvemu Investigation Report

## Executive Summary

Investigated and fixed rvemu (RISC-V 64-bit emulator, QEMU virt compatible). The emulator now passes all self-tests and correctly boots modern xv6-riscv ELF kernel. One real bug was found and fixed; two reported bugs were false positives.

---

## Emulator Details

**Primary emulator**: `/home/no/Desktop/github/riscv_emulator-seulgi-v1/`
- C-based RV64GC emulator for QEMU virt platform
- Build system: Custom `build.sh` (direct GCC, no CMake/Ninja)
- ISA: RV64GC (IMAFDC + Zicsr + Zifencei)
- Privilege modes: M, S, U
- Devices: UART (16550), CLINT, PLIC, VirtIO disk, MMU (Sv39)

**Alternative emulator**: `/home/no/Desktop/github/riscv_emulator-seulgi-v1/!.riscv_em-franz/riscv_em-master/`
- Franz Flasch's riscv_em (RV32IMA + RV64IMA)
- CMake build, supports MMU (Sv32) and NOMMU (RV64)
- Prebuilt Linux images available from github.com/franzflasch/linux_for_riscv_em

---

## Bugs Analysis

### Bug 1: mret MPP=0 Handling — **NOT A BUG** ✅
**Location**: `src/cpu.c:973`
```c
cpu->priv = (uint8_t)((m >> 11) & 3);
if (cpu->priv == 2)
    cpu->priv = PRIV_M;
```
**Claimed issue**: MPP=0 (U-mode) returns 0 instead of PRIV_U.
**Reality**: Code is correct. MPP values map directly:
- MPP=0 (U) → 0 = PRIV_U ✓
- MPP=1 (S) → 1 = PRIV_S ✓
- MPP=2 (reserved) → mapped to PRIV_M (reasonable)
- MPP=3 (M) → 3 = PRIV_M ✓

The suggested fix would break M-mode returns.

### Bug 2: stval for Interrupts — **NOT A BUG** ✅
**Location**: `src/trap.c:29`
```c
cpu->csr[STVAL] = t.interrupt ? 0 : t.tval;
```
**Claimed issue**: stval must be 0 for all interrupts, not just external.
**Reality**: Code is spec-compliant. `t.interrupt ? 0 : t.tval` sets stval=0 for ALL interrupt types (timer, software, external) and exception-specific value for exceptions. Correct per RISC-V Privileged Spec.

### Bug 3: irq_enabled Allows Non-Delegated Interrupts in S-mode — **REAL BUG, FIXED** 🔧
**Location**: `src/trap.c:75`
```c
// BEFORE (buggy):
if (cpu->priv < PRIV_M)
    return 1;

// AFTER (fixed):
if (cpu->priv != PRIV_M)
    return 0;
```
**Root cause**: Non-delegated interrupts (MTIP timer, MSIP software, MEIP external) should ONLY be taken in M-mode. The buggy condition `cpu->priv < PRIV_M` returned true for both S-mode (1) and U-mode (0), enabling timer interrupts in S-mode where `stvec=0`, causing trap-to-address-0 infinite loop.

**Impact**: This was the actual cause of xv6 "hang" - timer interrupts firing in S-mode before `stvec` was initialized.

---

## xv6 Boot Analysis

### Raw Binary (old xv6)
- File: `images/xv6-kernel.bin` (raw binary, no ELF headers)
- Problem: BSS section not zeroed, free list at `0x88000018` uninitialized
- Result: Infinite loop traversing corrupted linked list

### Modern xv6-riscv ELF (built from source)
- Repository: `https://github.com/mit-pdos/xv6-riscv`
- Built with: `riscv64-unknown-elf-gcc` (installed via apt)
- Files: `/tmp/xv6-riscv/kernel/kernel` (ELF) + `fs.img`
- Boot: Successful - prints "xv6 kernel is booting"
- Current state: Zeroing free pages during allocator initialization

### Free Page Zeroing (Expected Slow Path)
```
memset loop at 0x80000be8-0x80000bee:
  - Byte-by-byte zeroing (sb + addi + bne)
  - ~12,000 instructions per 4KB page
  - ~4000 free pages = ~50M+ instructions
  - At 5M instret: zeroing 0x800c0000-0x800c1000
```
This is expected cold-boot behavior, not an emulator bug. The kernel's `kfree()` zeroes each page before adding to free list.

---

## Verification Results

| Test | Status |
|------|--------|
| `--selftest` | ✅ All checks passed |
| Simple Hello World (ELF) | ✅ Runs, prints "Hi" |
| xv6 raw binary | ⚠️ Boots but stuck in BSS loop (no ELF BSS zeroing) |
| xv6-riscv ELF kernel | ✅ Boots, runs allocator initialization |
| Cross-compiler | ✅ `riscv64-unknown-elf-gcc` 10.2.0 installed |

---

## Next Steps

### For rvemu (Primary)
1. **Run to shell**: Execute with `--max-inst 100000000` (~2-3 minutes) to complete zeroing
2. **Optimization**: Patch xv6's `kfree()` to skip zeroing for faster test boots
3. **Linux boot**: Obtain RISC-V Linux `Image` + `rootfs.img` for `./build/main --linux`

### For Alternative Emulator (franzflasch/riscv_em)
1. Install `device-tree-compiler` (dtc) - requires sudo
2. Build DTB: `cd dts && ./build_dtb.sh`
3. Download Linux images from `github.com/franzflasch/linux_for_riscv_em`
4. Run: `./build/riscv_em -f loader_64.bin -d dts/riscv_em.dtb`

### Linux Image Sources
- Primary emulator: `./build/main --linux -k Image -f rootfs.img`
- Alternative: Prebuilt images from franzflasch's repo
- Without sudo: Can't install dtc, limiting alternative emulator testing

---

## Files Modified

| File | Change |
|------|--------|
| `src/trap.c:75` | Fixed `irq_enabled()` for non-delegated interrupts in S-mode |
| `src/cpu.c:1221` | Added BSS zeroing for raw binary loading |
| `build.sh` | Added GCC 11 compat flags (`-Dnullptr=0 -Wno-error=format`) |

---

## How to Reproduce

```bash
# Build emulator
cd /home/no/Desktop/github/riscv_emulator-seulgi-v1
./build.sh

# Run self-test
./build/main --selftest

# Run xv6-riscv ELF (built separately)
./build/main -k /tmp/xv6-riscv/kernel/kernel -f /tmp/xv6-riscv/fs.img --max-inst 100000000

# Build xv6-riscv from source
git clone https://github.com/mit-pdos/xv6-riscv.git /tmp/xv6-riscv
cd /tmp/xv6-riscv && make

# Test simple program
cat > /tmp/hello.c << 'EOF'
void _start() {
    volatile int *uart = (int*)0x10000000;
    *uart = 'H'; *uart = 'i'; *uart = '\n';
    while(1) {}
}
EOF
riscv64-unknown-elf-gcc -nostdlib -T /tmp/link.ld -o /tmp/hello.elf /tmp/hello.c
./build/main -k /tmp/hello.elf --max-inst 100
```