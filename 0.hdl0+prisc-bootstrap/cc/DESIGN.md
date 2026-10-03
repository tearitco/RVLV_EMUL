# C Compiler in prisc+x (.pal) — Design Document (v2)

## Overview

A minimal C compiler written entirely in prisc+x assembly (`.pal`). It compiles a
C subset to RV32I assembly text, which is then assembled by `asm.pal` into ELF.

## Language Subset (Phase 1)

### Types
- `int` (32-bit signed)
- `char` (8-bit, stored in int)
- `int*` / `char*` (pointers) — Phase 2

### Statements
- Variable declarations with initialization: `int x = 5;`
- Assignment: `x = 10;`
- Return: `return expr;`
- If/else: `if (cond) { ... } else { ... }` — Phase 2
- While loops: `while (cond) { ... }` — Phase 2
- Function definitions: `int main() { ... }`
- Function calls: `foo(args);`

### Operators
1. `=` (assignment, right-assoc)
2. `||` (logical or)
3. `&&` (logical and)
4. `==`, `!=` (equality)
5. `<`, `>`, `<=`, `>=` (relational)
6. `+`, `-` (additive)
7. `*`, `/`, `%` (multiplicative)
8. `!` (logical not)
9. Unary `-`, `&`, `*`
10. `()` (function call / grouping)

### Built-in Functions
- `putint(int n)` — output integer
- `putchar(int c)` — output character (Phase 2)
- `exit(int code)` — exit program (Phase 2)

## .pal VM Constraints (Critical)

### Available Instructions
| Category | Instructions |
|----------|-------------|
| Arithmetic | `addi`, `add`, `sub`, `andi`, `ori`, `xori`, `slli`, `srli` |
| Shifts | `sll`, `slt`, `sltu`, `srl`, `sra`, `xor`, `or` |
| Memory | `lw`, `sw`, `li` |
| Branch | `beq`, `bne` (NO `blt`/`bgt`/`ble`/`bge`) |
| Jump | `j label` (NO `jal`/`jr`/`jalr` for subroutines) |
| Control | `halt`, `ecall`, `builtin_out xN` |

### Workarounds
- **Multiply**: Use repeated `add` or shift-add: `x * 3` → `add tmp, x, x; add result, tmp, x`
- **Less-than branch**: `slt xt, xa, xb; bne xt, x0, label`
- **Function calls**: NO subroutine calls possible. All code is **inlined linearly**
- **Multiply by constant**: Use `slli` + `add` sequences

### String Operations (Linux prisc+x.c version)
| Op | Syntax | Returns |
|-----|--------|---------|
| `slit` | `slit sD, "str"` | — |
| `scpy` | `scpy sD, sS` | — |
| `sappend` | `sappend sD, sS` | — |
| `sread` | `sread sD, xN, sM` | reads line N from file sM into sD, len → x12 |
| `slen` | `slen xD, sS` | length → xD (integer) |
| `sfind` | `sfind xD, sS, "sub"` | index → xD (integer) |
| `satoi` | `satoi xD, sS` | atoi → xD (integer) |
| `sbeq` | `sbeq sA, sB, label` | branch if strings equal |
| `sbne` | `sbne sA, sB, label` | branch if strings not equal |
| `sfopen` | `sfopen xD, sM` | open file sM for write, fd → xD |
| `swrite` | `swrite xN, sS` | write sS to fd xN |
| `sfclose` | `sfclose xN` | close file fd |
| `strim` | `strim sD, sS` | trim whitespace |

### String Operation Limitations
- **No `sgetc` or `sindex`**: Cannot read individual character at position N
- **No `ssubstr`**: Cannot extract substring
- **No `schr`**: Cannot get character at position
- **Workaround**: Use `sfind` to locate delimiters, use `sread` to read file lines

### Register Constraints
- 16 integer registers (x0-x15) — very limited
- 32 string registers (s0-s31), 4096 bytes each
- 4096 int memory slots for dmem
- 1024 instruction limit per `.pal` file

### Memory Layout (dmem)
```
0-499:   working variables
500-999: token array (3 slots per token)
1000-1499: AST nodes (4 slots per node)
1500-1999: symbol table (4 slots per symbol)
2000-4095: available
```

## Architecture

### Single `.pal` File Approach (Phase 1)

Due to the 1024-instruction limit and no subroutine calls, the compiler runs as
a single `.pal` file that:

1. Reads C source file line by line using `sread sD, xN, sM`
2. For each line, uses `sfind` to locate delimiters (spaces, `=`, `;`, `(`, etc.)
3. Extracts tokens using sfind results as positions
4. Classifies tokens via `sbeq` against keyword strings
5. Emits RV32I assembly directly using file output (`sfopen`/`swrite`)

### Token Structure (3 slots in dmem per token)
```
slot 0: type (1=keyword, 2=ident, 3=int_lit, 4=op, 5=punct)
slot 1: value (integer value)
slot 2: str_idx (index into string register pool)
```

### Symbol Table (4 slots per symbol)
```
slot 0: name_idx (string register index)
slot 1: type (1=int, 2=char, 3=int*, 4=char*)
slot 2: value (constant or stack offset)
slot 3: scope (0=global, 1=local)
```

### Register Allocation Strategy
Since there's no subroutine mechanism, the compiler uses all 16 registers
statically for its own operation:

- x1-x3: loop counters and indices
- x4-x7: scratch/temporary
- x8: token count / AST node count
- x9-x12: addresses and computed values
- x13-x15: syscall parameters (reserved for ECALL)

String registers:
- s0: source filename
- s1: current source line
- s2: token buffer (building)
- s3-s9: keyword comparison strings
- s10-s15: temporary strings
- s16+: symbol names in string register space

## Code Generation Strategy

### Simplified Approach: Pattern Matching
Instead of building a full AST, the compiler uses **line-by-line pattern matching**
for Phase 1. Each line type has a dedicated code path that emits the appropriate
RV32I instructions.

#### Supported Line Patterns (Phase 1)
```
int IDENT = EXPR;         → declaration + assignment
IDENT = EXPR;             → assignment
return EXPR;              → return
IDENT();                  → function call
putint(EXPR);             → built-in call
int main() {              → function start
}                         → function end
```

### Expression Codegen
Expressions are evaluated using a simple stack machine approach:
- Load operands into registers
- Apply operator
- Result in x12 (accumulator)

Example: `putint(x + 1)`
```
lw x10, x_offset(sp)    # load x
li x11, 1               # load 1
add x10, x10, x11       # x + 1
li x17, 1               # putint syscall (or inline)
```

## Test Plan

### Phase 1: Variable + Arithmetic
```c
int main() {
    int x = 5;
    int y = 3;
    putint(x + y);
    putint(x - y);
    putint(x * y);
    return 0;
}
```
Expected output: "8\n2\n15\n"

### Phase 2: Control Flow
```c
int main() {
    int i = 0;
    while (i < 3) {
        putint(i);
        i = i + 1;
    }
    return 0;
}
```
Expected output: "0\n1\n2\n"

### Phase 3: Conditionals
```c
int main() {
    int x = 10;
    if (x > 5) {
        putint(1);
    } else {
        putint(0);
    }
    return 0;
}
```
Expected output: "1\n"

## Self-Hosting Path

1. Write compiler in `.pal`
2. Compiler compiles C source → RV32I assembly text
3. `asm.pal` assembles → binary
4. Binary runs on rvemu
5. Once complete: compiler compiles its own source (written in C dialect)

## Files

```
cc/
├── DESIGN.md              # This document
├── test_phase1.pal        # Phase 1 test: variable + arithmetic
├── test_phase2.pal        # Phase 2 test: while loops
├── test_phase3.pal        # Phase 3 test: conditionals
├── test/
│   ├── var_arith.c        # Phase 1 C test (int x = 5; putint(x + 3);)
│   ├── while.c            # Phase 2 C test (while loop)
│   ├── cond.c             # Phase 3 C test (if/else)
│   └── all.c              # Combined test
└── README.md              # Usage notes
```
