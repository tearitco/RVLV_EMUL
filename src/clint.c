#include "clint.h"
#include "memmap.h"

void clint_init(CLINT *c)
{
    if (!c)
        return;
    *c = (CLINT){ .mtimecmp = ~0ULL };
}

void clint_tick(CLINT *c, uint64_t n)
{
    if (!c)
        return;
    c->mtime += n;
}

Trap clint_load(CLINT *c, uint64_t addr, uint64_t bits, uint64_t *out)
{
    uint64_t off = addr - CLINT_BASE;
    uint64_t v = 0;
    if (off == 0x0000) {
        v = c->msip;
        if (bits == 8)
            v &= 0xff;
        else if (bits == 16)
            v &= 0xffff;
        else if (bits == 32)
            v &= 0xffffffffull;
    } else if (off == 0x4000 || off == 0x4004) {
        if (off == 0x4000 && bits == 64)
            v = c->mtimecmp;
        else if (off == 0x4000)
            v = (uint32_t)c->mtimecmp;
        else
            v = (uint32_t)(c->mtimecmp >> 32);
    } else if (off == 0xbff8 || off == 0xbffc) {
        if (off == 0xbff8 && bits == 64)
            v = c->mtime;
        else if (off == 0xbff8)
            v = (uint32_t)c->mtime;
        else
            v = (uint32_t)(c->mtime >> 32);
    }
    *out = v;
    return trap_none();
}

Trap clint_store(CLINT *c, uint64_t addr, uint64_t bits, uint64_t value)
{
    uint64_t off = addr - CLINT_BASE;
    if (off == 0x0000) {
        c->msip = (uint32_t)(value & 1);
    } else if (off == 0x4000 && bits == 64) {
        c->mtimecmp = value;
    } else if (off == 0x4000) {
        c->mtimecmp = (c->mtimecmp & ~0xffffffffull) | (uint32_t)value;
    } else if (off == 0x4004) {
        c->mtimecmp = (c->mtimecmp & 0xffffffffull) | ((uint64_t)(uint32_t)value << 32);
    } else if (off == 0xbff8 && bits == 64) {
        c->mtime = value;
    } else if (off == 0xbff8) {
        c->mtime = (c->mtime & ~0xffffffffull) | (uint32_t)value;
    } else if (off == 0xbffc) {
        c->mtime = (c->mtime & 0xffffffffull) | ((uint64_t)(uint32_t)value << 32);
    }
    return trap_none();
}
