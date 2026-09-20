#include "cpu.h"

#include <inttypes.h>
#include <stdio.h>

void take_trap(CPU *cpu, Trap t)
{
    if (!t.taken)
        return;
    if (cpu->trace_trap && cpu->trap_prints < 40) {
        cpu->trap_prints++;
        fprintf(stderr, "trap %s cause=%" PRIu64 " pc=%" PRIx64 " tval=%" PRIx64 " priv=%u\n",
                t.interrupt ? "irq" : "ex", t.cause, cpu->pc, t.tval, cpu->priv);
    }
    uint64_t cause = t.cause;
    int dest = PRIV_M;
    if (cpu->priv != PRIV_M) {
        uint64_t deleg = t.interrupt ? cpu->csr[MIDELEG] : cpu->csr[MEDELEG];
        if ((deleg >> cause) & 1)
            dest = PRIV_S;
    }

    uint64_t epc = cpu->pc;
    uint64_t cause_w = t.interrupt ? (cause | (1ull << 63)) : cause;

    if (dest == PRIV_S) {
        cpu->csr[SEPC] = epc;
        cpu->csr[SCAUSE] = cause_w;
        cpu->csr[STVAL] = t.interrupt ? 0 : t.tval;
        uint64_t s = cpu->csr[MSTATUS];
        int sie = (int)((s >> 1) & 1);
        s = (s & ~MSTATUS_SPIE) | ((uint64_t)sie << 5);
        s &= ~MSTATUS_SIE;
        s = (s & ~MSTATUS_SPP) | ((uint64_t)(cpu->priv & 1) << 8);
        cpu->csr[MSTATUS] = s;
        cpu->priv = PRIV_S;
        uint64_t tvec = cpu->csr[STVEC];
        if (t.interrupt && (tvec & 1))
            cpu->pc = (tvec & ~3ull) + 4 * cause;
        else
            cpu->pc = tvec & ~3ull;
    } else {
        cpu->csr[MEPC] = epc;
        cpu->csr[MCAUSE] = cause_w;
        cpu->csr[MTVAL] = t.interrupt ? 0 : t.tval;
        uint64_t m = cpu->csr[MSTATUS];
        int mie = (int)((m >> 3) & 1);
        m = (m & ~MSTATUS_MPIE) | ((uint64_t)mie << 7);
        m &= ~MSTATUS_MIE;
        m = (m & ~MSTATUS_MPP) | ((uint64_t)cpu->priv << 11);
        cpu->csr[MSTATUS] = m;
        cpu->priv = PRIV_M;
        uint64_t tvec = cpu->csr[MTVEC];
        if (t.interrupt && (tvec & 1))
            cpu->pc = (tvec & ~3ull) + 4 * cause;
        else
            cpu->pc = tvec & ~3ull;
    }
    cpu->wfi = 0;
}

static int irq_enabled(CPU *cpu, uint64_t irqbit, int irq, bool for_wfi)
{
    uint64_t mie = cpu->csr[MIE];
    if ((mie & irqbit) == 0)
        return 0;
    int delegated = (cpu->csr[MIDELEG] >> irq) & 1;
    if (delegated) {
        if (cpu->priv > PRIV_S)
            return 0;
        if (!for_wfi && cpu->priv == PRIV_S && (cpu->csr[MSTATUS] & MSTATUS_SIE) == 0)
            return 0;
        return 1;
    }
    if (cpu->priv != PRIV_M)
        return 0;
    return (cpu->csr[MSTATUS] & MSTATUS_MIE) != 0;
}

Trap check_interrupts(CPU *cpu, bool for_wfi)
{
    if ((cpu->irq_div & 255ull) == 0)
        uart_poll(&cpu->bus.uart);
    if (cpu->bus.virtio.notify) {
        cpu->bus.virtio.notify = 0;
        virtio_disk_access(&cpu->bus.virtio, &cpu->bus.dram);
    }

    int uart_irq = uart_irq_pending(&cpu->bus.uart);
    int vio_irq = virtio_irq_pending(&cpu->bus.virtio);
    plic_set_irq(&cpu->bus.plic, UART_IRQ, uart_irq);
    plic_set_irq(&cpu->bus.plic, VIRTIO_IRQ, vio_irq);

    if (plic_pending_ctx(&cpu->bus.plic, 1))
        cpu->csr[MIP] |= MIP_SEIP;
    else
        cpu->csr[MIP] &= ~MIP_SEIP;
    if (plic_pending_ctx(&cpu->bus.plic, 0))
        cpu->csr[MIP] |= MIP_MEIP;
    else
        cpu->csr[MIP] &= ~MIP_MEIP;

    if (cpu->bus.clint.msip)
        cpu->csr[MIP] |= MIP_MSIP;
    else
        cpu->csr[MIP] &= ~MIP_MSIP;

    if (cpu->bus.clint.mtime >= cpu->bus.clint.mtimecmp)
        cpu->csr[MIP] |= MIP_MTIP;
    else
        cpu->csr[MIP] &= ~MIP_MTIP;

    if (cpu->csr[MENVCFG] & MENVCFG_STCE) {
        if (cpu->bus.clint.mtime >= cpu->csr[STIMECMP])
            cpu->csr[MIP] |= MIP_STIP;
        else
            cpu->csr[MIP] &= ~MIP_STIP;
    }

    // DEBUG: Print timer state
    static int timer_debug_count = 0;
    if (++timer_debug_count % 1000000 == 0) {
        printf("[DEBUG] mtime=%lu mtimecmp=%lu mip=%lx mie=%lx mideleg=%lx menvcfg=%lx priv=%d\n",
               cpu->bus.clint.mtime, cpu->bus.clint.mtimecmp,
               cpu->csr[MIP], cpu->csr[MIE], cpu->csr[MIDELEG], cpu->csr[MENVCFG], cpu->priv);
    }

    uint64_t pending = cpu->csr[MIP] & cpu->csr[MIE];
    if (pending == 0)
        return trap_none();

    struct {
        uint64_t bit;
        int irq;
    } order[] = {
        { MIP_MEIP, INT_MEIP },
        { MIP_MSIP, INT_MSIP },
        { MIP_MTIP, INT_MTIP },
        { MIP_SEIP, INT_SEIP },
        { MIP_SSIP, INT_SSIP },
        { MIP_STIP, INT_STIP },
    };
    for (unsigned i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
        if ((pending & order[i].bit) && irq_enabled(cpu, order[i].bit, order[i].irq, for_wfi))
            return trap_irq((uint64_t)order[i].irq);
    }
    return trap_none();
}
