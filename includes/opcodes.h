#ifndef OPCODES_H
#define OPCODES_H

#define LUI     0x37
#define AUIPC   0x17
#define JAL     0x6f
#define JALR    0x67
#define B_TYPE  0x63
#define LOAD    0x03
#define S_TYPE  0x23
#define I_TYPE  0x13
#define R_TYPE  0x33
#define FENCE   0x0f
#define I_TYPE_64 0x1b
#define R_TYPE_64 0x3b
#define SYSTEM  0x73
#define AMO     0x2f
#define LOAD_FP 0x07
#define STORE_FP 0x27
#define MADD    0x43
#define MSUB    0x47
#define NMSUB   0x4b
#define NMADD   0x4f
#define OP_FP   0x53

#endif
