#!/usr/bin/env python3
"""Convert output.dec (decimal instruction words) to binary for rvemu."""
import struct
import sys

def main():
    input_file = sys.argv[1] if len(sys.argv) > 1 else "output.dec"
    output_file = sys.argv[2] if len(sys.argv) > 2 else "output.bin"
    
    with open(input_file) as f:
        values = [int(line.strip()) for line in f if line.strip()]
    
    with open(output_file, "wb") as f:
        for v in values:
            f.write(struct.pack("<I", v & 0xFFFFFFFF))
    
    print(f"Wrote {len(values)} instructions to {output_file} ({len(values)*4} bytes)")

if __name__ == "__main__":
    main()
