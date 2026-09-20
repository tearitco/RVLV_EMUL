#include "cpu.h"
#include "opcodes.h"

#include <stdio.h>
#include <string.h>

static int g_fail;

static void expect(const char *name, uint64_t got, uint64_t want)
{
    if (got != want) {
        fprintf(stderr, "FAIL %s: got 0x%llx want 0x%llx\n", name,
                (unsigned long long)got, (unsigned long long)want);
        g_fail++;
    }
}

static void poke32(CPU *cpu, uint64_t addr, uint32_t w)
{
    (void)dram_store(&cpu->bus.dram, addr, 32, w);
}

static void poke16(CPU *cpu, uint64_t addr, uint16_t w)
{
    (void)dram_store(&cpu->bus.dram, addr, 16, w);
}

static uint32_t enc_i(uint32_t op, uint32_t f3, uint32_t rd, uint32_t rs1, int32_t imm)
{
    return ((uint32_t)imm << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}

static uint32_t enc_r(uint32_t op, uint32_t f3, uint32_t f7, uint32_t rd, uint32_t rs1,
                      uint32_t rs2)
{
    return (f7 << 25) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | (rd << 7) | op;
}

static uint32_t enc_u(uint32_t op, uint32_t rd, uint32_t imm)
{
    return (imm & 0xfffff000u) | (rd << 7) | op;
}

static uint32_t enc_s(uint32_t op, uint32_t f3, uint32_t rs1, uint32_t rs2, int32_t imm)
{
    uint32_t u = (uint32_t)imm;
    return ((u & 0xfe0) << 20) | (rs2 << 20) | (rs1 << 15) | (f3 << 12) | ((u & 0x1f) << 7) | op;
}

static int setup(CPU *cpu)
{
    if (cpu_init(cpu, 4 * 1024 * 1024, nullptr, 0) < 0)
        return -1;
    cpu->csr[MIE] = 0;
    return 0;
}

int selftest(void)
{
    CPU cpu;
    g_fail = 0;
    if (setup(&cpu) < 0) {
        fprintf(stderr, "cpu_init failed\n");
        return 1;
    }

    poke32(&cpu, DRAM_BASE, enc_i(I_TYPE, 0, 1, 0, 42));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("addi x1,x0,42", cpu.regs[1], 42);

    poke32(&cpu, DRAM_BASE, enc_i(I_TYPE, 0, 2, 1, -5));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("addi x2,x1,-5", cpu.regs[2], 37);

    poke32(&cpu, DRAM_BASE, enc_r(R_TYPE, 0, 0, 3, 1, 2));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("add x3,x1,x2", cpu.regs[3], 79);

    poke32(&cpu, DRAM_BASE, enc_r(R_TYPE, 0, 0x20, 4, 1, 2));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("sub x4,x1,x2", cpu.regs[4], 5);

    poke32(&cpu, DRAM_BASE, enc_r(R_TYPE, 0, 1, 5, 1, 2));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("mul x5,x1,x2", cpu.regs[5], 42 * 37);

    poke32(&cpu, DRAM_BASE, enc_i(I_TYPE_64, 0, 6, 0, -1));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("addiw x6,x0,-1", cpu.regs[6], ~0ull);

    poke32(&cpu, DRAM_BASE, enc_i(I_TYPE, 1, 7, 1, 1));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("slli x7,x1,1", cpu.regs[7], 84);

    poke32(&cpu, DRAM_BASE, enc_u(LUI, 8, 0x12345000));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("lui x8", cpu.regs[8], 0x12345000ull);

    poke32(&cpu, DRAM_BASE + 0x100, 0x11223344);
    cpu.regs[9] = DRAM_BASE + 0x100;
    poke32(&cpu, DRAM_BASE, enc_i(LOAD, 2, 10, 9, 0));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("lw", cpu.regs[10], 0x11223344ull);

    poke32(&cpu, DRAM_BASE, enc_s(S_TYPE, 3, 9, 1, 8));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    uint64_t memv = 0;
    (void)dram_load(&cpu.bus.dram, DRAM_BASE + 0x108, 64, &memv);
    expect("sd", memv, 42);

    cpu.regs[11] = DRAM_BASE + 0x200;
    poke32(&cpu, DRAM_BASE + 0x200, 10);
    poke32(&cpu, DRAM_BASE, enc_r(AMO, 2, 0, 12, 11, 1));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("amoadd.w rd", cpu.regs[12], 10);
    (void)dram_load(&cpu.bus.dram, DRAM_BASE + 0x200, 32, &memv);
    expect("amoadd.w mem", memv, 52);

    poke16(&cpu, DRAM_BASE, 0x0085);
    cpu.pc = DRAM_BASE;
    cpu.regs[1] = 0;
    cpu_step(&cpu);
    expect("c.addi x1,1", cpu.regs[1], 1);

    poke32(&cpu, DRAM_BASE, enc_i(SYSTEM, 2, 13, 0, 0x300));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    if ((cpu.regs[13] & MSTATUS_MPP) == 0)
        g_fail++, fprintf(stderr, "FAIL csrr mstatus\n");

    cpu.regs[1] = 0x3f800000;
    poke32(&cpu, DRAM_BASE, enc_i(OP_FP, 0, 14, 1, 0) | (0x78u << 25));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    poke32(&cpu, DRAM_BASE, enc_i(OP_FP, 0, 15, 1, 0) | (0x78u << 25));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    poke32(&cpu, DRAM_BASE, (15 << 20) | (14 << 15) | (16 << 7) | OP_FP);
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    poke32(&cpu, DRAM_BASE, (0x70u << 25) | (16 << 15) | (17 << 7) | OP_FP);
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("fadd.s 1+1", cpu.regs[17] & 0xffffffffull, 0x40000000ull);

    cpu.regs[1] = ~0ull;
    poke32(&cpu, DRAM_BASE, enc_i(I_TYPE, 5, 2, 1, 0x401));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("srai -1,1", cpu.regs[2], ~0ull);

    poke32(&cpu, DRAM_BASE, enc_r(R_TYPE, 5, 1, 3, 1, 0));
    cpu.regs[1] = 10;
    cpu.regs[0] = 0;
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("divu by 0", cpu.regs[3], ~0ull);

    {
        uint64_t pt = DRAM_BASE + 0x3000;
        uint64_t pte = (1ull << 0) | (1ull << 1) | (1ull << 2) | (1ull << 3) | (1ull << 6) |
                       (1ull << 7) | ((0x80000000ull >> 12) << 10);
        (void)dram_store(&cpu.bus.dram, pt + 2 * 8, 64, pte);
        cpu.csr[SATP] = (8ull << 60) | (pt >> 12);
        poke32(&cpu, DRAM_BASE + 0x200, enc_i(I_TYPE, 0, 21, 0, 77));
        cpu.csr[MEPC] = DRAM_BASE + 0x200;
        cpu.csr[MSTATUS] = (cpu.csr[MSTATUS] & ~MSTATUS_MPP) | ((uint64_t)PRIV_S << 11) |
                           MSTATUS_MPIE;
        poke32(&cpu, DRAM_BASE, enc_i(SYSTEM, 0, 0, 0, 0x302));
        cpu.pc = DRAM_BASE;
        cpu.priv = PRIV_M;
        cpu_step(&cpu);
        expect("mret to S", cpu.priv, PRIV_S);
        expect("mret pc", cpu.pc, DRAM_BASE + 0x200);
        cpu_step(&cpu);
        expect("sv39 fetch addi", cpu.regs[21], 77);
        cpu.csr[SATP] = 0;
        cpu.priv = PRIV_M;
    }

    cpu_destroy(&cpu);
    if (setup(&cpu) < 0)
        return 1;
    cpu.csr[MTVEC] = DRAM_BASE + 0x100;
    cpu.csr[MSTATUS] |= MSTATUS_MIE;
    poke32(&cpu, DRAM_BASE, enc_i(SYSTEM, 0, 0, 0, 0));
    poke32(&cpu, DRAM_BASE + 0x100, enc_i(I_TYPE, 0, 18, 0, 99));
    cpu.pc = DRAM_BASE;
    cpu_step(&cpu);
    expect("ecall mepc", cpu.csr[MEPC], DRAM_BASE);
    expect("ecall priv", cpu.priv, PRIV_M);
    cpu_step(&cpu);
    expect("trap vector addi", cpu.regs[18], 99);

    cpu_destroy(&cpu);
    if (g_fail == 0)
        printf("selftest: all checks passed\n");
    else
        printf("selftest: %d failure(s)\n", g_fail);
    return g_fail ? 1 : 0;
}
