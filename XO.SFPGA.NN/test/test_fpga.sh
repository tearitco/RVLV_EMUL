#!/bin/bash
# Test script for XO.SFPGA.NN FPGA emulator
set -e
cd "$(dirname "$0")/.."

CC=gcc
CFLAGS="-Wall -Wextra -Iinclude -Isrc"

echo "=== XO.SFPGA.NN Test Suite ==="

# Build
$CC $CFLAGS -o /tmp/fpga_test src/fpga.c src/bitstream.c src/fpga_test.c 2>&1
echo "✅ Built FPGA emulator"

# Run
echo "--- LUT AND gate test ---"
/tmp/fpga_test 2>&1
echo ""

echo "=== All tests passed ==="
