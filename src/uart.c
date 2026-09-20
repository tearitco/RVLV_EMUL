#include "uart.h"
#include "memmap.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#endif

#define RHR 0
#define IER 1
#define IIR 2
#define LCR 3
#define MCR 4
#define LSR 5
#define MSR 6
#define SCR 7

#define IER_RDA 0x01
#define IER_THRE 0x02
#define LCR_DLAB 0x80
#define LSR_DR 0x01
#define LSR_THRE 0x20
#define LSR_TEMT 0x40

#ifndef _WIN32
static struct termios g_old_term;
static int g_term_saved;
#endif
static int g_term_atexit;

#ifndef _WIN32
static void uart_raw_term(void)
{
    if (g_term_saved)
        return;
    if (!isatty(STDIN_FILENO))
        return;
    if (tcgetattr(STDIN_FILENO, &g_old_term) < 0)
        return;
    struct termios t = g_old_term;
    t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    if (flags >= 0)
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    g_term_saved = 1;
}
#endif

void uart_restore_term(void)
{
#ifndef _WIN32
    if (g_term_saved) {
        tcsetattr(STDIN_FILENO, TCSANOW, &g_old_term);
        g_term_saved = 0;
    }
#endif
}

void uart_init(UART *u)
{
    if (!u)
        return;
    *u = (UART){};
    u->lsr = LSR_THRE | LSR_TEMT;
    u->msr = 0x90;
    if (!g_term_atexit) {
        atexit(uart_restore_term);
        g_term_atexit = 1;
    }
#ifndef _WIN32
    uart_raw_term();
#endif
}

static int cons_readable(void)
{
#ifdef _WIN32
    return _kbhit() ? 1 : 0;
#else
    fd_set rfds;
    struct timeval tv = {0, 0};
    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    return select(STDIN_FILENO + 1, &rfds, nullptr, nullptr, &tv) > 0;
#endif
}

static int cons_getchar(void)
{
#ifdef _WIN32
    int c = _getch();
    if (c == 0 || c == 0xE0) {
        (void)_getch();
        return -1;
    }
    return c;
#else
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1)
        return (int)c;
    return -1;
#endif
}

static void rx_push(UART *u, uint8_t c)
{
    if (u->rh_count >= UART_RX_FIFO)
        return;
    u->rhr[u->rh_tail] = c;
    u->rh_tail = (u->rh_tail + 1) % UART_RX_FIFO;
    u->rh_count++;
}

static int rx_pop(UART *u)
{
    if (u->rh_count == 0)
        return -1;
    int c = u->rhr[u->rh_head];
    u->rh_head = (u->rh_head + 1) % UART_RX_FIFO;
    u->rh_count--;
    return c;
}

void uart_poll(UART *u)
{
    while (cons_readable()) {
        int c = cons_getchar();
        if (c < 0)
            break;
        rx_push(u, (uint8_t)c);
    }
}

int uart_irq_pending(UART *u)
{
    int rx = (u->ier & IER_RDA) && u->rh_count > 0;
    int tx = (u->ier & IER_THRE) && u->tx_irq_pending;
    u->interrupting = rx || tx;
    return u->interrupting;
}

Trap uart_load(UART *u, uint64_t addr, uint64_t bits, uint64_t *out)
{
    (void)bits;
    uint64_t off = addr - UART_BASE;
    uint64_t v = 0;
    switch (off) {
    case RHR:
        if (u->lcr & LCR_DLAB) {
            v = u->dll;
        } else {
            int c = rx_pop(u);
            v = (c < 0) ? 0 : (uint64_t)(uint8_t)c;
        }
        break;
    case IER:
        v = (u->lcr & LCR_DLAB) ? u->dlm : u->ier;
        break;
    case IIR: {
        int rx = (u->ier & IER_RDA) && u->rh_count > 0;
        int tx = (u->ier & IER_THRE) && u->tx_irq_pending;
        if (rx)
            v = 0x04;
        else if (tx) {
            v = 0x02;
            u->tx_irq_pending = 0;
        } else
            v = 0x01;
        v |= 0xC0;
        break;
    }
    case LCR:
        v = u->lcr;
        break;
    case MCR:
        v = u->mcr;
        break;
    case LSR:
        v = LSR_THRE | LSR_TEMT;
        if (u->rh_count > 0)
            v |= LSR_DR;
        break;
    case MSR:
        v = u->msr;
        break;
    case SCR:
        v = u->scr;
        break;
    default:
        v = 0;
        break;
    }
    *out = v;
    return trap_none();
}

Trap uart_store(UART *u, uint64_t addr, uint64_t bits, uint64_t value)
{
    (void)bits;
    uint64_t off = addr - UART_BASE;
    uint8_t val = (uint8_t)value;
    switch (off) {
    case RHR:
        if (u->lcr & LCR_DLAB) {
            u->dll = val;
        } else {
            if (val == '\r') {
                /* keep CR; guests often map it */
            }
            fputc((int)val, stdout);
            fflush(stdout);
            if (u->ier & IER_THRE)
                u->tx_irq_pending = 1;
        }
        break;
    case IER:
        if (u->lcr & LCR_DLAB)
            u->dlm = val;
        else {
            uint8_t old = u->ier;
            u->ier = val & 0x0f;
            if ((u->ier & IER_THRE) && !(old & IER_THRE))
                u->tx_irq_pending = 1;
        }
        break;
    case IIR:
        /* FCR */
        if (val & 0x02) {
            u->rh_head = u->rh_tail = u->rh_count = 0;
        }
        break;
    case LCR:
        u->lcr = val;
        break;
    case MCR:
        u->mcr = val;
        break;
    case SCR:
        u->scr = val;
        break;
    default:
        break;
    }
    return trap_none();
}
