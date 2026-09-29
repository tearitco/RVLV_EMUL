# Project Guide for Future Agents

## Quick Start
```
SRC=/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL
cd "$SRC" && ./build.sh && ./build/main --selftest
```

## Linux boot (full boot to login prompt):
```
"$SRC/build/main" --linux -k "$SRC/kernel/Image" -f "$SRC/buildroot/output/images/rootfs.ext2" --max-inst 50000000000 --bootargs "console=ttyS0,115200n8 earlycon root=/dev/vda rw rootwait init=/sbin/init"
```

## Path Quirk
The bash tool workdir param intermittently fails with NotFound error. Workaround: use variable SRC=/path; "$SRC/file".

## Directory Structure
- src/      rvemu emulator source (cpu.c, csr.c, fpu.c, trap.c, sbi.c, etc.)
- main.c    rvemu entry point (in project root, NOT src/)
- includes/ headers (cpu.h, csr.h, memmap.h, uart.h, etc.)
- kernel/   Linux kernel Image + extracted source tree
- buildroot/ Buildroot build system (produces kernel Image + rootfs.ext2)
- docs/     Documentation

## Linux Boot Status
Linux 6.18.7 boots to buildroot login: prompt. See RVEM_LINUX_BOOT_NOTES.md for details.

## Buildroot Rebuild
```
cd "$SRC/buildroot" && make clean && nohup make BR2_JLEVEL=$(nproc) > /tmp/buildroot_full.log 2>&1 &
```
Wait ~45-60 min. After rebuild: cp "$SRC/buildroot/output/images/Image" "$SRC/kernel/Image"

## Test Files
- test_linux.pal expected Linux boot patterns for automated verification
- test.py Python test harness
