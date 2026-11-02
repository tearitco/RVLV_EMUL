# RISC-V RV64I Assembler in `.pal` (`asm.pal`)

`asm.pal` is a self-hosting RISC-V RV64I assembler written in prisc+x `.pal`
syntax. It encodes RISC-V machine instructions using only the prisc+x VM's
native R-type/I-type/S-type/B-type/U-type arithmetic, then wraps the result in
a valid ELF64 PT_LOAD binary loadable by rvemu.

## What it produces

- `output.elf` — ELF64-LSB RISC-V executable at `0x80000000`, single PT_LOAD
  segment (PF_R | PF_X), entry point `0x80000000`.

## How it works

The assembler runs entirely inside the prisc+x VM. It uses:

- `li`, `slli`, `add`, `addi` to build each 32-bit instruction word via
  bit-field arithmetic (no hex literals — `sscanf %d` doesn't parse `0x`)
- `sw` to store each encoded word into `mem[]`
- `SYS_OPEN`/`SYS_WRITE_MEM`/`SYS_CLOSE` syscalls to write the ELF64 file
- `SYS_EXIT` to halt the VM

## Encoding coverage

| Format | Instructions | Status |
|--------|-------------|--------|
| R-type | add, sub | ✅ |
| I-type | addi, andi | ✅ |
| S-type | sw | ✅ |
| B-type | beq | ✅ |
| U-type | lui | ✅ |
| RV64I | ld, sd, addiw | ✅ |

## Build & run

```bash
# Build rvemu (if not already built)
./build.sh

# Run the assembler inside prisc+x VM
./prisc_x asm.pal

# Verify output is a valid ELF64 RISC-V binary
file output.elf
# -> ELF 64-bit LSB executable, UCB RISC-V

# Run on rvemu
/tmp/rvemu -k output.elf --max-inst 1000
echo $?
# -> 1 (exits past the final instruction; expected)
```

## Known constraint

All immediate values must be decimal (e.g. `1179403647` not `0x464C457F`) because
prisc+x's `sscanf("%d")` does not parse `0x` hex prefixes. Each instruction's
encoding is documented inline as a comment with its hex value.
