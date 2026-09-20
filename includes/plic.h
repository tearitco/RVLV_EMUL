#ifndef PLIC_H
#define PLIC_H

#include <stdint.h>
#include "trap.h"

#define PLIC_NIRQ 32

typedef struct PLIC {
    uint32_t priority[PLIC_NIRQ];
    uint32_t pending;
    uint32_t enable[2];
    uint32_t threshold[2];
    uint32_t claimed[2];
} PLIC;

void plic_init(PLIC *p);
void plic_set_irq(PLIC *p, int irq, int level);
int plic_pending_ctx(PLIC *p, int ctx);
Trap plic_load(PLIC *p, uint64_t addr, uint64_t bits, uint64_t *out);
Trap plic_store(PLIC *p, uint64_t addr, uint64_t bits, uint64_t value);

#endif
