#ifndef DRAM_H
#define DRAM_H

#include <stdint.h>
#include "memmap.h"
#include "trap.h"

typedef struct DRAM {
    uint8_t *mem;
    uint64_t size;
} DRAM;

int dram_init(DRAM *dram, uint64_t size);
void dram_free(DRAM *dram);
int dram_in_range(const DRAM *dram, uint64_t addr, uint64_t nbytes);
Trap dram_load(DRAM *dram, uint64_t addr, uint64_t bits, uint64_t *out);
Trap dram_store(DRAM *dram, uint64_t addr, uint64_t bits, uint64_t value);
int dram_copy_in(DRAM *dram, uint64_t addr, const uint8_t *src, uint64_t n);
int dram_fill(DRAM *dram, uint64_t addr, uint8_t val, uint64_t n);

#endif
