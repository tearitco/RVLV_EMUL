#ifndef CSR_H
#define CSR_H

#include <stdint.h>
#include "trap.h"

#define FFLAGS      0x001
#define FRM         0x002
#define FCSR        0x003

#define SSTATUS     0x100
#define SEDELEG     0x102
#define SIDELEG     0x103
#define SIE         0x104
#define STVEC       0x105
#define SCOUNTEREN  0x106
#define SSCRATCH    0x140
#define SEPC        0x141
#define SCAUSE      0x142
#define STVAL       0x143
#define SIP         0x144
#define STIMECMP    0x14D
#define MTIMECMP    0x14D
#define SATP        0x180

#define MSTATUS     0x300
#define MISA        0x301
#define MEDELEG     0x302
#define MIDELEG     0x303
#define MIE         0x304
#define MTVEC       0x305
#define MCOUNTEREN  0x306
#define MENVCFG     0x30A
#define MCOUNTINHIBIT 0x320
#define MHPMEVENT3  0x323
#define MSCRATCH    0x340
#define MEPC        0x341
#define MCAUSE      0x342
#define MTVAL       0x343
#define MIP         0x344
#define PMPCFG0     0x3A0
#define PMPCFG2     0x3A2
#define PMPADDR0    0x3B0

#define MCYCLE      0xB00
#define MINSTRET    0xB02

#define CYCLE       0xC00
#define TIME        0xC01
#define INSTRET     0xC02

#define MVENDORID   0xF11
#define MARCHID     0xF12
#define MIMPID      0xF13
#define MHARTID     0xF14

#define MSTATUS_SIE   (1ULL << 1)
#define MSTATUS_MIE   (1ULL << 3)
#define MSTATUS_SPIE  (1ULL << 5)
#define MSTATUS_UBE   (1ULL << 6)
#define MSTATUS_MPIE  (1ULL << 7)
#define MSTATUS_SPP   (1ULL << 8)
#define MSTATUS_MPP   (3ULL << 11)
#define MSTATUS_FS    (3ULL << 13)
#define MSTATUS_XS    (3ULL << 15)
#define MSTATUS_MPRV  (1ULL << 17)
#define MSTATUS_SUM   (1ULL << 18)
#define MSTATUS_MXR   (1ULL << 19)
#define MSTATUS_TVM   (1ULL << 20)
#define MSTATUS_TW    (1ULL << 21)
#define MSTATUS_TSR   (1ULL << 22)
#define MSTATUS_UXL   (3ULL << 32)
#define MSTATUS_SXL   (3ULL << 34)
#define MSTATUS_SD    (1ULL << 63)

#define MSTATUS_FS_OFF 0
#define MSTATUS_FS_INITIAL (1ULL << 13)
#define MSTATUS_FS_CLEAN   (2ULL << 13)
#define MSTATUS_FS_DIRTY   (3ULL << 13)

#define MIP_SSIP (1ULL << 1)
#define MIP_MSIP (1ULL << 3)
#define MIP_STIP (1ULL << 5)
#define MIP_MTIP (1ULL << 7)
#define MIP_SEIP (1ULL << 9)
#define MIP_MEIP (1ULL << 11)

#define MENVCFG_FIOM (1ULL << 0)
#define MENVCFG_CBIE (3ULL << 4)
#define MENVCFG_CBCFE (1ULL << 6)
#define MENVCFG_CBZE (1ULL << 7)
#define MENVCFG_PBMTE (1ULL << 62)
#define MENVCFG_STCE (1ULL << 63)
#define MENVCFG_ADUE (1ULL << 61)

#define SATP_MODE_SV39 8ULL

#define SSTATUS_MASK (MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_UBE | MSTATUS_SPP | \
                      MSTATUS_FS | MSTATUS_XS | MSTATUS_SUM | MSTATUS_MXR | \
                      MSTATUS_UXL | MSTATUS_SD)

#define PRIV_U 0
#define PRIV_S 1
#define PRIV_M 3

struct CPU;

Trap csr_read(struct CPU *cpu, uint32_t addr, uint64_t *out);
Trap csr_write(struct CPU *cpu, uint32_t addr, uint64_t value);
uint64_t csr_read_raw(const struct CPU *cpu, uint32_t addr);
void csr_write_raw(struct CPU *cpu, uint32_t addr, uint64_t value);

#endif
