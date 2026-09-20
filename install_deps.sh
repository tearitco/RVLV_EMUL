#!/bin/bash
# RVLV_EMUL — Install dependencies for rvemu + bootstrap toolchain
# Run on fresh Ubuntu/Debian machine (or adapt for other distros)

set -euo pipefail

echo "=== Installing system packages ==="
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    gcc \
    gdb \
    git \
    wget \
    curl \
    python3 \
    python3-pip \
    device-tree-compiler \
    qemu-system-riscv64 \
    libglib2.0-dev \
    libpixman-1-dev \
    libfdt-dev \
    pkg-config \
    bc \
    cpio \
    rsync \
    unzip \
    xz-utils \
    file

echo "=== Installing RISC-V cross-compiler (GCC + newlib) ==="
# Option 1: Ubuntu package (older but works)
sudo apt-get install -y gcc-riscv64-unknown-elf

# Option 2: Build newer GCC + newlib from source (uncomment if needed)
# ./build_cross_compiler.sh

echo "=== Verifying cross-compiler ==="
riscv64-unknown-elf-gcc --version

echo "=== Building rvemu ==="
cd /home/no/Desktop/github/RVLV_EMUL
./build.sh

echo "=== Running self-test ==="
./build/main --selftest

echo "=== All dependencies installed! ==="
echo ""
echo "Next steps:"
echo "  1. Test xv6:     ./build/main -k xv6-riscv/kernel/kernel -f xv6-riscv/fs.img --max-inst 50000000"
echo "  2. Build Linux:  ./build_buildroot.sh   (takes 30-60 min)"
echo "  3. Test Linux:   ./build/main --linux -k buildroot/output/images/Image -f buildroot/output/images/rootfs.ext2"