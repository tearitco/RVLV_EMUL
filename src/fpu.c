#include "cpu.h"
#include "opcodes.h"

#include <fenv.h>
#include <math.h>
#include <string.h>

#define FFLAG_NV 0x10
#define FFLAG_DZ 0x08
#define FFLAG_OF 0x04
#define FFLAG_UF 0x02
#define FFLAG_NX 0x01

static uint32_t unbox_s(uint64_t v)
{
    if ((v >> 32) == 0xffffffffu)
        return (uint32_t)v;
    return 0x7fc00000u;
}

static uint64_t box_s(uint32_t v)
{
    return 0xffffffff00000000ull | v;
}

static float bits_to_f32(uint32_t b)
{
    float f;
    memcpy(&f, &b, 4);
    return f;
}

static uint32_t f32_to_bits(float f)
{
    uint32_t b;
    memcpy(&b, &f, 4);
    return b;
}

static double bits_to_f64(uint64_t b)
{
    double d;
    memcpy(&d, &b, 8);
    return d;
}

static uint64_t f64_to_bits(double d)
{
    uint64_t b;
    memcpy(&b, &d, 8);
    return b;
}

static int set_rm(CPU *cpu, uint32_t inst)
{
    uint32_t rm = (inst >> 12) & 7;
    if (rm == 7)
        rm = (uint32_t)((cpu->csr[FCSR] >> 5) & 7);
    int mode;
    switch (rm) {
    case 0:
        mode = FE_TONEAREST;
        break;
    case 1:
        mode = FE_TOWARDZERO;
        break;
    case 2:
        mode = FE_DOWNWARD;
        break;
    case 3:
        mode = FE_UPWARD;
        break;
    case 4:
        mode = FE_TONEAREST;
        break;
    default:
        return -1;
    }
    fesetround(mode);
    feclearexcept(FE_ALL_EXCEPT);
    return 0;
}

static void collect_flags(CPU *cpu)
{
    int e = fetestexcept(FE_ALL_EXCEPT);
    uint64_t f = cpu->csr[FCSR];
    if (e & FE_INVALID)
        f |= FFLAG_NV;
    if (e & FE_DIVBYZERO)
        f |= FFLAG_DZ;
    if (e & FE_OVERFLOW)
        f |= FFLAG_OF;
    if (e & FE_UNDERFLOW)
        f |= FFLAG_UF;
    if (e & FE_INEXACT)
        f |= FFLAG_NX;
    cpu->csr[FCSR] = (cpu->csr[FCSR] & ~0x1full) | (f & 0x1f);
    cpu->csr[MSTATUS] |= MSTATUS_FS_DIRTY;
}

static int fp_enabled(CPU *cpu)
{
    return (cpu->csr[MSTATUS] & MSTATUS_FS) != 0;
}

static uint32_t fclass_s(uint32_t b)
{
    uint32_t exp = (b >> 23) & 0xff;
    uint32_t frac = b & 0x7fffff;
    int sign = (b >> 31) & 1;
    if (exp == 0xff) {
        if (frac == 0)
            return sign ? (1u << 0) : (1u << 7);
        if (frac & 0x400000)
            return 1u << 9;
        return 1u << 8;
    }
    if (exp == 0) {
        if (frac == 0)
            return sign ? (1u << 3) : (1u << 4);
        return sign ? (1u << 2) : (1u << 5);
    }
    return sign ? (1u << 1) : (1u << 6);
}

static uint32_t fclass_d(uint64_t b)
{
    uint32_t exp = (uint32_t)((b >> 52) & 0x7ff);
    uint64_t frac = b & 0xfffffffffffffull;
    int sign = (int)((b >> 63) & 1);
    if (exp == 0x7ff) {
        if (frac == 0)
            return sign ? (1u << 0) : (1u << 7);
        if (frac & 0x8000000000000ull)
            return 1u << 9;
        return 1u << 8;
    }
    if (exp == 0) {
        if (frac == 0)
            return sign ? (1u << 3) : (1u << 4);
        return sign ? (1u << 2) : (1u << 5);
    }
    return sign ? (1u << 1) : (1u << 6);
}

static int64_t fcvt_to_l(double d, int unsigned_out, int width32)
{
    if (isnan(d) || d == INFINITY || d == -INFINITY) {
        feraiseexcept(FE_INVALID);
        if (unsigned_out)
            return 0;
        if (width32)
            return 0x7fffffff;
        return 0x7fffffffffffffffll;
    }
    if (unsigned_out) {
        if (d < 0) {
            feraiseexcept(FE_INVALID);
            return 0;
        }
        if (width32) {
            if (d >= 4294967296.0) {
                feraiseexcept(FE_INVALID);
                return 0xffffffffu;
            }
            return (int64_t)(uint32_t)d;
        }
        if (d >= 18446744073709551616.0) {
            feraiseexcept(FE_INVALID);
            return (int64_t)~0ull;
        }
        return (int64_t)(uint64_t)d;
    }
    if (width32) {
        if (d >= 2147483648.0) {
            feraiseexcept(FE_INVALID);
            return 0x7fffffff;
        }
        if (d < -2147483649.0) {
            feraiseexcept(FE_INVALID);
            return (int32_t)0x80000000;
        }
        return (int32_t)nearbyint(d);
    }
    if (d >= 9223372036854775808.0) {
        feraiseexcept(FE_INVALID);
        return 0x7fffffffffffffffll;
    }
    if (d < -9223372036854775808.0) {
        feraiseexcept(FE_INVALID);
        return (int64_t)0x8000000000000000ull;
    }
    return (int64_t)nearbyint(d);
}

Trap exec_fp(CPU *cpu, uint32_t inst)
{
    if (!fp_enabled(cpu))
        return trap_ex(EX_ILLEGAL_INST, inst);
    uint32_t opcode = inst & 0x7f;
    uint32_t rd = (inst >> 7) & 0x1f;
    uint32_t rs1 = (inst >> 15) & 0x1f;
    uint32_t rs2 = (inst >> 20) & 0x1f;
    uint32_t rs3 = (inst >> 27) & 0x1f;
    uint32_t funct3 = (inst >> 12) & 7;
    uint32_t funct7 = (inst >> 25) & 0x7f;
    uint32_t rm_ok_needed = 0;

    if (opcode == LOAD_FP) {
        uint64_t imm = (uint64_t)((int64_t)(int32_t)inst >> 20);
        uint64_t addr = cpu->regs[rs1] + imm;
        if (funct3 == 2) {
            uint64_t v;
            Trap t = cpu_load(cpu, addr, 32, &v);
            if (t.taken)
                return t;
            cpu->fregs[rd] = box_s((uint32_t)v);
            cpu->csr[MSTATUS] |= MSTATUS_FS_DIRTY;
            return trap_none();
        }
        if (funct3 == 3) {
            uint64_t v;
            Trap t = cpu_load(cpu, addr, 64, &v);
            if (t.taken)
                return t;
            cpu->fregs[rd] = v;
            cpu->csr[MSTATUS] |= MSTATUS_FS_DIRTY;
            return trap_none();
        }
        return trap_ex(EX_ILLEGAL_INST, inst);
    }
    if (opcode == STORE_FP) {
        uint64_t imm = ((uint64_t)((int64_t)(int32_t)(inst & 0xfe000000) >> 20)) |
                       ((inst >> 7) & 0x1f);
        uint64_t addr = cpu->regs[rs1] + imm;
        if (funct3 == 2)
            return cpu_store(cpu, addr, 32, unbox_s(cpu->fregs[rs2]));
        if (funct3 == 3)
            return cpu_store(cpu, addr, 64, cpu->fregs[rs2]);
        return trap_ex(EX_ILLEGAL_INST, inst);
    }

    int is_d = 0;
    if (opcode == MADD || opcode == MSUB || opcode == NMSUB || opcode == NMADD) {
        uint32_t fmt = (inst >> 25) & 3;
        is_d = fmt == 1;
        if (fmt > 1)
            return trap_ex(EX_ILLEGAL_INST, inst);
        if (set_rm(cpu, inst) < 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
        if (!is_d) {
            float a = bits_to_f32(unbox_s(cpu->fregs[rs1]));
            float b = bits_to_f32(unbox_s(cpu->fregs[rs2]));
            float c = bits_to_f32(unbox_s(cpu->fregs[rs3]));
            float r;
            if (opcode == MADD)
                r = fmaf(a, b, c);
            else if (opcode == MSUB)
                r = fmaf(a, b, -c);
            else if (opcode == NMSUB)
                r = fmaf(-a, b, c);
            else
                r = fmaf(-a, b, -c);
            cpu->fregs[rd] = box_s(f32_to_bits(r));
        } else {
            double a = bits_to_f64(cpu->fregs[rs1]);
            double b = bits_to_f64(cpu->fregs[rs2]);
            double c = bits_to_f64(cpu->fregs[rs3]);
            double r;
            if (opcode == MADD)
                r = fma(a, b, c);
            else if (opcode == MSUB)
                r = fma(a, b, -c);
            else if (opcode == NMSUB)
                r = fma(-a, b, c);
            else
                r = fma(-a, b, -c);
            cpu->fregs[rd] = f64_to_bits(r);
        }
        collect_flags(cpu);
        return trap_none();
    }

    if (opcode != OP_FP)
        return trap_ex(EX_ILLEGAL_INST, inst);

    is_d = (funct7 & 3) == 1;
    uint32_t op = funct7 >> 2;

    if (op <= 3 || op == 11 || op == 4 || op == 5 || op == 20 || op == 24 || op == 26 ||
        op == 8) {
        rm_ok_needed = 1;
        if (op == 4 || op == 5 || op == 20)
            rm_ok_needed = 0;
        if (rm_ok_needed && set_rm(cpu, inst) < 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
    }

    switch (op) {
    case 0: /* FADD */
    case 1: /* FSUB */
    case 2: /* FMUL */
    case 3: /* FDIV */ {
        if (!is_d) {
            float a = bits_to_f32(unbox_s(cpu->fregs[rs1]));
            float b = bits_to_f32(unbox_s(cpu->fregs[rs2]));
            float r = (op == 0) ? a + b : (op == 1) ? a - b : (op == 2) ? a * b : a / b;
            cpu->fregs[rd] = box_s(f32_to_bits(r));
        } else {
            double a = bits_to_f64(cpu->fregs[rs1]);
            double b = bits_to_f64(cpu->fregs[rs2]);
            double r = (op == 0) ? a + b : (op == 1) ? a - b : (op == 2) ? a * b : a / b;
            cpu->fregs[rd] = f64_to_bits(r);
        }
        collect_flags(cpu);
        return trap_none();
    }
    case 11: /* FSQRT */ {
        if (rs2 != 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
        if (!is_d) {
            float a = bits_to_f32(unbox_s(cpu->fregs[rs1]));
            cpu->fregs[rd] = box_s(f32_to_bits(sqrtf(a)));
        } else {
            double a = bits_to_f64(cpu->fregs[rs1]);
            cpu->fregs[rd] = f64_to_bits(sqrt(a));
        }
        collect_flags(cpu);
        return trap_none();
    }
    case 4: { /* FSGNJ */
        if (!is_d) {
            uint32_t a = unbox_s(cpu->fregs[rs1]);
            uint32_t b = unbox_s(cpu->fregs[rs2]);
            uint32_t sign;
            if (funct3 == 0)
                sign = b & 0x80000000u;
            else if (funct3 == 1)
                sign = (~b) & 0x80000000u;
            else if (funct3 == 2)
                sign = (a ^ b) & 0x80000000u;
            else
                return trap_ex(EX_ILLEGAL_INST, inst);
            cpu->fregs[rd] = box_s((a & 0x7fffffffu) | sign);
        } else {
            uint64_t a = cpu->fregs[rs1];
            uint64_t b = cpu->fregs[rs2];
            uint64_t sign;
            if (funct3 == 0)
                sign = b & (1ull << 63);
            else if (funct3 == 1)
                sign = (~b) & (1ull << 63);
            else if (funct3 == 2)
                sign = (a ^ b) & (1ull << 63);
            else
                return trap_ex(EX_ILLEGAL_INST, inst);
            cpu->fregs[rd] = (a & ~(1ull << 63)) | sign;
        }
        cpu->csr[MSTATUS] |= MSTATUS_FS_DIRTY;
        return trap_none();
    }
    case 5: { /* FMIN/FMAX */
        if (funct3 > 1)
            return trap_ex(EX_ILLEGAL_INST, inst);
        if (!is_d) {
            float a = bits_to_f32(unbox_s(cpu->fregs[rs1]));
            float b = bits_to_f32(unbox_s(cpu->fregs[rs2]));
            float r = funct3 == 0 ? fminf(a, b) : fmaxf(a, b);
            cpu->fregs[rd] = box_s(f32_to_bits(r));
        } else {
            double a = bits_to_f64(cpu->fregs[rs1]);
            double b = bits_to_f64(cpu->fregs[rs2]);
            double r = funct3 == 0 ? fmin(a, b) : fmax(a, b);
            cpu->fregs[rd] = f64_to_bits(r);
        }
        collect_flags(cpu);
        return trap_none();
    }
    case 20: { /* FCMP */
        if (funct3 > 2)
            return trap_ex(EX_ILLEGAL_INST, inst);
        int r = 0;
        if (!is_d) {
            float a = bits_to_f32(unbox_s(cpu->fregs[rs1]));
            float b = bits_to_f32(unbox_s(cpu->fregs[rs2]));
            if (funct3 == 2)
                r = a == b;
            else if (funct3 == 1)
                r = a < b;
            else
                r = a <= b;
        } else {
            double a = bits_to_f64(cpu->fregs[rs1]);
            double b = bits_to_f64(cpu->fregs[rs2]);
            if (funct3 == 2)
                r = a == b;
            else if (funct3 == 1)
                r = a < b;
            else
                r = a <= b;
        }
        cpu->regs[rd] = (uint64_t)r;
        collect_flags(cpu);
        return trap_none();
    }
    case 8: { /* FCVT.S.D / FCVT.D.S */
        if (set_rm(cpu, inst) < 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
        if (funct7 == 0x20) { /* FCVT.S.D */
            double a = bits_to_f64(cpu->fregs[rs1]);
            cpu->fregs[rd] = box_s(f32_to_bits((float)a));
        } else if (funct7 == 0x21) { /* FCVT.D.S */
            float a = bits_to_f32(unbox_s(cpu->fregs[rs1]));
            cpu->fregs[rd] = f64_to_bits((double)a);
        } else
            return trap_ex(EX_ILLEGAL_INST, inst);
        collect_flags(cpu);
        return trap_none();
    }
    case 24: { /* FCVT.W/WU/L/LU.S/D */
        if (set_rm(cpu, inst) < 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
        int width32 = (rs2 & 2) == 0;
        int unsig = (rs2 & 1) != 0;
        double d;
        if (!is_d)
            d = (double)bits_to_f32(unbox_s(cpu->fregs[rs1]));
        else
            d = bits_to_f64(cpu->fregs[rs1]);
        int64_t r = fcvt_to_l(d, unsig, width32);
        if (width32)
            cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)r;
        else
            cpu->regs[rd] = (uint64_t)r;
        collect_flags(cpu);
        return trap_none();
    }
    case 26: { /* FCVT.S/D.W/WU/L/LU */
        if (set_rm(cpu, inst) < 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
        int width32 = (rs2 & 2) == 0;
        int unsig = (rs2 & 1) != 0;
        double d;
        if (width32) {
            if (unsig)
                d = (double)(uint32_t)cpu->regs[rs1];
            else
                d = (double)(int32_t)cpu->regs[rs1];
        } else {
            if (unsig)
                d = (double)cpu->regs[rs1];
            else
                d = (double)(int64_t)cpu->regs[rs1];
        }
        if (!is_d)
            cpu->fregs[rd] = box_s(f32_to_bits((float)d));
        else
            cpu->fregs[rd] = f64_to_bits(d);
        collect_flags(cpu);
        return trap_none();
    }
    case 28: { /* FMV.X / FCLASS */
        if (funct3 == 1) {
            if (!is_d)
                cpu->regs[rd] = fclass_s(unbox_s(cpu->fregs[rs1]));
            else
                cpu->regs[rd] = fclass_d(cpu->fregs[rs1]);
            return trap_none();
        }
        if (funct3 != 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
        if (!is_d)
            cpu->regs[rd] = (uint64_t)(int64_t)(int32_t)unbox_s(cpu->fregs[rs1]);
        else
            cpu->regs[rd] = cpu->fregs[rs1];
        return trap_none();
    }
    case 30: { /* FMV.W.X / FMV.D.X */
        if (funct3 != 0)
            return trap_ex(EX_ILLEGAL_INST, inst);
        if (!is_d)
            cpu->fregs[rd] = box_s((uint32_t)cpu->regs[rs1]);
        else
            cpu->fregs[rd] = cpu->regs[rs1];
        cpu->csr[MSTATUS] |= MSTATUS_FS_DIRTY;
        return trap_none();
    }
    default:
        return trap_ex(EX_ILLEGAL_INST, inst);
    }
}
