#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
RVEMU="${RVEMU:-/tmp/rvemu}"
PNR="${PNR:-/tmp/pnr}"
HDL0_DIR="$PROJECT_ROOT/XO.SFPGA.NN/hdl0"

echo "=== FPGA Bare-Metal Test Framework ==="
echo "Project root: $PROJECT_ROOT"
echo "rvemu binary: $RVEMU"
echo "PNR binary: $PNR"
echo ""

# Check rvemu exists
if [ ! -x "$RVEMU" ]; then
    echo "ERROR: rvemu not found at $RVEMU"
    echo "Build it first: gcc -o $RVEMU ... (see CMakeLists.txt)"
    exit 1
fi

# Check PNR exists
if [ ! -x "$PNR" ]; then
    echo "Building PNR..."
    gcc -Wall -Wextra -I"$PROJECT_ROOT/XO.SFPGA.NN/src" \
        -o "$PNR" "$PROJECT_ROOT/XO.SFPGA.NN/src/pnr.c"
fi

# Build bitstreams
echo "Building bitstreams..."
"$PNR" "$HDL0_DIR/counter4.hdl0" /tmp/counter4.bit
"$PNR" "$HDL0_DIR/gpio_echo.hdl0" /tmp/gpio_echo.bit
echo ""

# Build tests
cd "$SCRIPT_DIR"
make clean >/dev/null 2>&1 || true
make all

echo ""
echo "=== Running tests ==="
FAIL=0
PASS=0

for test in build/*.bin; do
    [ -f "$test" ] || continue
    name=$(basename "$test" .bin)

    # fpga_test_fail is a negative test (expects exit code 1)
    if [ "$name" = "fpga_test_fail" ]; then
        echo -n "Test $name (negative): "
    else
        echo -n "Test $name: "
    fi

    "$RVEMU" "$test" 2>/dev/null
    code=$?

    if [ "$name" = "fpga_test_fail" ]; then
        if [ $code -eq 1 ]; then
            echo "PASS (exited $code as expected)"
            PASS=$((PASS + 1))
        else
            echo "FAIL (expected exit 1, got $code)"
            FAIL=$((FAIL + 1))
        fi
    elif [ $code -eq 0 ]; then
        echo "PASS"
        PASS=$((PASS + 1))
    else
        echo "FAIL (exit code $code)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "=== Results ==="
echo "Passed: $PASS"
echo "Failed: $FAIL"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
