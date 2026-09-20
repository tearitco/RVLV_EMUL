#include "cpu.h"

#include <string.h>

static int pmp_cfg(const CPU *cpu, int i)
{
    uint64_t cfgcsr = cpu->csr[PMPCFG0 + (i / 8) * 2];
    return (int)((cfgcsr >> ((i % 8) * 8)) & 0xff);
}

static uint64_t pmp_addr_decode(uint64_t pmpaddr, int a, uint64_t *lo, uint64_t *hi)
{
    if (a == 1) { /* TOR handled by caller */
        *lo = 0;
        *hi = pmpaddr << 2;
        return 0;
    }
    if (a == 2) { /* NA4 */
        *lo = pmpaddr << 2;
        *hi = *lo + 4;
        return 0;
    }
    if (a == 3) { /* NAPOT */
        uint64_t x = pmpaddr;
        int bits = 0;
        while (bits < 64 && (x & 1)) {
            x >>= 1;
            bits++;
        }
        uint64_t size = 8ull << bits;
        uint64_t base = (pmpaddr & ~((1ull << bits) - 1)) << 2;
        *lo = base;
        *hi = base + size;
        return 0;
    }
    *lo = 0;
    *hi = 0;
    return 0;
}

int cpu_pmp_ok(CPU *cpu, uint64_t addr, uint64_t nbytes, int access, int priv)
{
    if (cpu->csr[PMPCFG0] == 0x0f && cpu->csr[PMPADDR0] == 0x3fffffffffffffffull)
        return 1;
    int any = 0;
    uint64_t prev = 0;
    for (int i = 0; i < 16; i++) {
        int cfg = pmp_cfg(cpu, i);
        int a = (cfg >> 3) & 3;
        if (a == 0)
            continue;
        any = 1;
        uint64_t lo = 0, hi = 0;
        uint64_t pmpaddr = cpu->csr[PMPADDR0 + i];
        if (a == 1) {
            lo = prev;
            hi = pmpaddr << 2;
        } else {
            pmp_addr_decode(pmpaddr, a, &lo, &hi);
        }
        prev = pmpaddr << 2;
        int locked = cfg & 0x80;
        if (addr >= lo && addr + (nbytes ? nbytes : 1) - 1 < hi) {
            int need = 0;
            if (access == ACC_FETCH)
                need = 4;
            else if (access == ACC_LOAD)
                need = 1;
            else
                need = 2;
            if (priv == PRIV_M && !locked)
                return 1;
            return (cfg & need) != 0;
        }
    }
    if (!any)
        return 1;
    return priv == PRIV_M;
}

static Trap page_fault(int access, uint64_t va)
{
    if (access == ACC_FETCH)
        return trap_ex(EX_INST_PAGE, va);
    if (access == ACC_LOAD)
        return trap_ex(EX_LOAD_PAGE, va);
    return trap_ex(EX_STORE_PAGE, va);
}

static Trap access_fault(int access, uint64_t va)
{
    if (access == ACC_FETCH)
        return trap_ex(EX_INST_ACCESS, va);
    if (access == ACC_LOAD)
        return trap_ex(EX_LOAD_ACCESS, va);
    return trap_ex(EX_STORE_ACCESS, va);
}

Trap cpu_mmu_translate(CPU *cpu, uint64_t va, int access, uint64_t *pa)
{
    int priv = cpu->priv;
    if (access != ACC_FETCH && (cpu->csr[MSTATUS] & MSTATUS_MPRV))
        priv = (int)((cpu->csr[MSTATUS] >> 11) & 3);

    if (priv == PRIV_M) {
        *pa = va;
        if (!cpu_pmp_ok(cpu, va, 1, access, PRIV_M))
            return access_fault(access, va);
        return trap_none();
    }

    uint64_t satp = cpu->csr[SATP];
    uint64_t mode = satp >> 60;
    if (mode == 0) {
        *pa = va;
        if (!cpu_pmp_ok(cpu, va, 1, access, priv))
            return access_fault(access, va);
        return trap_none();
    }
    if (mode != SATP_MODE_SV39)
        return page_fault(access, va);

    uint64_t sign = va & (1ull << 38);
    uint64_t high = va >> 39;
    if ((sign && high != 0x1ffffffull) || (!sign && high != 0))
        return page_fault(access, va);

    uint64_t vpn[3] = {
        (va >> 12) & 0x1ff,
        (va >> 21) & 0x1ff,
        (va >> 30) & 0x1ff,
    };
    uint64_t a = (satp & ((1ull << 44) - 1)) * PAGE_SIZE;
    uint64_t pte = 0;
    int level = 2;
    for (;;) {
        uint64_t pte_addr = a + vpn[level] * 8;
        if (!cpu_pmp_ok(cpu, pte_addr, 8, ACC_LOAD, PRIV_S))
            return access_fault(access, va);
        Trap t = bus_load(&cpu->bus, pte_addr, 64, &pte);
        if (t.taken)
            return access_fault(access, va);
        int v = (int)(pte & 1);
        int r = (int)((pte >> 1) & 1);
        int w = (int)((pte >> 2) & 1);
        int x = (int)((pte >> 3) & 1);
        if (!v || (w && !r))
            return page_fault(access, va);
        if (r || x)
            break;
        level--;
        if (level < 0)
            return page_fault(access, va);
        a = ((pte >> 10) & 0x0fffffffffffull) * PAGE_SIZE;
    }

    int r = (int)((pte >> 1) & 1);
    int w = (int)((pte >> 2) & 1);
    int x = (int)((pte >> 3) & 1);
    int u = (int)((pte >> 4) & 1);
    int mxr = (cpu->csr[MSTATUS] & MSTATUS_MXR) != 0;
    int sum = (cpu->csr[MSTATUS] & MSTATUS_SUM) != 0;

    if (priv == PRIV_U && !u)
        return page_fault(access, va);
    if (priv == PRIV_S && u && !sum)
        return page_fault(access, va);

    if (access == ACC_FETCH && !x)
        return page_fault(access, va);
    if (access == ACC_LOAD && !r && !(mxr && x))
        return page_fault(access, va);
    if (access == ACC_STORE && !w)
        return page_fault(access, va);

    if (level > 0) {
        uint64_t mask = (1ull << (9 * (unsigned)level)) - 1;
        if (((pte >> 10) & mask) != 0)
            return page_fault(access, va);
    }

    int a_bit = (int)((pte >> 6) & 1);
    int d_bit = (int)((pte >> 7) & 1);
    int need_update = 0;
    if (!a_bit) {
        pte |= (1ull << 6);
        need_update = 1;
    }
    if (access == ACC_STORE && !d_bit) {
        pte |= (1ull << 7);
        need_update = 1;
    }
    if (need_update) {
        uint64_t pte_addr = a + vpn[level] * 8;
        (void)bus_store(&cpu->bus, pte_addr, 64, pte);
    }

    uint64_t ppn = (pte >> 10) & 0x0fffffffffffull;
    uint64_t pa_out;
    if (level == 0)
        pa_out = (ppn << 12) | (va & 0xfff);
    else if (level == 1)
        pa_out = (ppn << 12) | (va & 0x1fffff);
    else
        pa_out = (ppn << 12) | (va & 0x3fffffff);

    if (!cpu_pmp_ok(cpu, pa_out, 1, access, priv))
        return access_fault(access, va);
    *pa = pa_out;
    return trap_none();
}

static int bare_mode(const CPU *cpu, int access)
{
    if (cpu->priv == PRIV_M && (access == ACC_FETCH || (cpu->csr[MSTATUS] & MSTATUS_MPRV) == 0))
        return 1;
    if ((cpu->csr[SATP] >> 60) == 0 &&
        (access == ACC_FETCH || (cpu->csr[MSTATUS] & MSTATUS_MPRV) == 0 ||
         ((cpu->csr[MSTATUS] >> 11) & 3) == PRIV_M))
        return 1;
    return 0;
}

Trap cpu_load(CPU *cpu, uint64_t addr, uint64_t bits, uint64_t *out)
{
    if (bare_mode(cpu, ACC_LOAD) && dram_in_range(&cpu->bus.dram, addr, bits / 8))
        return dram_load(&cpu->bus.dram, addr, bits, out);
    uint64_t pa = 0;
    Trap t = cpu_mmu_translate(cpu, addr, ACC_LOAD, &pa);
    if (t.taken) {
        t.tval = addr;
        return t;
    }
    t = bus_load(&cpu->bus, pa, bits, out);
    if (t.taken)
        t.tval = addr;
    return t;
}

Trap cpu_store(CPU *cpu, uint64_t addr, uint64_t bits, uint64_t value)
{
    cpu->reservation_valid = 0;
    if (bare_mode(cpu, ACC_STORE) && dram_in_range(&cpu->bus.dram, addr, bits / 8))
        return dram_store(&cpu->bus.dram, addr, bits, value);
    uint64_t pa = 0;
    Trap t = cpu_mmu_translate(cpu, addr, ACC_STORE, &pa);
    if (t.taken) {
        t.tval = addr;
        return t;
    }
    t = bus_store(&cpu->bus, pa, bits, value);
    if (t.taken)
        t.tval = addr;
    return t;
}
