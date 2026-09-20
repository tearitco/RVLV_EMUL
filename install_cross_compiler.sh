#!/bin/bash
# Install RISC-V cross-compiler (riscv64-unknown-elf-gcc)
# Run with: bash install_cross_compiler.sh

set -e

INSTALL_DIR="$HOME/opt/riscv"
mkdir -p "$INSTALL_DIR"

echo "=== Installing RISC-V cross-compiler ==="
echo "Install directory: $INSTALL_DIR"

# Check if already installed
if [ -f "$INSTALL_DIR/bin/riscv64-unknown-elf-gcc" ]; then
    echo "Cross-compiler already installed at $INSTALL_DIR"
    "$INSTALL_DIR/bin/riscv64-unknown-elf-gcc" --version
    exit 0
fi

# Try to install via package manager (requires sudo)
if command -v apt-get >/dev/null 2>&1; then
    echo "Trying apt-get install (requires sudo)..."
    if sudo apt-get update && sudo apt-get install -y gcc-riscv64-unknown-elf 2>/dev/null; then
        echo "Installed via apt-get"
        riscv64-unknown-elf-gcc --version
        exit 0
    fi
    echo "apt-get install failed or no sudo, trying manual build..."
fi

# Manual build from source
echo "Building cross-compiler from source..."

# Install dependencies
if command -v apt-get >/dev/null 2>&1; then
    echo "Installing build dependencies (requires sudo)..."
    sudo apt-get update && sudo apt-get install -y \
        build-essential \
        libgmp-dev \
        libmpfr-dev \
        libmpc-dev \
        libisl-dev \
        zlib1g-dev \
        flex \
        bison \
        texinfo \
        wget \
        2>/dev/null || echo "Could not install all deps, continuing..."
fi

cd /tmp
RISCV_GNU_TOOLCHAIN_VERSION="riscv-gnu-toolchain-2023.12.29"

echo "Downloading riscv-gnu-toolchain..."
wget -q "https://github.com/riscv-collab/riscv-gnu-toolchain/archive/refs/tags/$RISCV_GNU_TOOLCHAIN_VERSION.tar.gz" \
    -O "$RISCV_GNU_TOOLCHAIN_VERSION.tar.gz"

echo "Extracting..."
tar -xzf "$RISCV_GNU_TOOLCHAIN_VERSION.tar.gz"
cd "riscv-gnu-toolchain-$RISCV_GNU_TOOLCHAIN_VERSION"

echo "Configuring (newlib only, no Linux headers)..."
./configure --prefix="$INSTALL_DIR" --enable-multilib --with-arch=rv64gc --with-abi=lp64d

echo "Building (this takes 10-30 minutes)..."
make -j$(nproc)

echo "=== Installation complete ==="
echo "Add to PATH: export PATH=\"$INSTALL_DIR/bin:\$PATH\""
echo ""
"$INSTALL_DIR/bin/riscv64-unknown-elf-gcc" --version