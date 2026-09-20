#ifndef BUS_H
#define BUS_H

#include "dram.h"
#include "uart.h"
#include "clint.h"
#include "plic.h"
#include "virtio.h"

typedef struct BUS {
    DRAM dram;
    UART uart;
    CLINT clint;
    PLIC plic;
    VIRTIO virtio;
    int test_exit;
    int test_code;
} BUS;

int bus_init(BUS *bus, uint64_t dram_size, const char *disk_path, int virtio_legacy);
void bus_free(BUS *bus);
Trap bus_load(BUS *bus, uint64_t addr, uint64_t bits, uint64_t *out);
Trap bus_store(BUS *bus, uint64_t addr, uint64_t bits, uint64_t value);

#endif
