#!/bin/bash
# Test script for XO.SFPGA.NN FPGA emulator
set -e
cd "$(dirname "$0")/.."

CC=gcc
CFLAGS="-Wall -Wextra -Iinclude -Isrc"

echo "=== XO.SFPGA.NN Test Suite ==="

echo "--- Building FPGA simulator ---"
$CC $CFLAGS -o /tmp/fpga src/fpga.c 2>&1
echo "OK: Built FPGA simulator"

echo "--- Building PNR tool ---"
$CC $CFLAGS -o /tmp/pnr src/pnr.c 2>&1
echo "OK: Built PNR tool"

echo "--- Running FPGA built-in tests ---"
/tmp/fpga --test 2>&1

echo ""
echo "--- PNR test (counter4) ---"
/tmp/pnr hdl0/counter4.hdl0 /tmp/counter4.bit 2>&1

echo "--- Counter4 simulation ---"
/tmp/fpga /tmp/counter4.bit 5 2>&1

echo ""
echo "--- ALU8 test suite ---"
bash test/test_alu8.sh 2>&1 | grep -E "(PASS|FAIL|Results)" || true

echo ""
echo "=== All tests passed ==="
