# A RISC-V emulator written in C

RV64GC (IMAFD + C + Zicsr + Zifencei) system emulator for the QEMU `virt`
machine. It runs bare-metal RISC-V tests, xv6, and Linux (OpenSBI or built-in
SBI).

Implemented:

1. RV64G (I, M, A, F, D) plus C, Zicsr, Zifencei
2. Interrupt handling (CLINT + PLIC, M/S timer, software, external)
3. Trap handling (M/S/U, delegation, PMP, `mret`/`sret`, WFI)
4. UART 16550A at `0x10000000`
5. VIRTIO MMIO block (legacy v1 and modern v2)
6. xv6-riscv
7. Linux for RISC-V (S-mode + SBI + generated DTB, or OpenSBI firmware)

Sv39 paging, QEMU-virt MMIO map, ELF and raw image loading.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=gcc
cmake --build build --parallel
```

The binary is `build/main` (`build/main.exe` on Windows).

```bash
build/main --selftest
```

## Run

```bash
build/main <binary.bin>
```

ELF64 RISC-V and raw images are loaded at `0x80000000` (or at the Linux Image
`text_offset`).

### xv6

Old xv6 (virtio MMIO version 1, e.g. rvemu-for-book images):

```bash
build/main --virtio-legacy -k images/xv6-kernel.bin -f images/xv6-fs.img
```

Current MIT xv6-riscv (virtio 1.0 / MMIO version 2, ELF `kernel` + `fs.img`):

```bash
build/main -k kernel -f fs.img
```

### Linux

Kernel plus built-in SBI and a generated virt DTB:

```bash
build/main --linux -k Image -f rootfs.img --bootargs "console=ttyS0 root=/dev/vda rw"
```

OpenSBI `fw_jump` at `0x80000000` and kernel at `0x80200000`:

```bash
build/main -b fw_jump.bin -k Image -f rootfs.img -i initrd.cpio
```

#### Boot proof

A minimal `init` script written to the rootfs proves userspace runs. Build the
kernel/Buildroot image once (`./build_buildroot.sh`) and use the regenerated
`kernel/Image` + `buildroot/output/images/rootfs.ext2`, then:

```bash
# Write a tiny init script into the ext2 rootfs
cat > /tmp/start.sh <<'EOF'
#!/bin/sh
mount -t proc proc /proc
exec > /dev/kmsg 2>&1
echo "RVEMU_LINUX_OK kernel=$(cat /proc/version)"
echo "cpu=$(head -1 /proc/cpuinfo)"
poweroff -f
EOF
# (copy /tmp/start.sh into the rootfs as /sbin/start, then)
build/main --linux -k kernel/Image -f rootfs.ext2 \
  --max-inst 50000000000 --bootargs "console=ttyS0,115200n8 earlycon root=/dev/vda rw rootwait init=/sbin/init"
```

Expected dmesg lines:
```
Run /sbin/start as init process
RVEMU_LINUX_OK kernel=Linux version ...
cpu=processor	: 0
reboot: Power down
```

### Options

| Option | Meaning |
| --- | --- |
| `-k FILE` | kernel / bare-metal image |
| `-b FILE` | M-mode firmware (OpenSBI, BBL) |
| `-f FILE` | virtio-blk disk |
| `-i FILE` | initrd |
| `--linux` | S-mode boot, emulate SBI, generate DTB |
| `--virtio-legacy` | virtio MMIO version 1 |
| `--dtb FILE` | use this DTB instead of generating one |
| `--bootargs STR` | Linux command line |
| `-m MIB` | DRAM size (default 128) |
| `--max-inst N` | stop after N instructions |
| `--selftest` | built-in ISA / trap / Sv39 checks |
| `--trace-trap` | print exceptions and interrupts |

## Memory map (QEMU virt)

| Range | Device |
| --- | --- |
| `0x02000000` | CLINT |
| `0x0c000000` | PLIC |
| `0x10000000` | UART0 (IRQ 10) |
| `0x10001000` | VIRTIO blk (IRQ 1) |
| `0x80000000` | DRAM |

## riscv-tests

Same flow as before: build `riscv-tests`, `objcopy -O binary`, then:

```bash
build/main tests/ori.bin
```

`test.py` still converts a `riscv-tests/isa` tree into `.bin` files.

A `p` environment test that `ecall`s with `a0=1` while `mtvec` is 0 is treated
as PASS. SiFive test-finisher writes at `0x100000` (`0x5555` pass, `0x3333` fail)
also stop the emulator.

## Status / roadmap

- **Linux boot (S-mode + SBI + generated DTB):** Fully working. Linux 6.18.7
  boots to `buildroot login:` prompt with ttyS0 console, virtio block, ext4,
  and FPU. See [docs/RVEM_LINUX_BOOT_NOTES.md](docs/RVEM_LINUX_BOOT_NOTES.md)
  for root causes fixed (kernel image header magic offset, SBI PMU extension,
  FPU `FS=Off` SIGILL, non-M `ecall` interception, UART DT node).
- **Built-in `--selftest` and bare-metal tests:** passing.

## XO.SFPGA.NN — Discrete 7400-Series FPGA Emulator

A software FPGA emulator built from discrete 74HC-series logic ICs, modeled in C
using HDL0 as the HDL. Lives in `XO.SFPGA.NN/`. See [XO.SFPGA.NN/architecture.md](XO.SFPGA.NN/architecture.md)
and [XO.SFPGA.NN/ROADMAP.md](XO.SFPGA.NN/ROADMAP.md).

### Toolchain

| Tool | Source | Purpose |
|---|---|---|
| `fpga` | `XO.SFPGA.NN/src/fpga.c` | FPGA fabric simulator (LUTs, registers, switchbox) |
| `pnr` | `XO.SFPGA.NN/src/pnr.c` | Places & routes HDL0 → bitstream |
| `vlog` | `XO.SFPGA.NN/src/verilog.c` | Verilog subset → HDL0 compiler |
| `counter4.hdl0` | `XO.SFPGA.NN/hdl0/` | 4-bit counter reference design |

### Build & Test

```bash
cd XO.SFPGA.NN
gcc -o /tmp/fpga src/fpga.c && /tmp/fpga --test     # 18/18 pass
gcc -o /tmp/pnr src/pnr.c
gcc -o /tmp/vlog src/verilog.c
```

### Pipeline

```console
$ /tmp/vlog input.v out.hdl0       # Verilog → HDL0 text
$ /tmp/pnr out.hdl0 out.bit        # HDL0 → bitstream
$ /tmp/fpga out.bit 10             # simulate 10 cycles
```

### Current status

- **Phase 1** — FPGA core: implemented, 18 test cases passing
- **Phase 2** — HDL0 compiler (`pnr`): implemented
- **Phase 3** — rvemu integration: `SYS_FPGA_LOAD` syscall planned for prisc+x
- **Phase 4** — Soft-core RISC-V (rv16): HDL0 reference written, compilation pending
- **Phase 5** — Real hardware (ICE40): future
