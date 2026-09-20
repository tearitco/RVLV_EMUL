#ifndef TRAP_H
#define TRAP_H

#include <stdint.h>

enum {
    EX_INST_MISALIGNED = 0,
    EX_INST_ACCESS     = 1,
    EX_ILLEGAL_INST    = 2,
    EX_BREAKPOINT      = 3,
    EX_LOAD_MISALIGNED = 4,
    EX_LOAD_ACCESS     = 5,
    EX_STORE_MISALIGNED= 6,
    EX_STORE_ACCESS    = 7,
    EX_ECALL_U         = 8,
    EX_ECALL_S         = 9,
    EX_ECALL_M         = 11,
    EX_INST_PAGE       = 12,
    EX_LOAD_PAGE       = 13,
    EX_STORE_PAGE      = 15,
};

enum {
    INT_SSIP = 1,
    INT_MSIP = 3,
    INT_STIP = 5,
    INT_MTIP = 7,
    INT_SEIP = 9,
    INT_MEIP = 11,
};

typedef struct Trap {
    int taken;
    int interrupt;
    uint64_t cause;
    uint64_t tval;
} Trap;

static inline Trap trap_none(void)
{
    return (Trap){};
}

static inline Trap trap_ex(uint64_t cause, uint64_t tval)
{
    return (Trap){ .taken = 1, .interrupt = 0, .cause = cause, .tval = tval };
}

static inline Trap trap_irq(uint64_t cause)
{
    return (Trap){ .taken = 1, .interrupt = 1, .cause = cause, .tval = 0 };
}

#endif
