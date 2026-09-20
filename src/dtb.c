#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FDT_MAGIC      0xd00dfeedu
#define FDT_BEGIN_NODE 0x1u
#define FDT_END_NODE   0x2u
#define FDT_PROP       0x3u
#define FDT_END        0x9u

typedef struct {
    uint8_t *buf;
    uint32_t cap;
    uint32_t struct_off;
    uint32_t strings_off;
    uint32_t struct_len;
    uint32_t strings_len;
    uint8_t *str;
    uint32_t str_cap;
} Fdt;

static uint32_t be32(uint32_t x)
{
    return ((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x >> 8) & 0xff00) | (x >> 24);
}

static int fdt_grow(Fdt *f, uint32_t need)
{
    if (f->struct_len + need <= f->cap)
        return 0;
    uint32_t ncap = f->cap ? f->cap : 4096;
    while (f->struct_len + need > ncap)
        ncap *= 2;
    uint8_t *n = realloc(f->buf, ncap);
    if (!n)
        return -1;
    f->buf = n;
    f->cap = ncap;
    return 0;
}

static int fdt_s32(Fdt *f, uint32_t v)
{
    if (fdt_grow(f, 4) < 0)
        return -1;
    uint32_t b = be32(v);
    memcpy(f->buf + f->struct_len, &b, 4);
    f->struct_len += 4;
    return 0;
}

static int fdt_bytes(Fdt *f, const void *p, uint32_t n)
{
    uint32_t pad = (n + 3) & ~3u;
    if (fdt_grow(f, pad) < 0)
        return -1;
    memcpy(f->buf + f->struct_len, p, n);
    memset(f->buf + f->struct_len + n, 0, pad - n);
    f->struct_len += pad;
    return 0;
}

static int str_add(Fdt *f, const char *s, uint32_t *off)
{
    uint32_t n = (uint32_t)strlen(s) + 1;
    for (uint32_t i = 0; i < f->strings_len;) {
        if (strcmp((char *)f->str + i, s) == 0) {
            *off = i;
            return 0;
        }
        i += (uint32_t)strlen((char *)f->str + i) + 1;
    }
    if (f->strings_len + n > f->str_cap) {
        uint32_t ncap = f->str_cap ? f->str_cap * 2 : 1024;
        while (f->strings_len + n > ncap)
            ncap *= 2;
        uint8_t *nb = realloc(f->str, ncap);
        if (!nb)
            return -1;
        f->str = nb;
        f->str_cap = ncap;
    }
    memcpy(f->str + f->strings_len, s, n);
    *off = f->strings_len;
    f->strings_len += n;
    return 0;
}

static int node_begin(Fdt *f, const char *name)
{
    if (fdt_s32(f, FDT_BEGIN_NODE) < 0)
        return -1;
    return fdt_bytes(f, name, (uint32_t)strlen(name) + 1);
}

static int node_end(Fdt *f)
{
    return fdt_s32(f, FDT_END_NODE);
}

static int prop(Fdt *f, const char *name, const void *val, uint32_t len)
{
    uint32_t off;
    if (str_add(f, name, &off) < 0)
        return -1;
    if (fdt_s32(f, FDT_PROP) < 0)
        return -1;
    if (fdt_s32(f, len) < 0)
        return -1;
    if (fdt_s32(f, off) < 0)
        return -1;
    return fdt_bytes(f, val, len);
}

static int prop_u32(Fdt *f, const char *name, uint32_t v)
{
    uint32_t b = be32(v);
    return prop(f, name, &b, 4);
}

static int prop_u32s(Fdt *f, const char *name, const uint32_t *v, int n)
{
    uint32_t tmp[16];
    if (n > 16)
        return -1;
    for (int i = 0; i < n; i++)
        tmp[i] = be32(v[i]);
    return prop(f, name, tmp, (uint32_t)n * 4);
}

static int prop_str(Fdt *f, const char *name, const char *s)
{
    return prop(f, name, s, (uint32_t)strlen(s) + 1);
}

static int prop_empty(Fdt *f, const char *name)
{
    return prop(f, name, "", 0);
}

int generate_dtb(CPU *cpu, uint64_t dram_size, uint64_t initrd_start, uint64_t initrd_end,
                 const char *bootargs, uint64_t dest, uint64_t *dtb_size)
{
    Fdt f = {};
    if (node_begin(&f, "") < 0)
        goto fail;
    if (prop_u32(&f, "#address-cells", 2) < 0)
        goto fail;
    if (prop_u32(&f, "#size-cells", 2) < 0)
        goto fail;
    if (prop_str(&f, "compatible", "riscv-virtio,qemu") < 0)
        goto fail;
    if (prop_str(&f, "model", "riscv-virtio,emulator") < 0)
        goto fail;

    if (node_begin(&f, "chosen") < 0)
        goto fail;
    if (prop_str(&f, "bootargs", bootargs ? bootargs : "console=ttyS0 earlycon") < 0)
        goto fail;
    if (prop_str(&f, "stdout-path", "/soc/uart@10000000") < 0)
        goto fail;
    if (initrd_end > initrd_start) {
        uint32_t s[2] = { (uint32_t)(initrd_start >> 32), (uint32_t)initrd_start };
        uint32_t e[2] = { (uint32_t)(initrd_end >> 32), (uint32_t)initrd_end };
        if (prop_u32s(&f, "linux,initrd-start", s, 2) < 0)
            goto fail;
        if (prop_u32s(&f, "linux,initrd-end", e, 2) < 0)
            goto fail;
    }
    if (node_end(&f) < 0)
        goto fail;

    char memname[64];
    snprintf(memname, sizeof(memname), "memory@%llx", (unsigned long long)DRAM_BASE);
    if (node_begin(&f, memname) < 0)
        goto fail;
    if (prop_str(&f, "device_type", "memory") < 0)
        goto fail;
    uint32_t memreg[4] = {
        (uint32_t)(DRAM_BASE >> 32), (uint32_t)DRAM_BASE,
        (uint32_t)(dram_size >> 32), (uint32_t)dram_size
    };
    if (prop_u32s(&f, "reg", memreg, 4) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;

    if (node_begin(&f, "cpus") < 0)
        goto fail;
    if (prop_u32(&f, "#address-cells", 1) < 0)
        goto fail;
    if (prop_u32(&f, "#size-cells", 0) < 0)
        goto fail;
    if (prop_u32(&f, "timebase-frequency", 10000000) < 0)
        goto fail;
    if (node_begin(&f, "cpu@0") < 0)
        goto fail;
    if (prop_str(&f, "device_type", "cpu") < 0)
        goto fail;
    if (prop_u32(&f, "reg", 0) < 0)
        goto fail;
    if (prop_str(&f, "status", "okay") < 0)
        goto fail;
    if (prop_str(&f, "compatible", "riscv") < 0)
        goto fail;
    if (prop_str(&f, "riscv,isa", "rv64imafdc") < 0)
        goto fail;
    if (prop_str(&f, "mmu-type", "riscv,sv39") < 0)
        goto fail;
    if (node_begin(&f, "interrupt-controller") < 0)
        goto fail;
    if (prop_empty(&f, "interrupt-controller") < 0)
        goto fail;
    if (prop_u32(&f, "#interrupt-cells", 1) < 0)
        goto fail;
    if (prop_str(&f, "compatible", "riscv,cpu-intc") < 0)
        goto fail;
    uint32_t intc_ph = 1;
    if (prop_u32(&f, "phandle", intc_ph) < 0)
        goto fail;
    if (prop_u32(&f, "linux,phandle", intc_ph) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;

    if (node_begin(&f, "soc") < 0)
        goto fail;
    if (prop_u32(&f, "#address-cells", 2) < 0)
        goto fail;
    if (prop_u32(&f, "#size-cells", 2) < 0)
        goto fail;
    if (prop_str(&f, "compatible", "simple-bus") < 0)
        goto fail;
    if (prop_empty(&f, "ranges") < 0)
        goto fail;

    uint32_t plic_ph = 2;
    if (node_begin(&f, "clint@2000000") < 0)
        goto fail;
    if (prop_str(&f, "compatible", "sifive,clint0") < 0)
        goto fail;
    uint32_t clint_reg[4] = { 0, 0x2000000, 0, 0x10000 };
    if (prop_u32s(&f, "reg", clint_reg, 4) < 0)
        goto fail;
    uint32_t clint_irq[4] = { intc_ph, 3, intc_ph, 7 };
    if (prop_u32s(&f, "interrupts-extended", clint_irq, 4) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;

    if (node_begin(&f, "plic@c000000") < 0)
        goto fail;
    if (prop_str(&f, "compatible", "sifive,plic-1.0.0") < 0)
        goto fail;
    if (prop_empty(&f, "interrupt-controller") < 0)
        goto fail;
    if (prop_u32(&f, "#interrupt-cells", 1) < 0)
        goto fail;
    if (prop_u32(&f, "riscv,ndev", 0x35) < 0)
        goto fail;
    uint32_t plic_reg[4] = { 0, 0x0c000000, 0, 0x4000000 };
    if (prop_u32s(&f, "reg", plic_reg, 4) < 0)
        goto fail;
    uint32_t plic_irq[4] = { intc_ph, 11, intc_ph, 9 };
    if (prop_u32s(&f, "interrupts-extended", plic_irq, 4) < 0)
        goto fail;
    if (prop_u32(&f, "phandle", plic_ph) < 0)
        goto fail;
    if (prop_u32(&f, "linux,phandle", plic_ph) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;

    if (node_begin(&f, "uart@10000000") < 0)
        goto fail;
    if (prop_str(&f, "compatible", "ns16550a") < 0)
        goto fail;
    uint32_t uart_reg[4] = { 0, 0x10000000, 0, 0x100 };
    if (prop_u32s(&f, "reg", uart_reg, 4) < 0)
        goto fail;
    if (prop_u32(&f, "interrupt-parent", plic_ph) < 0)
        goto fail;
    if (prop_u32(&f, "interrupts", UART_IRQ) < 0)
        goto fail;
    if (prop_u32(&f, "clock-frequency", 3686400) < 0)
        goto fail;
    if (prop_u32(&f, "reg-shift", 0) < 0)
        goto fail;
    if (prop_u32(&f, "reg-io-width", 1) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;

    if (cpu->bus.virtio.present) {
        if (node_begin(&f, "virtio_mmio@10001000") < 0)
            goto fail;
        if (prop_str(&f, "compatible", "virtio,mmio") < 0)
            goto fail;
        uint32_t vio_reg[4] = { 0, 0x10001000, 0, 0x1000 };
        if (prop_u32s(&f, "reg", vio_reg, 4) < 0)
            goto fail;
        if (prop_u32(&f, "interrupt-parent", plic_ph) < 0)
            goto fail;
        if (prop_u32(&f, "interrupts", VIRTIO_IRQ) < 0)
            goto fail;
        if (node_end(&f) < 0)
            goto fail;
    }

    if (node_end(&f) < 0)
        goto fail;
    if (node_end(&f) < 0)
        goto fail;
    if (fdt_s32(&f, FDT_END) < 0)
        goto fail;

    uint32_t rsv = 40;
    uint32_t struct_off = 56;
    uint32_t strings_off = struct_off + f.struct_len;
    uint32_t total = strings_off + f.strings_len;
    total = (total + 7u) & ~7u;

    uint8_t *blob = calloc(1, total);
    if (!blob)
        goto fail;
    uint32_t hdr[10];
    hdr[0] = be32(FDT_MAGIC);
    hdr[1] = be32(total);
    hdr[2] = be32(struct_off);
    hdr[3] = be32(strings_off);
    hdr[4] = be32(rsv);
    hdr[5] = be32(17);
    hdr[6] = be32(16);
    hdr[7] = be32(0);
    hdr[8] = be32(f.strings_len);
    hdr[9] = be32(f.struct_len);
    memcpy(blob, hdr, 40);
    memcpy(blob + struct_off, f.buf, f.struct_len);
    memcpy(blob + strings_off, f.str, f.strings_len);

    int rc = dram_copy_in(&cpu->bus.dram, dest, blob, total);
    free(blob);
    free(f.buf);
    free(f.str);
    if (rc < 0)
        return -1;
    if (dtb_size)
        *dtb_size = total;
    return 0;
fail:
    free(f.buf);
    free(f.str);
    return -1;
}
