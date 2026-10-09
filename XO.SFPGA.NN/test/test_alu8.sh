#!/bin/bash
# Test script for ALU8 operations
cd "$(dirname "$0")/.."

CC=gcc
INCLUDES="-I/home/jb/Desktop/ele-desk+cal/1.RVEM/RVLV_EMUL/includes -Iinclude -Isrc"

echo "=== ALU8 Test Suite ==="
echo "--- Building FPGA and PNR ---"
$CC $CFLAGS $INCLUDES -o /tmp/fpga ../src/fpga.c 2>&1 | grep -v "warning:" || true
$CC $CFLAGS $INCLUDES -o /tmp/pnr ../src/pnr.c 2>&1
echo "OK: Built"

echo "--- Compiling alu8.hdl0 ---"
/tmp/pnr hdl0/alu8.hdl0 /tmp/alu8.bit 2>&1

echo ""
echo "--- Testing ALU operations ---"

# Wire IDs from PNR output:
# a_bit0=24, a_bit1=26, a_bit2=28, a_bit3=30, a_bit4=32, a_bit5=34, a_bit6=36, a_bit7=38
# b_bit0=4,  b_bit1=9,  b_bit2=11, b_bit3=13, b_bit4=15, b_bit5=17, b_bit6=19, b_bit7=21
# fn0=5, fn1=6, fn2=7

pass=0
fail=0

set_a() {
    local val=$1
    local out=""
    for i in 0 1 2 3 4 5 6 7; do
        local bit=$(( (val >> i) & 1 ))
        local wire=$(( 24 + i * 2 ))
        out="${out}${wire}=${bit},"
    done
    echo -n "$out"
}

set_b() {
    local val=$1
    local wires=(4 9 11 13 15 17 19 21)
    local out=""
    for i in 0 1 2 3 4 5 6 7; do
        local bit=$(( (val >> i) & 1 ))
        out="${out}${wires[$i]}=${bit},"
    done
    echo -n "$out"
}

# pin0=result0 (bit 0 of result), pin1=result1 (bit 1), etc.
get_result() {
    local pin_line="$1"
    local result=0
    for i in 0 1 2 3 4 5 6 7; do
        local pin_val
        pin_val=$(echo "$pin_line" | awk -v p=$((i+3)) '{print $p}')
        if [ "$pin_val" = "1" ]; then
            result=$((result + (1 << i)))
        fi
    done
    echo "$result"
}

run_test() {
    local desc="$1" a_val="$2" b_val="$3" fn2="$4" fn1="$5" fn0="$6" expected="$7"
    local a_init b_init
    a_init=$(set_a "$a_val")
    b_init=$(set_b "$b_val")
    local init="${a_init}${b_init}5=${fn0},6=${fn1},7=${fn2}"
    local output
    output=$(/tmp/fpga /tmp/alu8.bit 1 --init "$init" 2>/dev/null | grep '^    0 ')
    local result
    result=$(get_result "$output")
    if [ "$result" = "$expected" ]; then
        echo "PASS: $desc ($result = 0x$(printf '%02x' $result))"
        pass=$((pass + 1))
    else
        echo "FAIL: $desc (expected $expected = 0x$(printf '%02x' $expected), got $result = 0x$(printf '%02x' $result))"
        echo "  pins: $(echo $output | awk '{for(i=3;i<=10;i++) printf "%d ", $i; print ""}')"
        fail=$((fail + 1))
    fi
}

# AND tests (fn=000)
run_test "AND(0xFF, 0x0F)" 255 15 0 0 0 15
run_test "AND(0xF0, 0x0F)" 240 15 0 0 0 0
run_test "AND(0xAA, 0x55)" 170 85 0 0 0 0
run_test "AND(0xFF, 0xFF)" 255 255 0 0 0 255

# OR tests (fn=001)
run_test "OR(0xF0, 0x0F)" 240 15 0 0 1 255
run_test "OR(0x00, 0xFF)" 0 255 0 0 1 255
run_test "OR(0xAA, 0x55)" 170 85 0 0 1 255

# XOR tests (fn=010)
run_test "XOR(0xFF, 0x0F)" 255 15 0 1 0 240
run_test "XOR(0xAA, 0xAA)" 170 170 0 1 0 0
run_test "XOR(0x00, 0xFF)" 0 255 0 1 0 255

# NOT A tests (fn=011)
run_test "NOT A(0xF0)" 240 0 0 1 1 15
run_test "NOT A(0xFF)" 255 0 0 1 1 0
run_test "NOT A(0x55)" 85 0 0 1 1 170

# ADD tests (fn=100)
run_test "ADD(0x01, 0x02)" 1 2 1 0 0 3
run_test "ADD(0x0F, 0x01)" 15 1 1 0 0 16
run_test "ADD(0x00, 0xFF)" 0 255 1 0 0 255
run_test "ADD(0x7F, 0x01)" 127 1 1 0 0 128
run_test "ADD(0xFF, 0x01)" 255 1 1 0 0 0

# SUB tests (fn=101)
run_test "SUB(0x03, 0x01)" 3 1 1 0 1 2
run_test "SUB(0x01, 0x03)" 1 3 1 0 1 254
run_test "SUB(0x80, 0x01)" 128 1 1 0 1 127
run_test "SUB(0x00, 0x01)" 0 1 1 0 1 255

# SLT tests (fn=110)
run_test "SLT(0xFF, 0x01)" 255 1 1 1 0 1
run_test "SLT(0x01, 0x02)" 1 2 1 1 0 1
run_test "SLT(0x7F, 0x01)" 127 1 1 1 0 0
run_test "SLT(0x80, 0x01)" 128 1 1 1 0 1
run_test "SLT(0x01, 0x80) [1 > -128 unsigned]" 1 128 1 1 0 0
run_test "SLT(0x00, 0x01)" 0 1 1 1 0 1
run_test "SLT(0x01, 0x00)" 1 0 1 1 0 0

# SLT unsigned comparison (positive numbers)
run_test "SLT(0x10, 0x20)" 16 32 1 1 0 1
run_test "SLT(0x20, 0x10)" 32 16 1 1 0 0

echo ""
echo "=== Results: $pass passed, $fail failed ==="
exit $fail
