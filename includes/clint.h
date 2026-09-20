#ifndef CLINT_H
#define CLINT_H

#include <stdint.h>
#include "trap.h"

typedef struct CLINT {
    uint64_t mtime;
    uint64_t mtimecmp;
    uint32_t msip;
} CLINT;

void clint_init(CLINT *c);
void clint_tick(CLINT *c, uint64_t n);
Trap clint_load(CLINT *c, uint64_t addr, uint64_t bits, uint64_t *out);
Trap clint_store(CLINT *c, uint64_t addr, uint64_t bits, uint64_t value);

#endif
