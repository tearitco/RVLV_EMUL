#include "bus.h"

#include <string.h>

int bus_init(BUS *bus, uint64_t dram_size, const char *disk_path, int virtio_legacy)
{
    if (!bus)
        return -1;
    memset(bus, 0, sizeof(*bus));
    if (dram_init(&bus->dram, dram_size) < 0)
        return -1;
    uart_init(&bus->uart);
    clint_init(&bus->clint);
    plic_init(&bus->plic);
    if (virtio_init(&bus->virtio, disk_path, virtio_legacy) < 0) {
        dram_free(&bus->dram);
        return -1;
    }
    bus->test_exit = 0;
    bus->test_code = 0;
    return 0;
}

void bus_free(BUS *bus)
{
    if (!bus)
        return;
    virtio_free(&bus->virtio);
    dram_free(&bus->dram);
}

static int in_range(uint64_t addr, uint64_t base, uint64_t size)
{
    return addr >= base && addr < base + size;
}

Trap bus_load(BUS *bus, uint64_t addr, uint64_t bits, uint64_t *out)
{
    if (!bus || !out)
        return trap_ex(EX_LOAD_ACCESS, addr);
    if (in_range(addr, UART_BASE, UART_SIZE))
        return uart_load(&bus->uart, addr, bits, out);
    if (in_range(addr, CLINT_BASE, CLINT_SIZE))
        return clint_load(&bus->clint, addr, bits, out);
    if (in_range(addr, PLIC_BASE, PLIC_SIZE))
        return plic_load(&bus->plic, addr, bits, out);
    if (in_range(addr, VIRTIO_BASE, VIRTIO_SIZE))
        return virtio_load(&bus->virtio, addr, bits, out);
    if (in_range(addr, TEST_BASE, TEST_SIZE)) {
        *out = 0;
        return trap_none();
    }
    if (addr >= DRAM_BASE)
        return dram_load(&bus->dram, addr, bits, out);
    if (in_range(addr, MROM_BASE, MROM_SIZE)) {
        *out = 0;
        return trap_none();
    }
    return trap_ex(EX_LOAD_ACCESS, addr);
}

Trap bus_store(BUS *bus, uint64_t addr, uint64_t bits, uint64_t value)
{
    if (!bus)
        return trap_ex(EX_STORE_ACCESS, addr);
    if (in_range(addr, UART_BASE, UART_SIZE))
        return uart_store(&bus->uart, addr, bits, value);
    if (in_range(addr, CLINT_BASE, CLINT_SIZE))
        return clint_store(&bus->clint, addr, bits, value);
    if (in_range(addr, PLIC_BASE, PLIC_SIZE))
        return plic_store(&bus->plic, addr, bits, value);
    if (in_range(addr, VIRTIO_BASE, VIRTIO_SIZE))
        return virtio_store(&bus->virtio, addr, bits, value);
    if (in_range(addr, TEST_BASE, TEST_SIZE)) {
        uint32_t v = (uint32_t)value;
        uint16_t cmd = (uint16_t)(v & 0xffff);
        if (cmd == 0x5555) {
            bus->test_exit = 1;
            bus->test_code = (int)((v >> 16) & 0xffff);
        } else if (cmd == 0x3333) {
            bus->test_exit = 1;
            bus->test_code = 1;
        }
        return trap_none();
    }
    if (addr >= DRAM_BASE)
        return dram_store(&bus->dram, addr, bits, value);
    if (in_range(addr, MROM_BASE, MROM_SIZE))
        return trap_none();
    return trap_ex(EX_STORE_ACCESS, addr);
}
