#!/bin/bash
set -e

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
"$PNR" "$HDL0_DIR/gpio_reg.hdl0" /tmp/gpio_reg.bit
"$PNR" "$HDL0_DIR/alu4.hdl0" /tmp/alu4.bit
"$PNR" "$HDL0_DIR/rv16_core.hdl0" /tmp/rv16_core.bit
"$PNR" "$HDL0_DIR/rv16_prog.hdl0" /tmp/rv16_prog.bit
"$PNR" "$HDL0_DIR/counter_74hc161.hdl0" /tmp/counter_74hc161.bit
"$PNR" "$HDL0_DIR/bytelut_test.hdl0" /tmp/bytelut_test.bit
"$PNR" "$HDL0_DIR/seg_decoder.hdl0" /tmp/seg_decoder.bit
"$PNR" "$HDL0_DIR/alu32.hdl0" /tmp/alu32.bit
"$PNR" "$HDL0_DIR/mux_74hc251.hdl0" /tmp/mux_74hc251.bit
"$PNR" "$HDL0_DIR/shift_74hc595.hdl0" /tmp/shift_74hc595.bit
"$PNR" "$HDL0_DIR/tristate_74hc125.hdl0" /tmp/tristate_74hc125.bit
"$PNR" "$HDL0_DIR/tristate_74hc126.hdl0" /tmp/tristate_74hc126.bit
"$PNR" "$HDL0_DIR/bus_74hc4066.hdl0" /tmp/bus_74hc4066.bit
echo ""

# Build tests
cd "$SCRIPT_DIR"
make clean >/dev/null 2>&1 || true
make all

echo ""
echo "=== Running tests ==="
set +e
FAIL=0
PASS=0

for test in build/*.bin; do
    [ -f "$test" ] || continue
    name=$(basename "$test" .bin)

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
