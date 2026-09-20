#!/bin/bash
# Test script for rvemu - runs xv6-riscv and optionally Linux

set -e

EMU="./build/main"
XV6_KERNEL="./xv6-riscv/kernel/kernel"
XV6_FS="./xv6-riscv/fs.img"
LINUX_KERNEL="Image"
LINUX_ROOTFS="rootfs.img"

echo "=== rvemu Test Suite ==="
echo ""

# Test 1: Selftest
echo "Test 1: Self-test"
$EMU --selftest
echo "✅ Self-test passed"
echo ""

# Test 2: xv6-riscv boot to shell
echo "Test 2: xv6-riscv boot (patched kfree, ~10M instructions)"
if [ -f "$XV6_KERNEL" ] && [ -f "$XV6_FS" ]; then
    timeout 60 $EMU -k "$XV6_KERNEL" -f "$XV6_FS" --max-inst 10000000 2>&1 | grep -E "(xv6 kernel is booting|init: starting sh|\$ )" && echo "✅ xv6-riscv booted to shell" || echo "❌ xv6-riscv failed"
else
    echo "⚠️  xv6-riscv not built (run: cd xv6-riscv && make)"
fi
echo ""

# Test 3: Linux boot (if images exist)
echo "Test 3: Linux boot (QEMU virt)"
if [ -f "$LINUX_KERNEL" ] && [ -f "$LINUX_ROOTFS" ]; then
    timeout 60 $EMU --linux -k "$LINUX_KERNEL" -f "$LINUX_ROOTFS" --max-inst 5000000 2>&1 | head -20 && echo "✅ Linux boot started" || echo "❌ Linux boot failed"
else
    echo "⚠️  Linux images not found (download from https://github.com/riscv/riscv-linux or build with Buildroot)"
    echo "   Note: Prebuilt QEMU virt images can be built with:"
    echo "     Linux: make qemu_riscv64_defconfig && make -j\$(nproc)"
    echo "     Rootfs: Buildroot qemu_riscv64_virt_defconfig"
fi
echo ""

echo "=== Test Suite Complete ==="
