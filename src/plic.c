#include "plic.h"
#include "memmap.h"

void plic_init(PLIC *p)
{
    if (!p)
        return;
    *p = (PLIC){};
}

void plic_set_irq(PLIC *p, int irq, int level)
{
    if (!p || irq <= 0 || irq >= PLIC_NIRQ)
        return;
    if (level)
        p->pending |= (1u << irq);
    else
        p->pending &= ~(1u << irq);
}

static int best_irq(PLIC *p, int ctx)
{
    uint32_t en = p->enable[ctx];
    uint32_t pend = p->pending & en;
    int best = 0;
    uint32_t best_pri = p->threshold[ctx];
    for (int i = 1; i < PLIC_NIRQ; i++) {
        if ((pend & (1u << i)) && p->priority[i] > best_pri) {
            best_pri = p->priority[i];
            best = i;
        }
    }
    return best;
}

int plic_pending_ctx(PLIC *p, int ctx)
{
    if (!p || ctx < 0 || ctx > 1)
        return 0;
    return best_irq(p, ctx) != 0;
}

Trap plic_load(PLIC *p, uint64_t addr, uint64_t bits, uint64_t *out)
{
    (void)bits;
    uint64_t off = addr - PLIC_BASE;
    uint64_t v = 0;
    if (off < 0x1000) {
        uint32_t irq = (uint32_t)(off / 4);
        if (irq < PLIC_NIRQ)
            v = p->priority[irq];
    } else if (off >= 0x1000 && off < 0x2000) {
        v = p->pending;
    } else if (off >= 0x2000 && off < 0x2100) {
        int ctx = (int)((off - 0x2000) / 0x80);
        if (ctx == 0 || ctx == 1)
            v = p->enable[ctx];
    } else if (off == 0x200000) {
        v = p->threshold[0];
    } else if (off == 0x200004) {
        int irq = best_irq(p, 0);
        if (irq) {
            p->pending &= ~(1u << irq);
            p->claimed[0] = (uint32_t)irq;
        }
        v = (uint32_t)irq;
    } else if (off == 0x201000) {
        v = p->threshold[1];
    } else if (off == 0x201004) {
        int irq = best_irq(p, 1);
        if (irq) {
            p->pending &= ~(1u << irq);
            p->claimed[1] = (uint32_t)irq;
        }
        v = (uint32_t)irq;
    }
    *out = v;
    return trap_none();
}

Trap plic_store(PLIC *p, uint64_t addr, uint64_t bits, uint64_t value)
{
    (void)bits;
    uint64_t off = addr - PLIC_BASE;
    if (off < 0x1000) {
        uint32_t irq = (uint32_t)(off / 4);
        if (irq < PLIC_NIRQ)
            p->priority[irq] = (uint32_t)value & 7;
    } else if (off >= 0x2000 && off < 0x2100) {
        int ctx = (int)((off - 0x2000) / 0x80);
        if (ctx == 0 || ctx == 1)
            p->enable[ctx] = (uint32_t)value;
    } else if (off == 0x200000) {
        p->threshold[0] = (uint32_t)value;
    } else if (off == 0x200004) {
        p->claimed[0] = 0;
    } else if (off == 0x201000) {
        p->threshold[1] = (uint32_t)value;
    } else if (off == 0x201004) {
        p->claimed[1] = 0;
    }
    return trap_none();
}
