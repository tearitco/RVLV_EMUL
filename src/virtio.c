#include "virtio.h"
#include "dram.h"
#include "memmap.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VIRTIO_MAGIC_VAL 0x74726976u
#define VIRTIO_VENDOR    0x554d4551u
#define DESC_MAX         256
#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2
#define VIRTIO_BLK_T_IN    0
#define VIRTIO_BLK_T_OUT   1
#define VIRTIO_BLK_T_FLUSH 4

#define VIRTIO_BLK_F_SIZE_MAX     1
#define VIRTIO_BLK_F_SEG_MAX      2
#define VIRTIO_BLK_F_GEOMETRY     4
#define VIRTIO_BLK_F_RO           5
#define VIRTIO_BLK_F_BLK_SIZE     6
#define VIRTIO_BLK_F_TOPOLOGY     10
#define VIRTIO_BLK_F_CONFIG_WCE   11
#define VIRTIO_F_VERSION_1        32

int virtio_init(VIRTIO *v, const char *disk_path, int legacy)
{
    if (!v)
        return -1;
    *v = (VIRTIO){ .legacy = legacy, .present = 0, .guest_page_size = 4096 };
    if (!disk_path)
        return 0;
    FILE *f = fopen(disk_path, "rb");
    if (!f)
        return -1;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return -1;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    uint64_t size = (uint64_t)sz;
    if (size == 0) {
        fclose(f);
        return -1;
    }
    uint8_t *buf = malloc((size_t)size);
    if (!buf) {
        fclose(f);
        return -1;
    }
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        free(buf);
        fclose(f);
        return -1;
    }
    fclose(f);
    v->disk = buf;
    v->disk_size = size;
    v->present = 1;
    return 0;
}

void virtio_free(VIRTIO *v)
{
    if (!v)
        return;
    free(v->disk);
    v->disk = nullptr;
    v->disk_size = 0;
}

int virtio_irq_pending(VIRTIO *v)
{
    return v && v->interrupt_status != 0;
}

static uint64_t desc_base(const VIRTIO *v)
{
    if (v->legacy)
        return (uint64_t)v->queue_pfn * (uint64_t)v->guest_page_size;
    return v->queue_desc;
}

static uint64_t avail_base(const VIRTIO *v)
{
    if (v->legacy)
        return desc_base(v) + 16ull * (v->queue_num ? v->queue_num : 8);
    return v->queue_driver;
}

static uint64_t used_base(const VIRTIO *v)
{
    if (v->legacy)
        return desc_base(v) + 4096;
    return v->queue_device;
}

static int dma_load(DRAM *dram, uint64_t addr, uint64_t bits, uint64_t *out)
{
    Trap t = dram_load(dram, addr, bits, out);
    return t.taken ? -1 : 0;
}

static int dma_store(DRAM *dram, uint64_t addr, uint64_t bits, uint64_t val)
{
    Trap t = dram_store(dram, addr, bits, val);
    return t.taken ? -1 : 0;
}

static int dma_copy_to_disk(VIRTIO *v, DRAM *dram, uint64_t src, uint64_t dst, uint32_t len)
{
    if ((uint64_t)len > v->disk_size || dst > v->disk_size - len)
        return -1;
    if (!dram_in_range(dram, src, len))
        return -1;
    memcpy(v->disk + dst, dram->mem + (src - DRAM_BASE), len);
    return 0;
}

static int dma_copy_from_disk(VIRTIO *v, DRAM *dram, uint64_t dst, uint64_t src, uint32_t len)
{
    if ((uint64_t)len > v->disk_size || src > v->disk_size - len)
        return -1;
    if (!dram_in_range(dram, dst, len))
        return -1;
    memcpy(dram->mem + (dst - DRAM_BASE), v->disk + src, len);
    return 0;
}

void virtio_disk_access(VIRTIO *v, DRAM *dram)
{
    if (!v || !v->present || !v->disk || !dram)
        return;
    uint32_t qn = v->queue_num ? v->queue_num : 8;
    if (qn > DESC_MAX)
        qn = DESC_MAX;
    uint64_t dbase = desc_base(v);
    uint64_t abase = avail_base(v);
    uint64_t ubase = used_base(v);
    uint64_t avail_idx_u = 0;
    if (dma_load(dram, abase + 2, 16, &avail_idx_u) < 0)
        return;
    uint16_t avail_idx = (uint16_t)avail_idx_u;
    int nproc = 0;
    while (v->last_avail_idx != avail_idx && nproc < (int)qn) {
        uint64_t ring_off = 4ull + (uint64_t)(v->last_avail_idx % (uint16_t)qn) * 2ull;
        uint64_t head_u = 0;
        if (dma_load(dram, abase + ring_off, 16, &head_u) < 0)
            return;
        uint16_t head = (uint16_t)head_u;
        if (head >= qn)
            return;

        uint64_t d0 = dbase + 16ull * head;
        uint64_t addr0 = 0, len0 = 0, flags0 = 0, next0 = 0;
        if (dma_load(dram, d0, 64, &addr0) < 0)
            return;
        if (dma_load(dram, d0 + 8, 32, &len0) < 0)
            return;
        if (dma_load(dram, d0 + 12, 16, &flags0) < 0)
            return;
        if (dma_load(dram, d0 + 14, 16, &next0) < 0)
            return;

        uint32_t type = 0;
        uint64_t sector = 0;
        uint64_t tmp = 0;
        if (dma_load(dram, addr0, 32, &tmp) < 0)
            return;
        type = (uint32_t)tmp;
        if (dma_load(dram, addr0 + 8, 64, &sector) < 0)
            return;

        uint16_t idx = (uint16_t)next0;
        uint64_t written = 0;
        uint8_t status = 0;
        int hops = 0;
        if (type == VIRTIO_BLK_T_FLUSH) {
            status = 0;
        } else if (!(flags0 & VRING_DESC_F_NEXT)) {
            status = 1;
        } else {
            while (hops++ < (int)qn) {
                uint64_t d = dbase + 16ull * idx;
                uint64_t addr = 0, len = 0, flags = 0, next = 0;
                if (dma_load(dram, d, 64, &addr) < 0) {
                    status = 1;
                    break;
                }
                if (dma_load(dram, d + 8, 32, &len) < 0) {
                    status = 1;
                    break;
                }
                if (dma_load(dram, d + 12, 16, &flags) < 0) {
                    status = 1;
                    break;
                }
                if (dma_load(dram, d + 14, 16, &next) < 0) {
                    status = 1;
                    break;
                }
                int is_last = !(flags & VRING_DESC_F_NEXT);
                if (is_last && (flags & VRING_DESC_F_WRITE) && (uint32_t)len == 1) {
                    if (dram_store(dram, addr, 8, status).taken)
                        status = 1;
                    else if (dma_store(dram, addr, 8, status) < 0)
                        status = 1;
                    break;
                }
                uint64_t disk_off = sector * 512ull + written;
                if (flags & VRING_DESC_F_WRITE) {
                    if (dma_copy_from_disk(v, dram, addr, disk_off, (uint32_t)len) < 0)
                        status = 1;
                } else {
                    if (dma_copy_to_disk(v, dram, addr, disk_off, (uint32_t)len) < 0)
                        status = 1;
                }
                written += (uint32_t)len;
                if (is_last)
                    break;
                idx = (uint16_t)next;
                if (idx >= qn) {
                    status = 1;
                    break;
                }
            }
        }

        uint64_t used_slot = ubase + 4ull + (uint64_t)(v->used_idx % (uint16_t)qn) * 8ull;
        (void)dma_store(dram, used_slot, 32, head);
        (void)dma_store(dram, used_slot + 4, 32, (uint32_t)written);
        v->used_idx++;
        (void)dma_store(dram, ubase + 2, 16, v->used_idx);
        v->last_avail_idx++;
        nproc++;
    }
    v->interrupt_status |= 1;
}

Trap virtio_load(VIRTIO *v, uint64_t addr, uint64_t bits, uint64_t *out)
{
    (void)bits;
    uint64_t off = addr - VIRTIO_BASE;
    uint64_t val = 0;
    uint64_t sectors = v->disk_size / 512ull;
    switch (off) {
    case 0x000:
        val = VIRTIO_MAGIC_VAL;
        break;
    case 0x004:
        val = v->legacy ? 1 : 2;
        break;
    case 0x008:
        val = v->present ? 2 : 0;
        break;
    case 0x00c:
        val = VIRTIO_VENDOR;
        break;
    case 0x010:
        if (v->device_features_sel == 0) {
            val = (1u << VIRTIO_BLK_F_SIZE_MAX) |
                  (1u << VIRTIO_BLK_F_SEG_MAX) |
                  (1u << VIRTIO_BLK_F_GEOMETRY) |
                  (1u << VIRTIO_BLK_F_RO) |
                  (1u << VIRTIO_BLK_F_BLK_SIZE) |
                  (1u << VIRTIO_BLK_F_TOPOLOGY) |
                  (1u << VIRTIO_BLK_F_CONFIG_WCE);
        } else if (v->device_features_sel == 1) {
            val = (1u << (VIRTIO_F_VERSION_1 - 32));
        } else {
            val = 0;
        }
        break;
    case 0x034:
        val = 8;
        break;
    case 0x040:
        val = v->queue_pfn;
        break;
    case 0x044:
        val = v->queue_ready;
        break;
    case 0x060:
        val = v->interrupt_status;
        break;
    case 0x070:
        val = v->status;
        break;
    case 0x100:
        val = (uint32_t)sectors;
        break;
    case 0x104:
        val = (uint32_t)(sectors >> 32);
        break;
    default:
        val = 0;
        break;
    }
    *out = val;
    return trap_none();
}

Trap virtio_store(VIRTIO *v, uint64_t addr, uint64_t bits, uint64_t value)
{
    (void)bits;
    uint64_t off = addr - VIRTIO_BASE;
    uint32_t val = (uint32_t)value;
    switch (off) {
    case 0x014:
        v->device_features_sel = val;
        break;
    case 0x020:
        v->driver_features = val;
        break;
    case 0x024:
        v->driver_features_sel = val;
        break;
    case 0x028:
        v->guest_page_size = val ? val : 4096;
        break;
    case 0x030:
        v->queue_sel = val;
        break;
    case 0x038:
        v->queue_num = val;
        break;
    case 0x040:
        v->queue_pfn = val;
        break;
    case 0x044:
        v->queue_ready = val;
        if (val == 0) {
            v->last_avail_idx = 0;
            v->used_idx = 0;
        }
        break;
    case 0x050:
        v->notify = 1;
        break;
    case 0x064:
        v->interrupt_status &= ~val;
        break;
    case 0x070:
        v->status = val;
        if (val == 0) {
            v->queue_ready = 0;
            v->queue_pfn = 0;
            v->interrupt_status = 0;
            v->last_avail_idx = 0;
            v->used_idx = 0;
            v->notify = 0;
        }
        break;
    case 0x080:
        v->queue_desc = (v->queue_desc & ~0xffffffffull) | val;
        break;
    case 0x084:
        v->queue_desc = (v->queue_desc & 0xffffffffull) | ((uint64_t)val << 32);
        break;
    case 0x090:
        v->queue_driver = (v->queue_driver & ~0xffffffffull) | val;
        break;
    case 0x094:
        v->queue_driver = (v->queue_driver & 0xffffffffull) | ((uint64_t)val << 32);
        break;
    case 0x0a0:
        v->queue_device = (v->queue_device & ~0xffffffffull) | val;
        break;
    case 0x0a4:
        v->queue_device = (v->queue_device & 0xffffffffull) | ((uint64_t)val << 32);
        break;
    default:
        break;
    }
    return trap_none();
}
