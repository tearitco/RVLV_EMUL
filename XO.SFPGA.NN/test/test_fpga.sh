#!/bin/bash
# Test script for XO.SFPGA.NN FPGA emulator
set -e
cd "$(dirname "$0")/.."

CC=gcc
CFLAGS="-Wall -Wextra -Iinclude -Isrc"
SRCS="src/fpga.c src/switchbox.c src/bitstream.c"

echo "=== XO.SFPGA.NN Test Suite ==="

echo "--- Building FPGA test ---"
$CC $CFLAGS -o /tmp/fpga_test $SRCS src/fpga_test.c 2>&1
echo "OK: Built FPGA test"

echo "--- Running FPGA test ---"
/tmp/fpga_test 2>&1

echo ""
echo "--- PNR test (counter4) ---"
$CC $CFLAGS -o /tmp/pnr_test $SRCS src/pnr.c src/pnr_main.c 2>&1
/tmp/pnr_test hdl0/counter4.hdl0 /tmp/counter4.bit 2>&1

echo ""
echo "=== All tests passed ==="
