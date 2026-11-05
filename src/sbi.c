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
#define SBI_EXT_PMU                  0x504D55ULL
#define SBI_EXT_DBCN                 0x4442434EULL
#define SBI_EXT_DBCN_CONSOLE_WRITE   0
#define SBI_EXT_DBCN_CONSOLE_READ    1
#define SBI_EXT_DBCN_CONSOLE_WRITE_BYTE 2

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

    if (eid != SBI_EXT_SET_TIMER && eid != SBI_EXT_CONSOLE_PUTCHAR)
        fprintf(stderr, "[SBI] ecall eid=0x%lx fid=0x%lx a0=0x%lx\n", eid, fid, a0);

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
        if (fid == 3) {
            uint64_t ext = a0;
            fprintf(stderr, "[SBI] probe ext=0x%lx\n", ext);
        }
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
                       ext == SBI_EXT_RFENCE || ext == SBI_EXT_HSM || ext == SBI_EXT_SRST || ext == SBI_EXT_PMU ||
                       ext == SBI_EXT_DBCN || ext <= 8;
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
    case SBI_EXT_PMU:
        sbi_ret(cpu, SBI_ERR_NOT_SUPPORTED, 0);
        return 1;
    case SBI_EXT_DBCN:
        fprintf(stderr, "[SBI] DBCN fid=%lu a0=0x%lx a1=0x%lx\n", fid, a0, a1);
        switch (fid) {
        case SBI_EXT_DBCN_CONSOLE_WRITE:
            fprintf(stderr, "[SBI] DBCN_WRITE len=%lu\n", a1);
            fwrite((const char *)a0, 1, a1, stdout);
            fflush(stdout);
            sbi_ret(cpu, SBI_SUCCESS, a1);
            return 1;
        case SBI_EXT_DBCN_CONSOLE_READ:
            uart_poll(&cpu->bus.uart);
            uint64_t read = 0;
            while (read < a1 && cpu->bus.uart.rh_count > 0) {
                int c = cpu->bus.uart.rhr[cpu->bus.uart.rh_head];
                cpu->bus.uart.rh_head = (cpu->bus.uart.rh_head + 1) % UART_RX_FIFO;
                cpu->bus.uart.rh_count--;
                ((char *)a0)[read++] = (char)c;
            }
            sbi_ret(cpu, SBI_SUCCESS, read);
            return 1;
        case SBI_EXT_DBCN_CONSOLE_WRITE_BYTE:
            fputc((int)(uint8_t)a0, stdout);
            fflush(stdout);
            sbi_ret(cpu, SBI_SUCCESS, 0);
            return 1;
        default:
            sbi_ret(cpu, SBI_ERR_NOT_SUPPORTED, 0);
            return 1;
        }
    default:
        return 0;
    }
}
