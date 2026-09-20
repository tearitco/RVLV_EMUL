#!/bin/bash
# Buildroot for RISC-V QEMU virt (compatible with rvemu)

set -e

BUILDROOT_DIR="/home/no/Desktop/github/riscv_emulator-seulgi-v1/buildroot"
OUTPUT_DIR="$BUILDROOT_DIR/output"

echo "=== Buildroot RISC-V QEMU virt Build ==="
echo "This takes 30-60 minutes on first run"
echo ""

# Clone Buildroot if not exists
if [ ! -d "$BUILDROOT_DIR" ]; then
    echo "Cloning Buildroot..."
    git clone https://git.buildroot.net/buildroot "$BUILDROOT_DIR"
fi

cd "$BUILDROOT_DIR"

# Configure for RISC-V QEMU virt
echo "Configuring Buildroot..."
make qemu_riscv64_virt_defconfig

# Optional: Customize config (enable more packages, etc.)
# make menuconfig

echo "Starting build (this takes 30-60 minutes)..."
make -j$(nproc)

echo ""
echo "=== Build Complete ==="
echo "Kernel: $OUTPUT_DIR/images/Image"
echo "Rootfs: $OUTPUT_DIR/images/rootfs.ext2 (or rootfs.cpio)"
echo ""
echo "To run with rvemu:"
echo "  cd /home/no/Desktop/github/riscv_emulator-seulgi-v1"
echo "  ./build/main --linux -k $OUTPUT_DIR/images/Image -f $OUTPUT_DIR/images/rootfs.ext2 --max-inst 10000000"
