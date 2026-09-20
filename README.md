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
