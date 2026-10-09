# Quick Start — Reproduce RVLV_EMUL Environment

**Target:** Fresh Ubuntu/Debian machine (or WSL2)  
**Time:** ~15 min (excluding Buildroot which takes 30-60 min)  
**Disk:** ~5 GB for Buildroot + downloads

---

## 1. Prerequisites

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential gcc gdb git wget curl python3 python3-pip \
    device-tree-compiler qemu-system-riscv64 \
    libglib2.0-dev libpixman-1-dev libfdt-dev pkg-config \
    bc cpio rsync unzip xz-utils file \
    gcc-riscv64-unknown-elf
```

Verify cross-compiler:
```bash
riscv64-unknown-elf-gcc --version
# Should show 12.x or 13.x
```

---

## 2. Clone & Build rvemu

```bash
cd ~/Desktop/github  # or wherever
git clone https://github.com/tearitco/RVLV_EMUL.git
cd RVLV_EMUL

# Build rvemu (direct GCC, no CMake)
./build.sh

# Run self-test
./build/main --selftest
# Expected: "selftest: all checks passed"
```

---

## 3. Test xv6-riscv (Optional)

```bash
# Boot xv6 to shell (takes ~30 sec due to memset in kfree)
./build/main -k xv6-riscv/kernel/kernel -f xv6-riscv/fs.img --max-inst 50000000
# You should reach: `$` prompt
```

---

## 4. Buildroot Linux Images (30-60 min)

```bash
# Uses all cores, downloads ~2-3 GB
./build_buildroot.sh

# Produces:
#   buildroot/output/images/Image       (Linux kernel)
#   buildroot/output/images/rootfs.ext2 (root filesystem)
```

**If Buildroot fails or takes too long:** Use prebuilt images (see below).

---

## 5. Test Linux Boot

```bash
# With Buildroot images
./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.ext2 --max-inst 100000000

# Or with prebuilt QEMU virt images (faster):
wget https://github.com/riscv/home/releases/download/v2023.04/Image -O buildroot/output/images/Image
wget https://github.com/riscv/home/releases/download/v2023.04/rootfs.img -O buildroot/output/images/rootfs.img
./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.img --max-inst 100000000
```

**Expected:** Linux boots, shows kernel messages, reaches userspace.  
**Known issue:** `virtio_blk` probe fails — rootfs not mounted. Work in progress.

---

## 6. Run Full Test Suite

```bash
./test_emulator.sh
# Runs: selftest, xv6, Linux
```

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `gcc-riscv64-unknown-elf` not found | `sudo apt-get install gcc-riscv64-unknown-elf` or build from source |
| Buildroot fails on host-gcc | `sudo apt-get install g++` (host C++ compiler) |
| Linux kernel build fails | Use prebuilt images (step 5) |
| `virtio_blk` probe fails | Known issue — rvemu virtio version mismatch |
| `max-inst` hit too early | Increase `--max-inst` (try 500M for Linux) |

---

## Directory Structure After Build

```
RVLV_EMUL/
├── build/
│   └── main                    # rvemu binary
├── buildroot/
│   └── output/images/
│       ├── Image               # Linux kernel
│       ├── rootfs.ext2         # Root filesystem (ext2)
│       └── rootfs.img          # Alternative (cpio)
├── xv6-riscv/
│   ├── kernel/kernel           # xv6 ELF kernel
│   └── fs.img                  # xv6 filesystem
├── 0.hdl0+prisc-bootstrap/
│   └── prisc+x.c               # PAL VM source
└── src/                        # rvemu source
```

---

## Next Steps After Boot

1. **Compile prisc+x.c as Linux binary:**
   ```bash
   gcc -o prisc_x 0.hdl0+prisc-bootstrap/prisc+x.c
   ./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.ext2 -a "prisc_x"
   ```

2. **Cross-compile musl for bare-metal:**
   ```bash
   # See docs/TODO_BLOCKERS.md for musl build steps
   ```

3. **Write prisc+x_bare.c** (clean rewrite for bare-metal)

---

## Useful Links

- [Architecture Decisions](../ARCHITECTURE_DECISIONS.md)
- [Directory Guide](DIRECTORY_GUIDE.md)
- [Agent Onboarding](AGENT_ONBOARDING.md)
- [Progress Report](PROGRESS_REPORT.md)