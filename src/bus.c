#include "bus.h"
#include "fpga_hw.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef FPGA_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#endif

#define FB_RGB565(r,g,b)  (((r) & 0xF8) | (((g) & 0xFC) << 5) | (((b) & 0xF8) << 11))

static unsigned char fb_pixels[FPGA_FB_WIDTH * FPGA_FB_HEIGHT * 4];
#ifdef FPGA_X11
static int fb_initialized = 0;
static unsigned char key_fifo[FPGA_KEY_FIFO_SIZE];
static int key_head = 0, key_tail = 0;
#endif

#ifdef FPGA_X11
static Display *x_dpy = NULL;
static Window x_win = 0;
static GC x_gc = NULL;
static XImage *x_img = NULL;

__attribute__((noinline)) static void fb_x11_init(void) {
    if (fb_initialized) return;
    x_dpy = XOpenDisplay(NULL);
    if (!x_dpy) {
        fprintf(stderr, "FPGA: cannot open X11 display, running headless\n");
        return;
    }
    int screen = DefaultScreen(x_dpy);
    Window root = RootWindow(x_dpy, screen);
    Visual *vis = DefaultVisual(x_dpy, screen);
    int depth = DefaultDepth(x_dpy, screen);
    x_win = XCreateWindow(x_dpy, root, 0, 0,
        FPGA_FB_WIDTH, FPGA_FB_HEIGHT, 0, depth,
        InputOutput, vis, 0, NULL);
    XStoreName(x_dpy, x_win, "rvemu FPGA");
    XSelectInput(x_dpy, x_win, ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
    XMapWindow(x_dpy, x_win);
    x_gc = XCreateGC(x_dpy, x_win, 0, NULL);
    x_img = XCreateImage(x_dpy, vis, depth, ZPixmap, 0,
        (char *)fb_pixels, FPGA_FB_WIDTH, FPGA_FB_HEIGHT, 32, 0);
    fb_initialized = 1;
}

__attribute__((noinline)) static void fb_x11_render(void) {
    if (!fb_initialized || !x_dpy || !x_win || !x_img) return;
    XPutImage(x_dpy, x_win, x_gc, x_img, 0, 0, 0, 0,
        FPGA_FB_WIDTH, FPGA_FB_HEIGHT);
    XFlush(x_dpy);
    XSync(x_dpy, False);
}

__attribute__((noinline)) static void fb_x11_poll_events(void) {
    if (!fb_initialized || !x_dpy) return;
    XEvent xev;
    while (XPending(x_dpy)) {
        XNextEvent(x_dpy, &xev);
        if (xev.type == KeyPress) {
            KeySym keysym = XLookupKeysym(&xev.xkey, 0);
            if (keysym >= 0x20 && keysym <= 0x7F) {
                int next = (key_tail + 1) % FPGA_KEY_FIFO_SIZE;
                if (next != key_head) {
                    key_fifo[key_tail] = (unsigned char)keysym;
                    key_tail = next;
                }
            }
        }
    }
}
#endif

#ifdef FPGA_X11
__attribute__((noinline)) static void fb_x11_init(void);
__attribute__((noinline)) static void fb_x11_render(void);
__attribute__((noinline)) static void fb_x11_poll_events(void);
#endif

static void fb_write_pixel(uint32_t offset, uint64_t value, uint64_t bits) {
    if (offset >= FPGA_FB_MAX_PIXELS) return;
    uint32_t r, g, b;
    if (bits == 32) {
        b = value & 0xFF;
        g = (value >> 8) & 0xFF;
        r = (value >> 16) & 0xFF;
    } else {
        r = (value >> 16) & 0xFF;
        g = (value >> 8) & 0xFF;
        b = value & 0xFF;
    }
    unsigned char *pixel = &fb_pixels[offset * 4];
    pixel[0] = b;
    pixel[1] = g;
    pixel[2] = r;
    pixel[3] = 0xFF;
}

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
    bus->fpga_loaded = 0;
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
    if (in_range(addr, FPGA_FB_BASE, FPGA_FB_STRIDE)) {
        uint32_t pixel_idx = (uint32_t)((addr - FPGA_FB_BASE) / 4);
        if (pixel_idx < FPGA_FB_MAX_PIXELS) {
            unsigned char *pixel = &fb_pixels[pixel_idx * 4];
            *out = (uint64_t)((pixel[2] << 16) | (pixel[1] << 8) | pixel[0]);
        } else {
            *out = 0;
        }
        return trap_none();
    }
    if (in_range(addr, FPGA_KEY_ADDR, 0x100)) {
        uint8_t key = 0;
#ifdef FPGA_X11
        if (key_head != key_tail) {
            key = key_fifo[key_head];
            key_head = (key_head + 1) % FPGA_KEY_FIFO_SIZE;
        }
#endif
        *out = key;
        return trap_none();
    }
    if (in_range(addr, FPGA_MMIO_BASE, FPGA_MMIO_SIZE)) {
        if (!bus->fpga_loaded) {
            *out = 0;
            return trap_none();
        }
        uint64_t pin_offset = (addr - FPGA_MMIO_BASE) / 0x100;
        if (pin_offset < FPGA_MAX_PINS) {
            fpga_cycle(&bus->fpga);
            *out = fpga_get_pin(&bus->fpga, (uint8_t)pin_offset);
            return trap_none();
        }
        *out = 0;
        return trap_none();
    }
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
    if (in_range(addr, FPGA_FB_BASE, FPGA_FB_STRIDE)) {
        uint32_t pixel_idx = (uint32_t)((addr - FPGA_FB_BASE) / 4);
        fb_write_pixel(pixel_idx, value, bits);
#ifdef FPGA_X11
        if (!fb_initialized) fb_x11_init();
        fb_x11_poll_events();
        if (fb_initialized) fb_x11_render();
#endif
        return trap_none();
    }
    if (in_range(addr, FPGA_MMIO_BASE, FPGA_MMIO_SIZE)) {
        if (!bus->fpga_loaded) {
            return trap_none();
        }
        uint64_t pin_offset = (addr - FPGA_MMIO_BASE) / 0x100;
        if (pin_offset < FPGA_MAX_PINS) {
            fpga_set_pin(&bus->fpga, (uint8_t)pin_offset, (fpga_word_t)value);
            fpga_cycle(&bus->fpga);
        }
        return trap_none();
    }
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

void bus_dump_fb(BUS *bus, const char *path)
{
    (void)bus;
    if (!path) return;
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open %s for framebuffer dump\n", path);
        return;
    }
    fprintf(f, "P6\n%d %d\n255\n", FPGA_FB_WIDTH, FPGA_FB_HEIGHT);
    for (uint32_t i = 0; i < FPGA_FB_MAX_PIXELS; i++) {
        unsigned char *pixel = &fb_pixels[i * 4];
        unsigned char rgb[3] = { pixel[2], pixel[1], pixel[0] };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    fprintf(stderr, "framebuffer dumped to %s\n", path);
}
