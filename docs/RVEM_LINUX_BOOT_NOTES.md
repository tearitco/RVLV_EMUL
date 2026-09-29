# rvemu Linux Boot Status Notes
For agents working on getting Buildroot Linux to boot under rvemu (RVLV_EMUL).
Last verified Sat Sep 28 2026 - Linux 6.18.7 boots to buildroot login.

## Run commands
./build.sh && ./build/main --selftest
./build/main --linux -k kernel/Image -f buildroot/output/images/rootfs.ext2 --max-inst 50000000000 --bootargs "console=ttyS0,115200n8 earlycon root=/dev/vda rw rootwait init=/sbin/init"

## Boot milestones (verified)
- ttyS0 8250 serial driver at MMIO 0x10000000
- virtio_blk [vda] 122880 blocks (62.9 MB)
- ext4 mounted on vda
- /sbin/init (busybox) runs -> "Welcome to Buildroot" -> "buildroot login:"

## Fixes for Linux 6.18
1. Image header magic offset (src/cpu.c): kernel 6.18 moved magic2 from offset 48 to 56. Check both offsets.
2. SBI PMU extension (src/sbi.c): add SBI_EXT_PMU (0x504D55) returning NOT_SUPPORTED.
3. Debug printf (src/trap.c): gate behind trace_trap flag.

## Previously fixed (no longer active)
- SIGILL (FPU fix in src/fpu.c)
- ecall interception (src/cpu.c:947, only SYS_SPAWN=400 to bare-metal)
- UART DT node (src/dtb.c:280, device_type="serial")

## Kernel config requirements
CONFIG_FPU=y, BINFMT_ELF=y, TTY=y, SERIAL_8250=y, SERIAL_8250_CONSOLE=y, EXT4_FS=y, VIRTIO_MMIO=y, VIRTIO_BLK=y

## Path quirk
bash workdir param intermittently fails on paths with "+". Use variable form: P=/path; "$P/file"

## TODO
- eth0/e1000e doesn't come up (network init timeout)
- 50B+ instructions needed for full boot (~7M i/s, simple interpreter)
