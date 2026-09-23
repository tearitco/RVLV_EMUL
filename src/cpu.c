#define _GNU_SOURCE
#include "cpu.h"
#include "opcodes.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MISA_VALUE ((2ULL << 62) | (1ULL << 0) | (1ULL << 2) | (1ULL << 3) | \
                    (1ULL << 5) | (1ULL << 8) | (1ULL << 12) | (1ULL << 18) | \
                    (1ULL << 20))

static uint64_t sext(uint64_t v, int bits)
{
    uint64_t m = 1ull << (bits - 1);
    return (v ^ m) - m;
}

static uint32_t rd_f(uint32_t i) { return (i >> 7) & 0x1f; }
static uint32_t rs1_f(uint32_t i) { return (i >> 15) & 0x1f; }
static uint32_t rs2_f(uint32_t i) { return (i >> 20) & 0x1f; }
static uint32_t f3(uint32_t i) { return (i >> 12) & 7; }
static uint32_t f7(uint32_t i) { return (i >> 25) & 0x7f; }

static int64_t imm_i(uint32_t i) { return (int64_t)(int32_t)i >> 20; }
static int64_t imm_s(uint32_t i)
{
    return ((int64_t)(int32_t)(i & 0xfe000000) >> 20) | ((i >> 7) & 0x1f);
}
static int64_t imm_b(uint32_t i)
{
    return ((int64_t)(int32_t)(i & 0x80000000) >> 19) | ((i & 0x80) << 4) |
           ((i >> 20) & 0x7e0) | ((i >> 7) & 0x1e);
}
static int64_t imm_u(uint32_t i) { return (int64_t)(int32_t)(i & 0xfffff000); }
static int64_t imm_j(uint32_t i)
{
    return ((int64_t)(int32_t)(i & 0x80000000) >> 11) | (i & 0xff000) |
           ((i >> 9) & 0x800) | ((i >> 20) & 0x7fe);
}

static uint32_t rvc_r(uint32_t r) { return r + 8; }

static uint32_t enc_i(uint32_t op, uint32_t funct3, uint32_t rd, uint32_t rs1, int32_t imm)
{
    return ((uint32_t)imm << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | op;
}

static uint32_t enc_r(uint32_t op, uint32_t funct3, uint32_t funct7, uint32_t rd, uint32_t rs1,
                      uint32_t rs2)
{
    return (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | op;
}

static uint32_t enc_s(uint32_t op, uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    uint32_t u = (uint32_t)imm;
    return ((u & 0xfe0) << 20) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | ((u & 0x1f) << 7) |
           op;
}

static uint32_t enc_b(uint32_t funct3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    uint32_t u = (uint32_t)imm;
    return ((u & 0x1000) << 19) | ((u & 0x7e0) << 20) | (rs2 << 20) | (rs1 << 15) |
           (funct3 << 12) | ((u & 0x1e) << 7) | ((u & 0x800) >> 4) | 0x63;
}

static uint32_t enc_u(uint32_t op, uint32_t rd, uint32_t imm)
{
    return (imm & 0xfffff000u) | (rd << 7) | op;
}

static uint32_t enc_j(uint32_t rd, int32_t imm)
{
    uint32_t u = (uint32_t)imm;
    return ((u & 0x100000) << 11) | ((u & 0x7fe) << 20) | ((u & 0x800) << 9) | (u & 0xff000) |
           (rd << 7) | 0x6f;
}

uint32_t decompress(uint16_t c, int *illegal)
{
    *illegal = 0;
    uint32_t i = c;
    uint32_t op = i & 3;
    uint32_t funct3 = (i >> 13) & 7;
    if (i == 0) {
        *illegal = 1;
        return 0;
    }
    if (op == 0) {
        uint32_t rd = rvc_r((i >> 2) & 7);
        uint32_t rs1 = rvc_r((i >> 7) & 7);
        uint32_t rs2 = rvc_r((i >> 2) & 7);
        switch (funct3) {
        case 0: {
            uint32_t nzuimm = ((i >> 1) & 0x3c0) | ((i >> 7) & 0x30) | ((i >> 2) & 8) |
                              ((i >> 4) & 4);
            if (nzuimm == 0) {
                *illegal = 1;
                return 0;
            }
            return enc_i(I_TYPE, 0, rd, 2, (int32_t)nzuimm);
        }
        case 1: {
            uint32_t off = ((i >> 7) & 0x38) | ((i << 1) & 0xc0);
            return enc_i(LOAD_FP, 3, rd, rs1, (int32_t)off);
        }
        case 2: {
            uint32_t off = ((i >> 7) & 0x38) | ((i >> 4) & 4) | ((i << 1) & 0x40);
            return enc_i(LOAD, 2, rd, rs1, (int32_t)off);
        }
        case 3: {
            uint32_t off = ((i >> 7) & 0x38) | ((i << 1) & 0xc0);
            return enc_i(LOAD, 3, rd, rs1, (int32_t)off);
        }
        case 5: {
            uint32_t off = ((i >> 7) & 0x38) | ((i << 1) & 0xc0);
            return enc_s(STORE_FP, 3, rs1, rs2, (int32_t)off);
        }
        case 6: {
            uint32_t off = ((i >> 7) & 0x38) | ((i >> 4) & 4) | ((i << 1) & 0x40);
            return enc_s(S_TYPE, 2, rs1, rs2, (int32_t)off);
        }
        case 7: {
            uint32_t off = ((i >> 7) & 0x38) | ((i << 1) & 0xc0);
            return enc_s(S_TYPE, 3, rs1, rs2, (int32_t)off);
        }
        default:
            *illegal = 1;
            return 0;
        }
    }
    if (op == 1) {
        uint32_t rd = (i >> 7) & 0x1f;
        int32_t imm6 = (int32_t)sext(((i >> 7) & 0x20) | ((i >> 2) & 0x1f), 6);
        switch (funct3) {
        case 0:
            return enc_i(I_TYPE, 0, rd, rd, imm6);
        case 1:
            if (rd == 0) {
                *illegal = 1;
                return 0;
            }
            return enc_i(I_TYPE_64, 0, rd, rd, imm6);
        case 2:
            return enc_i(I_TYPE, 0, rd, 0, imm6);
        case 3:
            if (rd == 2) {
                uint32_t nz = ((i << 3) & 0x20) | ((i >> 2) & 0x10) | ((i << 1) & 0x40) |
                              ((i << 4) & 0x180) | ((i >> 3) & 0x200);
                int32_t imm = (int32_t)sext(nz, 10);
                if (imm == 0) {
                    *illegal = 1;
                    return 0;
                }
                return enc_i(I_TYPE, 0, 2, 2, imm);
            }
            if (rd == 0) {
                *illegal = 1;
                return 0;
            }
            {
                uint32_t u = ((i << 5) & 0x20000) | ((i << 10) & 0x1f000);
                int32_t imm = (int32_t)sext(u, 18);
                if (imm == 0) {
                    *illegal = 1;
                    return 0;
                }
                return enc_u(LUI, rd, (uint32_t)imm);
            }
        case 4: {
            uint32_t rd_p = rvc_r((i >> 7) & 7);
            uint32_t rs2_p = rvc_r((i >> 2) & 7);
            uint32_t f2 = (i >> 10) & 3;
            if (f2 == 0)
                return enc_i(I_TYPE, 5, rd_p, rd_p, (int32_t)(((i >> 7) & 0x20) | ((i >> 2) & 0x1f)));
            if (f2 == 1)
                return enc_i(I_TYPE, 5, rd_p, rd_p,
                             (int32_t)(0x400 | ((i >> 7) & 0x20) | ((i >> 2) & 0x1f)));
            if (f2 == 2)
                return enc_i(I_TYPE, 7, rd_p, rd_p, imm6);
            {
                uint32_t sub = (i >> 5) & 3;
                if ((i & 0x1000) == 0) {
                    if (sub == 0)
                        return enc_r(R_TYPE, 0, 0x20, rd_p, rd_p, rs2_p);
                    if (sub == 1)
                        return enc_r(R_TYPE, 4, 0, rd_p, rd_p, rs2_p);
                    if (sub == 2)
                        return enc_r(R_TYPE, 6, 0, rd_p, rd_p, rs2_p);
                    return enc_r(R_TYPE, 7, 0, rd_p, rd_p, rs2_p);
                }
                if (sub == 0)
                    return enc_r(R_TYPE_64, 0, 0x20, rd_p, rd_p, rs2_p);
                if (sub == 1)
                    return enc_r(R_TYPE_64, 0, 0, rd_p, rd_p, rs2_p);
                *illegal = 1;
                return 0;
            }
        }
        case 5: {
            uint32_t off = ((i >> 1) & 0x800) | ((i >> 7) & 0x10) | ((i >> 1) & 0x300) |
                           ((i << 2) & 0x400) | ((i >> 1) & 0x40) | ((i << 1) & 0x80) |
                           ((i >> 2) & 0xe) | ((i << 3) & 0x20);
            int32_t imm = (int32_t)sext(off, 12);
            return enc_j(0, imm);
        }
        case 6:
        case 7: {
            uint32_t rs1 = rvc_r((i >> 7) & 7);
            uint32_t off = ((i >> 4) & 0x100) | ((i >> 7) & 0x18) | ((i << 1) & 0xc0) |
                           ((i >> 2) & 0x6) | ((i << 3) & 0x20);
            int32_t imm = (int32_t)sext(off, 9);
            return enc_b(funct3 == 6 ? 0 : 1, rs1, 0, imm);
        }
        default:
            *illegal = 1;
            return 0;
        }
    }
    if (op == 2) {
        uint32_t rd = (i >> 7) & 0x1f;
        uint32_t rs2 = (i >> 2) & 0x1f;
        switch (funct3) {
        case 0: {
            uint32_t shamt = ((i >> 7) & 0x20) | ((i >> 2) & 0x1f);
            return enc_i(I_TYPE, 1, rd, rd, (int32_t)shamt);
        }
        case 1: {
            uint32_t off = ((i >> 7) & 0x20) | ((i >> 2) & 0x18) | ((i << 4) & 0x1c0);
            if (rd == 0) {
                *illegal = 1;
                return 0;
            }
            return enc_i(LOAD_FP, 3, rd, 2, (int32_t)off);
        }
        case 2: {
            uint32_t off = ((i >> 7) & 0x20) | ((i >> 2) & 0x1c) | ((i << 4) & 0xc0);
            if (rd == 0) {
                *illegal = 1;
                return 0;
            }
            return enc_i(LOAD, 2, rd, 2, (int32_t)off);
        }
        case 3: {
            uint32_t off = ((i >> 7) & 0x20) | ((i >> 2) & 0x18) | ((i << 4) & 0x1c0);
            if (rd == 0) {
                *illegal = 1;
                return 0;
            }
            return enc_i(LOAD, 3, rd, 2, (int32_t)off);
        }
        case 4:
            if ((i & 0x1000) == 0) {
                if (rs2 == 0) {
                    if (rd == 0) {
                        *illegal = 1;
                        return 0;
                    }
                    return enc_i(JALR, 0, 0, rd, 0);
                }
                return enc_r(R_TYPE, 0, 0, rd, 0, rs2);
            }
            if (rd == 0 && rs2 == 0)
                return enc_i(SYSTEM, 0, 0, 0, 1);
            if (rs2 == 0) {
                if (rd == 0) {
                    *illegal = 1;
                    return 0;
                }
                return enc_i(JALR, 0, 1, rd, 0);
            }
            return enc_r(R_TYPE, 0, 0, rd, rd, rs2);
        case 5: {
            uint32_t off = ((i >> 7) & 0x38) | ((i >> 1) & 0x1c0);
            return enc_s(STORE_FP, 3, 2, rs2, (int32_t)off);
        }
        case 6: {
            uint32_t off = ((i >> 7) & 0x3c) | ((i >> 1) & 0xc0);
            return enc_s(S_TYPE, 2, 2, rs2, (int32_t)off);
        }
        case 7: {
            uint32_t off = ((i >> 7) & 0x38) | ((i >> 1) & 0x1c0);
            return enc_s(S_TYPE, 3, 2, rs2, (int32_t)off);
        }
        default:
            *illegal = 1;
            return 0;
        }
    }
    *illegal = 1;
    return 0;
}

static uint64_t mulh_ss(int64_t a, int64_t b)
{
    return (uint64_t)(((__int128)a * (__int128)b) >> 64);
}

static uint64_t mulh_uu(uint64_t a, uint64_t b)
{
    return (uint64_t)(((__uint128_t)a * (__uint128_t)b) >> 64);
}

static uint64_t mulh_su(int64_t a, uint64_t b)
{
    return (uint64_t)(((__int128)a * (__uint128_t)b) >> 64);
}

static uint64_t div_s(int64_t a, int64_t b)
{
    if (b == 0)
        return (uint64_t)-1;
    if (a == INT64_MIN && b == -1)
        return (uint64_t)a;
    return (uint64_t)(a / b);
}

static uint64_t div_u(uint64_t a, uint64_t b)
{
    if (b == 0)
        return ~0ull;
    return a / b;
}

static uint64_t rem_s(int64_t a, int64_t b)
{
    if (b == 0)
        return (uint64_t)a;
    if (a == INT64_MIN && b == -1)
        return 0;
    return (uint64_t)(a % b);
}

static uint64_t rem_u(uint64_t a, uint64_t b)
{
    if (b == 0)
        return a;
    return a % b;
}

int cpu_init(CPU *cpu, uint64_t dram_size, const char *disk_path, int virtio_legacy)
{
    if (!cpu)
        return -1;
    memset(cpu, 0, sizeof(*cpu));
    if (bus_init(&cpu->bus, dram_size, disk_path, virtio_legacy) < 0)
        return -1;
    cpu->virtio_legacy = virtio_legacy;
    cpu_reset(cpu);
    return 0;
}

void cpu_destroy(CPU *cpu)
{
    if (!cpu)
        return;
    bus_free(&cpu->bus);
}

void cpu_reset(CPU *cpu)
{
    memset(cpu->regs, 0, sizeof(cpu->regs));
    memset(cpu->fregs, 0, sizeof(cpu->fregs));
    memset(cpu->csr, 0, sizeof(cpu->csr));
    cpu->pc = DRAM_BASE;
    cpu->priv = PRIV_M;
    cpu->regs[2] = DRAM_BASE + cpu->bus.dram.size;
    cpu->csr[MISA] = MISA_VALUE;
    cpu->csr[MSTATUS] = MSTATUS_MPP | (2ULL << 32) | (2ULL << 34) | MSTATUS_FS_INITIAL;
    cpu->csr[MHARTID] = 0;
    cpu->csr[STIMECMP] = ~0ULL;
    cpu->csr[PMPADDR0] = 0x3fffffffffffffffull;
    cpu->csr[PMPCFG0] = 0x0f;
    cpu->reservation_valid = 0;
    cpu->instret = 0;
    cpu->wfi = 0;
    cpu->halt = 0;
    cpu->halt_code = 0;
}

Trap cpu_fetch(CPU *cpu, uint32_t *inst, int *ilen)
{
    if (cpu->pc & 1)
        return trap_ex(EX_INST_MISALIGNED, cpu->pc);
    uint64_t pa = cpu->pc;
    Trap t;
    if (!(cpu->priv == PRIV_M || (cpu->csr[SATP] >> 60) == 0)) {
        t = cpu_mmu_translate(cpu, cpu->pc, ACC_FETCH, &pa);
        if (t.taken) {
            t.tval = cpu->pc;
            return t;
        }
    }
    uint64_t lo = 0;
    if (dram_in_range(&cpu->bus.dram, pa, 4))
        t = dram_load(&cpu->bus.dram, pa, 16, &lo);
    else
        t = bus_load(&cpu->bus, pa, 16, &lo);
    if (t.taken)
        return trap_ex(EX_INST_ACCESS, cpu->pc);
    if ((lo & 3) != 3) {
        *inst = (uint32_t)lo;
        *ilen = 2;
        return trap_none();
    }
    if (cpu->pc & 2) {
        uint64_t pa2 = pa + 2;
        if (!(cpu->priv == PRIV_M || (cpu->csr[SATP] >> 60) == 0)) {
            t = cpu_mmu_translate(cpu, cpu->pc + 2, ACC_FETCH, &pa2);
            if (t.taken) {
                t.tval = cpu->pc;
                return t;
            }
        }
        uint64_t hi = 0;
        if (dram_in_range(&cpu->bus.dram, pa2, 2))
            t = dram_load(&cpu->bus.dram, pa2, 16, &hi);
        else
            t = bus_load(&cpu->bus, pa2, 16, &hi);
        if (t.taken)
            return trap_ex(EX_INST_ACCESS, cpu->pc);
        *inst = (uint32_t)lo | ((uint32_t)hi << 16);
        *ilen = 4;
        return trap_none();
    }
    uint64_t w = 0;
    if (dram_in_range(&cpu->bus.dram, pa, 4))
        t = dram_load(&cpu->bus.dram, pa, 32, &w);
    else
        t = bus_load(&cpu->bus, pa, 32, &w);
    if (t.taken)
        return trap_ex(EX_INST_ACCESS, cpu->pc);
    *inst = (uint32_t)w;
    *ilen = 4;
    return trap_none();
}

static Trap exec_amo(CPU *cpu, uint32_t inst)
{
    uint32_t rd = rd_f(inst), rs1 = rs1_f(inst), rs2 = rs2_f(inst);
    uint32_t funct3 = f3(inst);
    uint32_t funct5 = f7(inst) >> 2;
    int is_d = funct3 == 3;
    int is_w = funct3 == 2;
    if (!is_d && !is_w)
        return trap_ex(EX_ILLEGAL_INST, inst);
    uint64_t bits = is_d ? 64 : 32;
    uint64_t addr = cpu->regs[rs1];
    if (funct5 == 2) {
        uint64_t pa = 0;
        Trap t = cpu_mmu_translate(cpu, addr, ACC_LOAD, &pa);
        if (t.taken) {
            t.tval = addr;
            return t;
        }
        uint64_t v;
        t = bus_load(&cpu->bus, pa, bits, &v);
        if (t.taken)
            return t;
        cpu->reservation_valid = 1;
        cpu->reservation_addr = pa;
        cpu->reservation_size = (uint8_t)(bits / 8);
        cpu->regs[rd] = is_w ? (uint64_t)(int64_t)(int32_t)v : v;
        return trap_none();
    }
    if (funct5 == 3) {
        uint64_t pa = 0;
        Trap t = cpu_mmu_translate(cpu, addr, ACC_STORE, &pa);
        if (t.taken) {
            t.tval = addr;
            return t;
        }
        int ok = cpu->reservation_valid && cpu->reservation_addr == pa &&
                 cpu->reservation_size == bits / 8;
        cpu->reservation_valid = 0;
        if (!ok) {
            cpu->regs[rd] = 1;
            return trap_none();
        }
        t = bus_store(&cpu->bus, pa, bits, cpu->regs[rs2]);
        if (t.taken)
            return t;
        cpu->regs[rd] = 0;
        return trap_none();
    }
    uint64_t tval;
    Trap t = cpu_load(cpu, addr, bits, &tval);
    if (t.taken)
        return t;
    uint64_t src = cpu->regs[rs2];
    uint64_t res;
    if (is_w) {
        int32_t a = (int32_t)tval;
        int32_t b = (int32_t)src;
        uint32_t ua = (uint32_t)tval;
        uint32_t ub = (uint32_t)src;
        uint32_t ur;
        switch (funct5) {
        case 0x00:
            ur = (uint32_t)(a + b);
            break;
        case 0x01:
            ur = ub;
            break;
        case 0x04:
            ur = ua ^ ub;
            break;
        case 0x08:
            ur = ua | ub;
            break;
        case 0x0c:
            ur = ua & ub;
            break;
        case 0x10:
            ur = (uint32_t)(a < b ? a : b);
            break;
        case 0x14:
            ur = (uint32_t)(a > b ? a : b);
            break;
        case 0x18:
            ur = ua < ub ? ua : ub;
            break;
        case 0x1c:
            ur = ua > ub ? ua : ub;
            break;
        default:
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        res = ur;
        cpu->regs[rd] = (uint64_t)(int64_t)a;
    } else {
        int64_t a = (int64_t)tval;
        int64_t b = (int64_t)src;
        switch (funct5) {
        case 0x00:
            res = tval + src;
            break;
        case 0x01:
            res = src;
            break;
        case 0x04:
            res = tval ^ src;
            break;
        case 0x08:
            res = tval | src;
            break;
        case 0x0c:
            res = tval & src;
            break;
        case 0x10:
            res = (uint64_t)(a < b ? a : b);
            break;
        case 0x14:
            res = (uint64_t)(a > b ? a : b);
            break;
        case 0x18:
            res = tval < src ? tval : src;
            break;
        case 0x1c:
            res = tval > src ? tval : src;
            break;
        default:
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        cpu->regs[rd] = tval;
    }
    return cpu_store(cpu, addr, bits, res);
}

Trap cpu_execute(CPU *cpu, uint32_t inst, int ilen)
{
    if (ilen == 2) {
        int illegal = 0;
        inst = decompress((uint16_t)inst, &illegal);
        if (illegal)
            return trap_ex(EX_ILLEGAL_INST, cpu->pc);
    }
    uint32_t opcode = inst & 0x7f;
    uint32_t rd = rd_f(inst);
    uint32_t rs1 = rs1_f(inst);
    uint32_t rs2 = rs2_f(inst);
    uint32_t funct3 = f3(inst);
    uint32_t funct7 = f7(inst);
    uint64_t inst_pc = cpu->pc;
    uint64_t next = inst_pc + (uint64_t)ilen;

    switch (opcode) {
    case LUI:
        cpu->regs[rd] = (uint64_t)imm_u(inst);
        cpu->pc = next;
        break;
    case AUIPC:
        cpu->regs[rd] = inst_pc + (uint64_t)imm_u(inst);
        cpu->pc = next;
        break;
    case JAL:
        cpu->regs[rd] = next;
        cpu->pc = inst_pc + (uint64_t)imm_j(inst);
        if (cpu->pc & 1)
            return trap_ex(EX_INST_MISALIGNED, cpu->pc);
        break;
    case JALR: {
        uint64_t t = next;
        cpu->pc = (cpu->regs[rs1] + (uint64_t)imm_i(inst)) & ~1ull;
        cpu->regs[rd] = t;
        if (cpu->pc & 1)
            return trap_ex(EX_INST_MISALIGNED, cpu->pc);
        break;
    }
    case B_TYPE: {
        int64_t a = (int64_t)cpu->regs[rs1];
        int64_t b = (int64_t)cpu->regs[rs2];
        int take = 0;
        switch (funct3) {
        case 0:
            take = cpu->regs[rs1] == cpu->regs[rs2];
            break;
        case 1:
            take = cpu->regs[rs1] != cpu->regs[rs2];
            break;
        case 4:
            take = a < b;
            break;
        case 5:
            take = a >= b;
            break;
        case 6:
            take = cpu->regs[rs1] < cpu->regs[rs2];
            break;
        case 7:
            take = cpu->regs[rs1] >= cpu->regs[rs2];
            break;
        default:
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        cpu->pc = take ? inst_pc + (uint64_t)imm_b(inst) : next;
        if (take && (cpu->pc & 1))
            return trap_ex(EX_INST_MISALIGNED, cpu->pc);
        break;
    }
    case LOAD: {
        uint64_t addr = cpu->regs[rs1] + (uint64_t)imm_i(inst);
        uint64_t v;
        Trap t;
        switch (funct3) {
        case 0:
            t = cpu_load(cpu, addr, 8, &v);
            if (t.taken)
                return t;
            cpu->regs[rd] = (uint64_t)(int64_t)(int8_t)v;
            break;
        case 1:
            t = cpu_load(cpu, addr, 16, &v);
            if (t.taken)
                return t;
            cpu->regs[rd] = (uint64_t)(int64_t)(int16_t)v;
            break;
        case 2:
            t = cpu_load(cpu, addr, 32, &v);
            if (t.taken)
                return t;
            cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)v;
            break;
        case 3:
            t = cpu_load(cpu, addr, 64, &v);
            if (t.taken)
                return t;
            cpu->regs[rd] = v;
            break;
        case 4:
            t = cpu_load(cpu, addr, 8, &v);
            if (t.taken)
                return t;
            cpu->regs[rd] = v;
            break;
        case 5:
            t = cpu_load(cpu, addr, 16, &v);
            if (t.taken)
                return t;
            cpu->regs[rd] = v;
            break;
        case 6:
            t = cpu_load(cpu, addr, 32, &v);
            if (t.taken)
                return t;
            cpu->regs[rd] = v;
            break;
        default:
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        cpu->pc = next;
        break;
    }
    case S_TYPE: {
        uint64_t addr = cpu->regs[rs1] + (uint64_t)imm_s(inst);
        Trap t;
        switch (funct3) {
        case 0:
            t = cpu_store(cpu, addr, 8, cpu->regs[rs2]);
            break;
        case 1:
            t = cpu_store(cpu, addr, 16, cpu->regs[rs2]);
            break;
        case 2:
            t = cpu_store(cpu, addr, 32, cpu->regs[rs2]);
            break;
        case 3:
            t = cpu_store(cpu, addr, 64, cpu->regs[rs2]);
            break;
        default:
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        if (t.taken)
            return t;
        cpu->pc = next;
        break;
    }
    case I_TYPE: {
        int64_t imm = imm_i(inst);
        uint32_t shamt = (uint32_t)(imm & 0x3f);
        switch (funct3) {
        case 0:
            cpu->regs[rd] = cpu->regs[rs1] + (uint64_t)imm;
            break;
        case 1:
            cpu->regs[rd] = cpu->regs[rs1] << shamt;
            break;
        case 2:
            cpu->regs[rd] = ((int64_t)cpu->regs[rs1] < imm) ? 1 : 0;
            break;
        case 3:
            cpu->regs[rd] = (cpu->regs[rs1] < (uint64_t)imm) ? 1 : 0;
            break;
        case 4:
            cpu->regs[rd] = cpu->regs[rs1] ^ (uint64_t)imm;
            break;
        case 5:
            if ((inst >> 30) & 1)
                cpu->regs[rd] = (uint64_t)((int64_t)cpu->regs[rs1] >> shamt);
            else
                cpu->regs[rd] = cpu->regs[rs1] >> shamt;
            break;
        case 6:
            cpu->regs[rd] = cpu->regs[rs1] | (uint64_t)imm;
            break;
        case 7:
            cpu->regs[rd] = cpu->regs[rs1] & (uint64_t)imm;
            break;
        default:
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        cpu->pc = next;
        break;
    }
    case R_TYPE: {
        uint32_t shamt = (uint32_t)(cpu->regs[rs2] & 0x3f);
        if (funct7 == 1) {
            switch (funct3) {
            case 0:
                cpu->regs[rd] = cpu->regs[rs1] * cpu->regs[rs2];
                break;
            case 1:
                cpu->regs[rd] = mulh_ss((int64_t)cpu->regs[rs1], (int64_t)cpu->regs[rs2]);
                break;
            case 2:
                cpu->regs[rd] = mulh_su((int64_t)cpu->regs[rs1], cpu->regs[rs2]);
                break;
            case 3:
                cpu->regs[rd] = mulh_uu(cpu->regs[rs1], cpu->regs[rs2]);
                break;
            case 4:
                cpu->regs[rd] = div_s((int64_t)cpu->regs[rs1], (int64_t)cpu->regs[rs2]);
                break;
            case 5:
                cpu->regs[rd] = div_u(cpu->regs[rs1], cpu->regs[rs2]);
                break;
            case 6:
                cpu->regs[rd] = rem_s((int64_t)cpu->regs[rs1], (int64_t)cpu->regs[rs2]);
                break;
            case 7:
                cpu->regs[rd] = rem_u(cpu->regs[rs1], cpu->regs[rs2]);
                break;
            default:
                return trap_ex(EX_ILLEGAL_INST, inst);
            }
        } else {
            switch (funct3) {
            case 0:
                if (funct7 == 0)
                    cpu->regs[rd] = cpu->regs[rs1] + cpu->regs[rs2];
                else if (funct7 == 0x20)
                    cpu->regs[rd] = cpu->regs[rs1] - cpu->regs[rs2];
                else
                    return trap_ex(EX_ILLEGAL_INST, inst);
                break;
            case 1:
                cpu->regs[rd] = cpu->regs[rs1] << shamt;
                break;
            case 2:
                cpu->regs[rd] = ((int64_t)cpu->regs[rs1] < (int64_t)cpu->regs[rs2]) ? 1 : 0;
                break;
            case 3:
                cpu->regs[rd] = (cpu->regs[rs1] < cpu->regs[rs2]) ? 1 : 0;
                break;
            case 4:
                cpu->regs[rd] = cpu->regs[rs1] ^ cpu->regs[rs2];
                break;
            case 5:
                if (funct7 == 0)
                    cpu->regs[rd] = cpu->regs[rs1] >> shamt;
                else if (funct7 == 0x20)
                    cpu->regs[rd] = (uint64_t)((int64_t)cpu->regs[rs1] >> shamt);
                else
                    return trap_ex(EX_ILLEGAL_INST, inst);
                break;
            case 6:
                cpu->regs[rd] = cpu->regs[rs1] | cpu->regs[rs2];
                break;
            case 7:
                cpu->regs[rd] = cpu->regs[rs1] & cpu->regs[rs2];
                break;
            default:
                return trap_ex(EX_ILLEGAL_INST, inst);
            }
        }
        cpu->pc = next;
        break;
    }
    case FENCE:
        cpu->pc = next;
        break;
    case I_TYPE_64: {
        int64_t imm = imm_i(inst);
        uint32_t shamt = (uint32_t)(imm & 0x1f);
        if (funct3 == 0)
            cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)(cpu->regs[rs1] + (uint64_t)imm);
        else if (funct3 == 1)
            cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)((uint32_t)cpu->regs[rs1] << shamt);
        else if (funct3 == 5) {
            if ((inst >> 30) & 1)
                cpu->regs[rd] = (uint64_t)(int64_t)((int32_t)cpu->regs[rs1] >> shamt);
            else
                cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)((uint32_t)cpu->regs[rs1] >> shamt);
        } else
            return trap_ex(EX_ILLEGAL_INST, inst);
        cpu->pc = next;
        break;
    }
    case R_TYPE_64: {
        uint32_t shamt = (uint32_t)(cpu->regs[rs2] & 0x1f);
        if (funct7 == 1) {
            int32_t a = (int32_t)cpu->regs[rs1];
            int32_t b = (int32_t)cpu->regs[rs2];
            uint32_t ua = (uint32_t)cpu->regs[rs1];
            uint32_t ub = (uint32_t)cpu->regs[rs2];
            switch (funct3) {
            case 0:
                cpu->regs[rd] = (uint64_t)(int64_t)(a * b);
                break;
            case 4:
                cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)div_s(a, b);
                break;
            case 5:
                cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)div_u(ua, ub);
                break;
            case 6:
                cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)rem_s(a, b);
                break;
            case 7:
                cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)rem_u(ua, ub);
                break;
            default:
                return trap_ex(EX_ILLEGAL_INST, inst);
            }
        } else {
            switch (funct3) {
            case 0:
                if (funct7 == 0)
                    cpu->regs[rd] =
                        (uint64_t)(int64_t)(int32_t)(cpu->regs[rs1] + cpu->regs[rs2]);
                else if (funct7 == 0x20)
                    cpu->regs[rd] =
                        (uint64_t)(int64_t)(int32_t)(cpu->regs[rs1] - cpu->regs[rs2]);
                else
                    return trap_ex(EX_ILLEGAL_INST, inst);
                break;
            case 1:
                cpu->regs[rd] =
                    (uint64_t)(int64_t)(int32_t)((uint32_t)cpu->regs[rs1] << shamt);
                break;
            case 5:
                if (funct7 == 0)
                    cpu->regs[rd] =
                        (uint64_t)(int64_t)(int32_t)((uint32_t)cpu->regs[rs1] >> shamt);
                else if (funct7 == 0x20)
                    cpu->regs[rd] = (uint64_t)(int64_t)((int32_t)cpu->regs[rs1] >> shamt);
                else
                    return trap_ex(EX_ILLEGAL_INST, inst);
                break;
            default:
                return trap_ex(EX_ILLEGAL_INST, inst);
            }
        }
        cpu->pc = next;
        break;
    }
    case AMO: {
        Trap t = exec_amo(cpu, inst);
        if (t.taken)
            return t;
        cpu->pc = next;
        break;
    }
    case LOAD_FP:
    case STORE_FP:
    case MADD:
    case MSUB:
    case NMSUB:
    case NMADD:
    case OP_FP: {
        Trap t = exec_fp(cpu, inst);
        if (t.taken)
            return t;
        cpu->pc = next;
        break;
    }
    case SYSTEM: {
        if (funct3 == 0) {
            uint32_t funct12 = inst >> 20;
            if (funct12 == 0) {
                if (cpu->sbi && cpu->priv == PRIV_S) {
                    if (sbi_ecall(cpu)) {
                        cpu->pc = next;
                        break;
                    }
                }
                if (cpu->priv == PRIV_M && (cpu->csr[MTVEC] & ~3ull) == 0) {
                    cpu->halt = 1;
                    cpu->halt_code = (cpu->regs[10] == 1) ? 0 : 1;
                    cpu->pc = next;
                    break;
                }
                uint64_t a7 = cpu->regs[17];
                if (cpu->priv != PRIV_M && a7 < 0x100000) {
                    uint64_t ret = syscall_handle(cpu, a7, cpu->regs[10], cpu->regs[11],
                                                  cpu->regs[12], cpu->regs[13],
                                                  cpu->regs[14], cpu->regs[15],
                                                  cpu->regs[16]);
                    cpu->regs[10] = ret;
                    cpu->pc = next;
                    break;
                }
                uint64_t c = cpu->priv == PRIV_M ? EX_ECALL_M
                                                 : cpu->priv == PRIV_S ? EX_ECALL_S : EX_ECALL_U;
                return trap_ex(c, 0);
            }
            if (funct12 == 1)
                return trap_ex(EX_BREAKPOINT, inst_pc);
            if (funct12 == 0x102) {
                if (cpu->priv < PRIV_S)
                    return trap_ex(EX_ILLEGAL_INST, inst);
                if (cpu->priv == PRIV_S && (cpu->csr[MSTATUS] & MSTATUS_TSR))
                    return trap_ex(EX_ILLEGAL_INST, inst);
                uint64_t s = cpu->csr[MSTATUS];
                cpu->pc = cpu->csr[SEPC];
                cpu->priv = (s & MSTATUS_SPP) ? PRIV_S : PRIV_U;
                if ((s >> 5) & 1)
                    s |= MSTATUS_SIE;
                else
                    s &= ~MSTATUS_SIE;
                s |= MSTATUS_SPIE;
                s &= ~MSTATUS_SPP;
                cpu->csr[MSTATUS] = s;
                break;
            }
            if (funct12 == 0x302) {
                if (cpu->priv < PRIV_M)
                    return trap_ex(EX_ILLEGAL_INST, inst);
                uint64_t m = cpu->csr[MSTATUS];
                cpu->pc = cpu->csr[MEPC];
                cpu->priv = (uint8_t)((m >> 11) & 3);
                if (cpu->priv == 2)
                    cpu->priv = PRIV_M;
                if ((m >> 7) & 1)
                    m |= MSTATUS_MIE;
                else
                    m &= ~MSTATUS_MIE;
                m |= MSTATUS_MPIE;
                m &= ~MSTATUS_MPP;
                cpu->csr[MSTATUS] = m;
                break;
            }
            if (funct12 == 0x105) {
                if (cpu->priv < PRIV_S)
                    return trap_ex(EX_ILLEGAL_INST, inst);
                if (cpu->priv != PRIV_M && (cpu->csr[MSTATUS] & MSTATUS_TW))
                    return trap_ex(EX_ILLEGAL_INST, inst);
                cpu->wfi = 1;
                cpu->pc = next;
                break;
            }
            if (funct7 == 0x09) {
                if (cpu->priv < PRIV_S)
                    return trap_ex(EX_ILLEGAL_INST, inst);
                if (cpu->priv == PRIV_S && (cpu->csr[MSTATUS] & MSTATUS_TVM))
                    return trap_ex(EX_ILLEGAL_INST, inst);
                cpu->pc = next;
                break;
            }
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        uint32_t csr = inst >> 20;
        uint64_t old;
        Trap t = csr_read(cpu, csr, &old);
        if (t.taken)
            return t;
        uint64_t src = (funct3 & 4) ? (uint64_t)rs1 : cpu->regs[rs1];
        int write = 1;
        if ((funct3 == 2 || funct3 == 3 || funct3 == 6 || funct3 == 7) && rs1 == 0)
            write = 0;
        uint64_t nv = old;
        switch (funct3 & 3) {
        case 1:
            nv = src;
            break;
        case 2:
            nv = old | src;
            break;
        case 3:
            nv = old & ~src;
            break;
        default:
            return trap_ex(EX_ILLEGAL_INST, inst);
        }
        if (write) {
            t = csr_write(cpu, csr, nv);
            if (t.taken)
                return t;
        }
        cpu->regs[rd] = old;
        cpu->pc = next;
        break;
    }
    default:
        return trap_ex(EX_ILLEGAL_INST, inst);
    }
    return trap_none();
}

int cpu_step(CPU *cpu)
{
    cpu->regs[0] = 0;
    cpu->irq_div++;
    if ((cpu->irq_div & 31ull) == 0) {
        Trap irq = check_interrupts(cpu, false);
        if (irq.taken) {
            take_trap(cpu, irq);
            cpu->regs[0] = 0;
            return 0;
        }
    } else {
        if (cpu->bus.clint.mtime >= cpu->bus.clint.mtimecmp)
            cpu->csr[MIP] |= MIP_MTIP;
        if ((cpu->csr[MENVCFG] & MENVCFG_STCE) &&
            cpu->bus.clint.mtime >= cpu->csr[STIMECMP])
            cpu->csr[MIP] |= MIP_STIP;
    }
    if (cpu->wfi) {
        uint64_t target = ~0ull;
        int have_timer = 0;
        if ((cpu->csr[MIE] & MIP_MTIP) != 0) {
            target = cpu->bus.clint.mtimecmp;
            have_timer = 1;
        }
        if ((cpu->csr[MENVCFG] & MENVCFG_STCE) != 0 && (cpu->csr[MIE] & MIP_STIP) != 0 &&
            cpu->csr[STIMECMP] < target) {
            target = cpu->csr[STIMECMP];
            have_timer = 1;
        }
        if (have_timer && cpu->bus.clint.mtime < target)
            clint_tick(&cpu->bus.clint, target - cpu->bus.clint.mtime);
        else
            clint_tick(&cpu->bus.clint, 1024);
        uart_poll(&cpu->bus.uart);
        Trap wake = check_interrupts(cpu, true);
        if (wake.taken) {
            cpu->wfi = 0;
            take_trap(cpu, wake);
        }
        cpu->regs[0] = 0;
        return 0;
    }
    uint32_t inst;
    int ilen = 4;
    Trap t = cpu_fetch(cpu, &inst, &ilen);
    if (t.taken) {
        take_trap(cpu, t);
        return 0;
    }
    if (cpu->dump) {
        printf("%08" PRIx64 ": %08x\n", cpu->pc, inst);
        dump_registers(cpu);
    }
    t = cpu_execute(cpu, inst, ilen);
    cpu->regs[0] = 0;
    cpu->instret++;
    clint_tick(&cpu->bus.clint, 1);
    if (cpu->bus.test_exit) {
        cpu->halt = 1;
        cpu->halt_code = cpu->bus.test_code;
    }
    if (t.taken)
        take_trap(cpu, t);
    return 0;
}

void dump_registers(CPU *cpu)
{
    static const char *abi[] = {
        "zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2", "s0", "s1", "a0", "a1",
        "a2",   "a3", "a4", "a5", "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
        "s8",   "s9", "s10","s11","t3", "t4", "t5", "t6",
    };
    for (int i = 0; i < 8; i++) {
        printf("   %4s: %#-13.2" PRIx64 "  ", abi[i], cpu->regs[i]);
        printf("   %2s: %#-13.2" PRIx64 "  ", abi[i + 8], cpu->regs[i + 8]);
        printf("   %2s: %#-13.2" PRIx64 "  ", abi[i + 16], cpu->regs[i + 16]);
        printf("   %3s: %#-13.2" PRIx64 "\n", abi[i + 24], cpu->regs[i + 24]);
    }
    printf("pc=%" PRIx64 " priv=%u instret=%" PRIu64 "\n", cpu->pc, cpu->priv, cpu->instret);
}

Trap cpu_load_bytes(CPU *cpu, uint64_t addr, void *buf, uint64_t len) {
    uint8_t *dst = (uint8_t*)buf;
    while (len > 0) {
        uint64_t chunk = len < 8 ? len : 8;
        uint64_t val;
        Trap t = cpu_load(cpu, addr, chunk * 8, &val);
        if (t.taken) return t;
        for (uint64_t i = 0; i < chunk; i++) {
            dst[i] = (val >> (i * 8)) & 0xFF;
        }
        dst += chunk;
        addr += chunk;
        len -= chunk;
    }
    return trap_none();
}

Trap cpu_store_bytes(CPU *cpu, uint64_t addr, const void *buf, uint64_t len) {
    const uint8_t *src = (const uint8_t*)buf;
    while (len > 0) {
        uint64_t chunk = len < 8 ? len : 8;
        uint64_t val = 0;
        for (uint64_t i = 0; i < chunk; i++) {
            val |= ((uint64_t)src[i]) << (i * 8);
        }
        Trap t = cpu_store(cpu, addr, chunk * 8, val);
        if (t.taken) return t;
        src += chunk;
        addr += chunk;
        len -= chunk;
    }
    return trap_none();
}

Trap cpu_load_string(CPU *cpu, uint64_t addr, char *buf, uint64_t maxlen) {
    for (uint64_t i = 0; i < maxlen - 1; i++) {
        uint64_t val;
        Trap t = cpu_load(cpu, addr + i, 8, &val);
        if (t.taken) return t;
        buf[i] = (char)val;
        if (buf[i] == '\0') return trap_none();
    }
    buf[maxlen - 1] = '\0';
    return trap_none();
}

typedef struct {
    uint8_t e_ident[16];
    uint16_t e_type, e_machine;
    uint32_t e_version;
    uint64_t e_entry, e_phoff, e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize, e_phentsize, e_phnum, e_shentsize, e_shnum, e_shstrndx;
} Elf64_Ehdr;

typedef struct {
    uint32_t p_type, p_flags;
    uint64_t p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align;
} Elf64_Phdr;

int load_image(CPU *cpu, const char *path, uint64_t default_addr, uint64_t *entry_out,
               uint64_t *size_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "unable to open %s\n", path);
        return -1;
    }
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
    if (size == 0 || size > cpu->bus.dram.size) {
        fprintf(stderr, "image %s is empty or larger than DRAM\n", path);
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

    uint64_t entry = default_addr;
    uint64_t dest = default_addr;
    if (size >= sizeof(Elf64_Ehdr) && memcmp(buf, "\x7f" "ELF", 4) == 0) {
        Elf64_Ehdr eh;
        memcpy(&eh, buf, sizeof(eh));
        if (eh.e_ident[4] != 2 || eh.e_machine != 243) {
            fprintf(stderr, "%s: not ELF64 RISC-V\n", path);
            free(buf);
            return -1;
        }
        entry = eh.e_entry;
        if (eh.e_phoff > size || eh.e_phentsize != sizeof(Elf64_Phdr)) {
            free(buf);
            return -1;
        }
        if ((uint64_t)eh.e_phnum * eh.e_phentsize > size - eh.e_phoff) {
            free(buf);
            return -1;
        }
        for (uint16_t i = 0; i < eh.e_phnum; i++) {
            Elf64_Phdr ph;
            memcpy(&ph, buf + eh.e_phoff + (uint64_t)i * eh.e_phentsize, sizeof(ph));
            if (ph.p_type != 1)
                continue;
            uint64_t pa = ph.p_paddr ? ph.p_paddr : ph.p_vaddr;
            if (ph.p_filesz > ph.p_memsz || ph.p_offset > size ||
                ph.p_filesz > size - ph.p_offset) {
                free(buf);
                return -1;
            }
            if (ph.p_memsz) {
                if (dram_fill(&cpu->bus.dram, pa, 0, ph.p_memsz) < 0) {
                    fprintf(stderr, "ELF segment does not fit in DRAM\n");
                    free(buf);
                    return -1;
                }
            }
            if (ph.p_filesz &&
                dram_copy_in(&cpu->bus.dram, pa, buf + ph.p_offset, ph.p_filesz) < 0) {
                free(buf);
                return -1;
            }
        }
    } else {
        if (size >= 56) {
            uint32_t magic;
            memcpy(&magic, buf + 48, 4);
            if (magic == 0x05435352u) {
                uint64_t text_off;
                memcpy(&text_off, buf + 8, 8);
                dest = DRAM_BASE + text_off;
            }
        }
        if (dram_copy_in(&cpu->bus.dram, dest, buf, size) < 0) {
            fprintf(stderr, "image does not fit in DRAM\n");
            free(buf);
            return -1;
        }
        uint64_t bss_start = dest + size;
        uint64_t bss_end = DRAM_BASE + 4 * 1024 * 1024;
        if (bss_start < bss_end) {
            dram_fill(&cpu->bus.dram, bss_start, 0, bss_end - bss_start);
        }
        entry = dest;
    }
    free(buf);
    if (entry_out)
        *entry_out = entry;
    if (size_out)
        *size_out = size;
    return 0;
}
