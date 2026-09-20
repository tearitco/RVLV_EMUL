#include "dram.h"

#include <stdlib.h>
#include <string.h>

int dram_init(DRAM *dram, uint64_t size)
{
    if (!dram || size == 0 || size > MAX_DRAM_MIB * 1024ULL * 1024ULL)
        return -1;
    dram->mem = calloc(1, (size_t)size);
    if (!dram->mem)
        return -1;
    dram->size = size;
    return 0;
}

void dram_free(DRAM *dram)
{
    if (!dram)
        return;
    free(dram->mem);
    dram->mem = nullptr;
    dram->size = 0;
}

int dram_in_range(const DRAM *dram, uint64_t addr, uint64_t nbytes)
{
    if (!dram || !dram->mem || nbytes == 0)
        return 0;
    if (addr < DRAM_BASE)
        return 0;
    uint64_t off = addr - DRAM_BASE;
    if (off >= dram->size)
        return 0;
    if (nbytes > dram->size - off)
        return 0;
    return 1;
}

static Trap dram_fault(int store)
{
    return trap_ex(store ? EX_STORE_ACCESS : EX_LOAD_ACCESS, 0);
}

Trap dram_load(DRAM *dram, uint64_t addr, uint64_t bits, uint64_t *out)
{
    uint64_t nbytes = bits / 8;
    if (!out || (bits != 8 && bits != 16 && bits != 32 && bits != 64))
        return dram_fault(0);
    if (!dram_in_range(dram, addr, nbytes))
        return dram_fault(0);
    uint64_t off = addr - DRAM_BASE;
    uint64_t v = 0;
    for (uint64_t i = 0; i < nbytes; i++)
        v |= (uint64_t)dram->mem[off + i] << (8 * i);
    *out = v;
    return trap_none();
}

Trap dram_store(DRAM *dram, uint64_t addr, uint64_t bits, uint64_t value)
{
    uint64_t nbytes = bits / 8;
    if (bits != 8 && bits != 16 && bits != 32 && bits != 64)
        return dram_fault(1);
    if (!dram_in_range(dram, addr, nbytes))
        return dram_fault(1);
    uint64_t off = addr - DRAM_BASE;
    for (uint64_t i = 0; i < nbytes; i++)
        dram->mem[off + i] = (uint8_t)((value >> (8 * i)) & 0xff);
    return trap_none();
}

int dram_copy_in(DRAM *dram, uint64_t addr, const uint8_t *src, uint64_t n)
{
    if (!src || n == 0)
        return 0;
    if (!dram_in_range(dram, addr, n))
        return -1;
    memcpy(dram->mem + (addr - DRAM_BASE), src, (size_t)n);
    return 0;
}

int dram_fill(DRAM *dram, uint64_t addr, uint8_t val, uint64_t n)
{
    if (n == 0)
        return 0;
    if (!dram_in_range(dram, addr, n))
        return -1;
    memset(dram->mem + (addr - DRAM_BASE), val, (size_t)n);
    return 0;
}
