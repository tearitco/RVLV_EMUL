#include "csr.h"
#include "cpu.h"

#include <string.h>

static uint64_t mstatus_legal(uint64_t v)
{
    v &= (MSTATUS_SIE | MSTATUS_MIE | MSTATUS_SPIE | MSTATUS_UBE | MSTATUS_MPIE |
          MSTATUS_SPP | MSTATUS_MPP | MSTATUS_FS | MSTATUS_XS | MSTATUS_MPRV |
          MSTATUS_SUM | MSTATUS_MXR | MSTATUS_TVM | MSTATUS_TW | MSTATUS_TSR |
          MSTATUS_UXL | MSTATUS_SXL | MSTATUS_SD);
    if (((v >> 11) & 3) == 2)
        v = (v & ~MSTATUS_MPP) | (3ULL << 11);
    uint64_t fs = v & MSTATUS_FS;
    uint64_t xs = v & MSTATUS_XS;
    if (fs == MSTATUS_FS_DIRTY || xs == (3ULL << 15))
        v |= MSTATUS_SD;
    else
        v &= ~MSTATUS_SD;
    return v;
}

uint64_t csr_read_raw(const CPU *cpu, uint32_t addr)
{
    switch (addr) {
    case SSTATUS:
        return cpu->csr[MSTATUS] & SSTATUS_MASK;
    case SIE:
        return cpu->csr[MIE] & cpu->csr[MIDELEG];
    case SIP:
        return cpu->csr[MIP] & cpu->csr[MIDELEG];
    case TIME:
        return cpu->bus.clint.mtime;
    case CYCLE:
    case MCYCLE:
        return cpu->instret;
    case INSTRET:
    case MINSTRET:
        return cpu->instret;
    case FFLAGS:
        return cpu->csr[FCSR] & 0x1f;
    case FRM:
        return (cpu->csr[FCSR] >> 5) & 7;
    case FCSR:
        return cpu->csr[FCSR] & 0xff;
    default:
        if (addr < 4096)
            return cpu->csr[addr];
        return 0;
    }
}

void csr_write_raw(CPU *cpu, uint32_t addr, uint64_t value)
{
    switch (addr) {
    case SSTATUS:
        cpu->csr[MSTATUS] = mstatus_legal((cpu->csr[MSTATUS] & ~SSTATUS_MASK) |
                                          (value & SSTATUS_MASK));
        break;
    case MSTATUS:
        cpu->csr[MSTATUS] = mstatus_legal(value);
        break;
    case SIE:
        cpu->csr[MIE] = (cpu->csr[MIE] & ~cpu->csr[MIDELEG]) | (value & cpu->csr[MIDELEG]);
        break;
    case SIP: {
        uint64_t mask = MIP_SSIP & cpu->csr[MIDELEG];
        cpu->csr[MIP] = (cpu->csr[MIP] & ~mask) | (value & mask);
        break;
    }
    case MIDELEG:
        cpu->csr[MIDELEG] = value & (MIP_SSIP | MIP_STIP | MIP_SEIP);
        break;
    case MEDELEG:
        cpu->csr[MEDELEG] = value & ~((1ull << 10) | (1ull << 11) | (1ull << 14));
        break;
    case MIE:
        cpu->csr[MIE] = value & (MIP_SSIP | MIP_MSIP | MIP_STIP | MIP_MTIP | MIP_SEIP | MIP_MEIP);
        break;
    case MIP: {
        uint64_t wmask = MIP_SSIP | MIP_MSIP | MIP_STIP;
        if ((cpu->csr[MENVCFG] & MENVCFG_STCE) == 0)
            wmask |= MIP_STIP;
        cpu->csr[MIP] = (cpu->csr[MIP] & ~wmask) | (value & wmask);
        break;
    }
    case FFLAGS:
        cpu->csr[FCSR] = (cpu->csr[FCSR] & ~0x1full) | (value & 0x1f);
        cpu->csr[MSTATUS] = mstatus_legal(cpu->csr[MSTATUS] | MSTATUS_FS_DIRTY);
        break;
    case FRM:
        cpu->csr[FCSR] = (cpu->csr[FCSR] & ~0xe0ull) | ((value & 7) << 5);
        cpu->csr[MSTATUS] = mstatus_legal(cpu->csr[MSTATUS] | MSTATUS_FS_DIRTY);
        break;
    case FCSR:
        cpu->csr[FCSR] = value & 0xff;
        cpu->csr[MSTATUS] = mstatus_legal(cpu->csr[MSTATUS] | MSTATUS_FS_DIRTY);
        break;
    case SATP:
        cpu->csr[SATP] = value;
        break;
    case MEPC:
    case SEPC:
        cpu->csr[addr] = value & ~1ull;
        break;
    case STIMECMP:
        cpu->csr[STIMECMP] = value;
        if (cpu->priv == PRIV_M)
            cpu->bus.clint.mtimecmp = value;
        break;
    case MISA:
        break;
    case TIME:
    case CYCLE:
    case INSTRET:
    case MVENDORID:
    case MARCHID:
    case MIMPID:
    case MHARTID:
        break;
    default:
        if (addr < 4096)
            cpu->csr[addr] = value;
        break;
    }
}

static int csr_min_priv(uint32_t addr)
{
    return (int)((addr >> 8) & 3);
}

static int csr_is_readonly(uint32_t addr)
{
    return ((addr >> 10) & 3) == 3;
}

Trap csr_read(CPU *cpu, uint32_t addr, uint64_t *out)
{
    if (addr > 0xfff)
        return trap_ex(EX_ILLEGAL_INST, 0);
    if (cpu->priv < csr_min_priv(addr))
        return trap_ex(EX_ILLEGAL_INST, 0);
    if ((addr == CYCLE || addr == TIME || addr == INSTRET) && cpu->priv < PRIV_M) {
        uint64_t en = cpu->csr[MCOUNTEREN];
        int bit = (addr == CYCLE) ? 0 : (addr == TIME) ? 1 : 2;
        if (((en >> bit) & 1) == 0)
            return trap_ex(EX_ILLEGAL_INST, 0);
        if (cpu->priv < PRIV_S) {
            uint64_t sen = cpu->csr[SCOUNTEREN];
            if (((sen >> bit) & 1) == 0)
                return trap_ex(EX_ILLEGAL_INST, 0);
        }
    }
    if (addr == SATP && cpu->priv == PRIV_S && (cpu->csr[MSTATUS] & MSTATUS_TVM))
        return trap_ex(EX_ILLEGAL_INST, 0);
    if (addr == STIMECMP && (cpu->csr[MENVCFG] & MENVCFG_STCE) == 0)
        return trap_ex(EX_ILLEGAL_INST, 0);
    *out = csr_read_raw(cpu, addr);
    return trap_none();
}

Trap csr_write(CPU *cpu, uint32_t addr, uint64_t value)
{
    if (addr > 0xfff)
        return trap_ex(EX_ILLEGAL_INST, 0);
    if (cpu->priv < csr_min_priv(addr))
        return trap_ex(EX_ILLEGAL_INST, 0);
    if (csr_is_readonly(addr))
        return trap_ex(EX_ILLEGAL_INST, 0);
    if (addr == SATP && cpu->priv == PRIV_S && (cpu->csr[MSTATUS] & MSTATUS_TVM))
        return trap_ex(EX_ILLEGAL_INST, 0);
    if (addr == STIMECMP && (cpu->csr[MENVCFG] & MENVCFG_STCE) == 0)
        return trap_ex(EX_ILLEGAL_INST, 0);
    csr_write_raw(cpu, addr, value);
    return trap_none();
}
