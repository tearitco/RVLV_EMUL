#include "cpu.h"

#include <stdio.h>
#include <stdlib.h>

#define SBI_SUCCESS            0
#define SBI_ERR_FAILED         -1
#define SBI_ERR_NOT_SUPPORTED  -2
#define SBI_ERR_INVALID_PARAM  -3
#define SBI_ERR_DENIED         -4
#define SBI_ERR_INVALID_ADDRESS -5
#define SBI_ERR_ALREADY_AVAILABLE -6
#define SBI_ERR_ALREADY_STARTED -7
#define SBI_ERR_ALREADY_STOPPED -8

#define SBI_EXT_SET_TIMER            0x00
#define SBI_EXT_CONSOLE_PUTCHAR      0x01
#define SBI_EXT_CONSOLE_GETCHAR      0x02
#define SBI_EXT_CLEAR_IPI            0x03
#define SBI_EXT_SEND_IPI             0x04
#define SBI_EXT_REMOTE_FENCE_I       0x05
#define SBI_EXT_REMOTE_SFENCE_VMA    0x06
#define SBI_EXT_REMOTE_SFENCE_VMA_ASID 0x07
#define SBI_EXT_SHUTDOWN             0x08
#define SBI_EXT_BASE                 0x10
#define SBI_EXT_TIME                 0x54494D45ULL
#define SBI_EXT_IPI                  0x735049ULL
#define SBI_EXT_RFENCE               0x52464E43ULL
#define SBI_EXT_HSM                  0x48534DULL
#define SBI_EXT_SRST                 0x53525354ULL

static void sbi_ret(CPU *cpu, int64_t err, uint64_t value)
{
    cpu->regs[10] = (uint64_t)err;
    cpu->regs[11] = value;
}

static void sbi_set_timer(CPU *cpu, uint64_t when)
{
    if (cpu->csr[MENVCFG] & MENVCFG_STCE) {
        cpu->csr[STIMECMP] = when;
        cpu->csr[MIP] &= ~MIP_STIP;
    } else {
        cpu->bus.clint.mtimecmp = when;
        cpu->csr[MIP] &= ~MIP_MTIP;
        cpu->csr[MIP] &= ~MIP_STIP;
    }
}

int sbi_ecall(CPU *cpu)
{
    uint64_t eid = cpu->regs[17];
    uint64_t fid = cpu->regs[16];
    uint64_t a0 = cpu->regs[10];
    uint64_t a1 = cpu->regs[11];
    (void)a1;

    switch (eid) {
    case SBI_EXT_SET_TIMER:
        sbi_set_timer(cpu, a0);
        cpu->regs[10] = 0;
        return 1;
    case SBI_EXT_CONSOLE_PUTCHAR:
        fputc((int)(uint8_t)a0, stdout);
        fflush(stdout);
        cpu->regs[10] = 0;
        return 1;
    case SBI_EXT_CONSOLE_GETCHAR:
        uart_poll(&cpu->bus.uart);
        if (cpu->bus.uart.rh_count > 0) {
            int c = cpu->bus.uart.rhr[cpu->bus.uart.rh_head];
            cpu->bus.uart.rh_head = (cpu->bus.uart.rh_head + 1) % UART_RX_FIFO;
            cpu->bus.uart.rh_count--;
            cpu->regs[10] = (uint64_t)(uint8_t)c;
        } else {
            cpu->regs[10] = (uint64_t)-1;
        }
        return 1;
    case SBI_EXT_CLEAR_IPI:
        cpu->csr[MIP] &= ~MIP_SSIP;
        cpu->regs[10] = 0;
        return 1;
    case SBI_EXT_SEND_IPI:
        cpu->csr[MIP] |= MIP_SSIP;
        cpu->regs[10] = 0;
        return 1;
    case SBI_EXT_REMOTE_FENCE_I:
    case SBI_EXT_REMOTE_SFENCE_VMA:
    case SBI_EXT_REMOTE_SFENCE_VMA_ASID:
        cpu->regs[10] = 0;
        return 1;
    case SBI_EXT_SHUTDOWN:
        cpu->halt = 1;
        cpu->halt_code = 0;
        cpu->regs[10] = 0;
        return 1;
    case SBI_EXT_BASE:
        switch (fid) {
        case 0:
            sbi_ret(cpu, SBI_SUCCESS, 0x02000000);
            return 1;
        case 1:
            sbi_ret(cpu, SBI_SUCCESS, 1);
            return 1;
        case 2:
            sbi_ret(cpu, SBI_SUCCESS, 1);
            return 1;
        case 3: {
            uint64_t ext = a0;
            int have = ext == SBI_EXT_BASE || ext == SBI_EXT_TIME || ext == SBI_EXT_IPI ||
                       ext == SBI_EXT_RFENCE || ext == SBI_EXT_HSM || ext == SBI_EXT_SRST ||
                       ext <= 8;
            sbi_ret(cpu, SBI_SUCCESS, have ? 1 : 0);
            return 1;
        }
        case 4:
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        case 5:
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        case 6:
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        default:
            sbi_ret(cpu, SBI_ERR_NOT_SUPPORTED, 0);
            return 1;
        }
    case SBI_EXT_TIME:
        if (fid == 0) {
            sbi_set_timer(cpu, a0);
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        }
        sbi_ret(cpu, SBI_ERR_NOT_SUPPORTED, 0);
        return 1;
    case SBI_EXT_IPI:
        if (fid == 0) {
            cpu->csr[MIP] |= MIP_SSIP;
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        }
        sbi_ret(cpu, SBI_ERR_NOT_SUPPORTED, 0);
        return 1;
    case SBI_EXT_RFENCE:
        sbi_ret(cpu, SBI_SUCCESS, 0);
        return 1;
    case SBI_EXT_HSM:
        if (fid == 2) {
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        }
        if (fid == 1) {
            cpu->wfi = 1;
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        }
        sbi_ret(cpu, SBI_ERR_NOT_SUPPORTED, 0);
        return 1;
    case SBI_EXT_SRST:
        cpu->halt = 1;
        cpu->halt_code = 0;
        sbi_ret(cpu, SBI_SUCCESS, 0);
        return 1;
    default:
        return 0;
    }
}
