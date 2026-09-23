/* prisc+x_bare.c - Bare-metal rewrite of prisc+x VM
 * Uses musl libc - no POSIX syscalls directly
 * Targets: rvemu bare-metal (RISC-V QEMU virt)
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ===== RVEMU syscall interface ===== */

#define SYS_WRITE   64
#define SYS_READ    63
#define SYS_EXIT    93
#define SYS_BRK     214
#define SYS_OPEN    1024
#define SYS_CLOSE   57
#define SYS_FSTAT   80
#define SYS_LSEEK   62

/* Inline syscall */
static inline uint64_t syscall8(uint64_t a7, uint64_t a0, uint64_t a1, uint64_t a2,
                                 uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    register uint64_t r10 __asm__("a0") = a0;
    register uint64_t r11 __asm__("a1") = a1;
    register uint64_t r12 __asm__("a2") = a2;
    register uint64_t r13 __asm__("a3") = a3;
    register uint64_t r14 __asm__("a4") = a4;
    register uint64_t r15 __asm__("a5") = a5;
    register uint64_t r16 __asm__("a6") = a6;
    register uint64_t r17 __asm__("a7") = a7;
    __asm__ volatile ("ecall" : "+r"(r10) : "r"(r11), "r"(r12), "r"(r13), "r"(r14), "r"(r15), "r"(r16), "r"(r17) : "memory");
    return r10;
}

/* Syscall wrappers */
static uint64_t sys_write(int fd, const void *buf, size_t count) {
    return syscall8(SYS_WRITE, fd, (uint64_t)buf, count, 0, 0, 0, 0);
}

static uint64_t sys_read(int fd, void *buf, size_t count) {
    return syscall8(SYS_READ, fd, (uint64_t)buf, count, 0, 0, 0, 0);
}

static uint64_t sys_open(const char *path, int flags, int mode) {
    return syscall8(SYS_OPEN, (uint64_t)path, flags, mode, 0, 0, 0, 0);
}

static uint64_t sys_close(int fd) {
    return syscall8(SYS_CLOSE, fd, 0, 0, 0, 0, 0, 0);
}

static uint64_t sys_brk(void *addr) {
    return syscall8(SYS_BRK, (uint64_t)addr, 0, 0, 0, 0, 0, 0);
}

static void sys_exit(int code) {
    syscall8(SYS_EXIT, code, 0, 0, 0, 0, 0, 0);
    for(;;); /* never returns */
}

/* UART I/O for early output */
static void uart_putc(char c) {
    sys_write(1, &c, 1);
}

static void uart_puts(const char *s) {
    sys_write(1, s, strlen(s));
}

static void uart_puthex(uint64_t v) {
    char buf[17];
    for (int i = 15; i >= 0; i--) {
        int nibble = (v >> (i * 4)) & 0xF;
        buf[15 - i] = nibble < 10 ? '0' + nibble : 'a' + nibble - 10;
    }
    buf[16] = '\0';
    uart_puts(buf);
}

/* Simple trim - replaces strtok/sscanf usage */
static void trim(char *str) {
    char *start = str;
    while (*start == ' ' || *start == '\t') start++;
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    if (start != str) memmove(str, start, strlen(start) + 1);
}

/* ===== prisc+x VM ===== */

#define MAX_INST 1024
#define MAX_LABELS 128
#define MAX_VARS 256
#define MAX_OPS 64
#define MEM_SIZE 4096
#define STRING_POOL_START 0xF00
#define NUM_SREGS 32
#define SREG_SZ   4096

typedef enum { OP_ADDI, OP_BEQ, OP_BNE, OP_LW, OP_SW, OP_JALR, OP_J, OP_HALT, OP_CUSTOM, OP_READ_HISTORY, OP_EXEC, OP_HIT_FRAME, OP_READ_STATE, OP_READ_ACTIVE_TARGET, OP_READ_ENV_KEY, OP_READ_LAYOUT, OP_READ_POS, OP_ECALL,
    OP_SLIT, OP_SCPY, OP_SAPPEND, OP_SGETENV, OP_SFMT, OP_SREAD, OP_SSPLIT,
    OP_SFIND, OP_SLEN, OP_SFOPEN, OP_SFAPPEND, OP_SWRITE, OP_SFCLOSE,
    OP_SBEQ, OP_SBNE, OP_STRIM, OP_SATOI } OpBase;

typedef struct {
    OpBase op;
    int rd, rs1, rs2, imm;
    char label_ref[32];
    char custom_name[32];
    char literal_arg[1024];
    char literal_arg2[1024];
    int sd, ss1, ss2, ss3;
    char sfmt_toks[8][8];
    int sfmt_n;
} Inst;

typedef struct { char name[32]; int addr; } Label;
typedef struct { char name[32]; int addr; char type[16]; int value; int size; } Variable;
typedef struct { char name[32]; char type[16]; char handler[256]; char desc[128]; } CustomOp;

static int32_t regs[16] = {0};
static int32_t mem[MEM_SIZE] = {0};
static char sregs[NUM_SREGS][SREG_SZ] = {{0}};
static Inst program[MAX_INST];
static Label labels[MAX_LABELS];
static Variable variables[MAX_VARS];
static CustomOp custom_ops[MAX_OPS];
static int label_count = 0, inst_count = 0, var_count = 0, op_count = 0;
static int next_var_addr = 0;
static char g_pal_dir[4096] = "";

#include <ctype.h>

static int sreg_idx(const char *tok) {
    while (*tok == ' ' || *tok == '\t') tok++;
    int n = -1;
    if (sscanf(tok, "s%d", &n) == 1 && n >= 0 && n < NUM_SREGS) return n;
    return -1;
}

static int find_custom_op(const char *name) {
    for (int i = 0; i < op_count; i++)
        if (strcmp(custom_ops[i].name, name) == 0) return i;
    return -1;
}

static void add_custom_op(const char *name, const char *type, const char *handler, const char *desc) {
    int existing = find_custom_op(name);
    CustomOp *c = (existing >= 0) ? &custom_ops[existing] : (op_count < MAX_OPS ? &custom_ops[op_count++] : NULL);
    if (!c) return;
    strncpy(c->name, name, 31);
    strncpy(c->type, type, 15);
    strncpy(c->handler, handler, 255);
    strncpy(c->desc, desc, 127);
}

static int find_label(const char *name) {
    for (int i = 0; i < label_count; i++)
        if (strcmp(labels[i].name, name) == 0) return labels[i].addr;
    return -1;
}

static void parse_ops_file(const char *path) {
    /* Minimal: only builtin ops */
    add_custom_op("builtin_out", "builtin", "builtin_out", "Print register");
    add_custom_op("builtin_halt", "builtin", "builtin_halt", "Halt VM");
}

static void parse_line(char *line, int pass) {
    char original[1024];
    strncpy(original, line, sizeof(original) - 1);
    original[sizeof(original) - 1] = '\0';

    for (int _pass = 0; _pass < 2; _pass++) {
        char *buf = _pass == 0 ? line : original;
        int inq = 0, seen = 0;
        for (char *c = buf; *c; c++) {
            if (*c == '"') { inq = !inq; seen = 1; }
            else if (*c == '#' && !inq && seen) { *c = '\0'; break; }
            else if (!isspace((unsigned char)*c)) seen = 1;
        }
    }
    trim(line);
    if (!*line || *line == '#') return;

    char part[32], *p = line;
    if (sscanf(p, "%s", part) != 1) return;

    if (part[strlen(part) - 1] == ':') {
        if (pass == 1) {
            part[strlen(part) - 1] = '\0';
            strcpy(labels[label_count].name, part);
            labels[label_count++].addr = inst_count;
        }
        return;
    }

    if (pass == 2) {
        Inst *i = &program[inst_count];
        memset(i, 0, sizeof(Inst));

        int op_idx = find_custom_op(part);
        if (op_idx >= 0) {
            i->op = OP_CUSTOM;
            strcpy(i->custom_name, part);
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            if (sscanf(args, "x%d", &i->rs1) != 1) i->rs1 = 0;
        } else if (strcmp(part, "addi") == 0) {
            int r_rd, r_rs1;
            sscanf(line, "%*s x%d, x%d, %d", &r_rd, &r_rs1, &i->imm);
            i->rd = r_rd; i->rs1 = r_rs1; i->op = OP_ADDI;
        } else if (strcmp(part, "beq") == 0) {
            sscanf(line, "%*s x%d, x%d, %s", &i->rs1, &i->rs2, i->label_ref);
            i->op = OP_BEQ;
        } else if (strcmp(part, "bne") == 0) {
            sscanf(line, "%*s x%d, x%d, %s", &i->rs1, &i->rs2, i->label_ref);
            i->op = OP_BNE;
        } else if (strcmp(part, "li") == 0) {
            sscanf(line, "%*s x%d, %d", &i->rd, &i->imm);
            i->rs1 = 0; i->op = OP_ADDI;
        } else if (strcmp(part, "lw") == 0) {
            sscanf(line, "%*s x%d, %d(x%d)", &i->rd, &i->imm, &i->rs1);
            i->op = OP_LW;
        } else if (strcmp(part, "sw") == 0) {
            sscanf(line, "%*s x%d, %d(x%d)", &i->rs2, &i->imm, &i->rs1);
            i->op = OP_SW;
        } else if (strcmp(part, "jalr") == 0) {
            sscanf(line, "%*s x%d, x%d, %d", &i->rd, &i->rs1, &i->imm);
            i->op = OP_JALR;
        } else if (strcmp(part, "j") == 0) {
            sscanf(line, "%*s %s", i->label_ref);
            i->op = OP_J;
        } else if (strcmp(part, "halt") == 0) {
            i->op = OP_HALT;
        } else if (strcmp(part, "ecall") == 0) {
            char *args = strstr(original, part) + strlen(part);
            while (*args == ' ' || *args == '\t') args++;
            sscanf(args, "\"%1023[^\"]\" \"%1023[^\"]\"", i->literal_arg, i->literal_arg2);
            i->op = OP_ECALL;
        }
        inst_count++;
    }
}

static void exec_ecall(Inst *i) {
    int sysnum = regs[15];
    switch (sysnum) {
        case 1: /* SYS_OPEN */ {
            const char *mode = (regs[13] == 1) ? "w" : (regs[13] == 2) ? "a" : "r";
            (void)mode; /* bare-metal: just return fd=0 for now */
            regs[12] = 0;
            break;
        }
        case 2: /* SYS_CLOSE */ regs[12] = 0; break;
        case 3: /* SYS_WRITE_LINE */ {
            sys_write(regs[12], i->literal_arg, strlen(i->literal_arg));
            regs[12] = strlen(i->literal_arg);
            break;
        }
        case 4: /* SYS_WRITE_INT */ break;
        case 5: /* SYS_READ_INT */ regs[12] = -1; break;
        case 6: /* SYS_GET_KV_INT */ regs[12] = regs[13]; break;
        case 7: /* SYS_SET_KV_INT */ regs[12] = 1; break;
        default: regs[12] = -1;
    }
}

static void exec_custom_op(Inst *i) {
    if (strcmp(i->custom_name, "builtin_out") == 0) {
        uart_puthex(regs[i->rs1]);
        uart_puts("\n");
    }
    /* builtin_halt does nothing */
}

int main(int argc, char **argv) {
    if (argc < 2) {
        uart_puts("Usage: prisc_x <prog> [mem_in] [mem_out] [ops_file]\n");
        return 1;
    }

    /* Parse ops file (minimal builtin) */
    parse_ops_file("default_op.txt");
    if (argc > 4) parse_ops_file(argv[4]);

    /* Read program */
    int fd = sys_open(argv[1], 0, 0);
    if (fd < 0) {
        uart_puts("[Prisc Error] Could not open program file: ");
        uart_puts(argv[1]);
        uart_puts("\n");
        return 1;
    }

    /* Read file into buffer */
    char line[256];
    char buf[65536];
    size_t buf_pos = 0;
    while (1) {
        uint64_t n = sys_read(fd, buf + buf_pos, sizeof(buf) - buf_pos - 1);
        if (n == 0 || n == (uint64_t)-1) break;
        buf_pos += n;
        if (buf_pos >= sizeof(buf) - 1) break;
    }
    buf[buf_pos] = '\0';
    sys_close(fd);

    /* First pass: labels */
    char *line_ptr = buf;
    inst_count = 0;
    while (*line_ptr) {
        char *nl = strchr(line_ptr, '\n');
        if (nl) { *nl = '\0'; nl++; }
        strcpy(line, line_ptr);
        parse_line(line, 1);
        if (!nl) break;
        line_ptr = nl;
    }

    /* Second pass: instructions */
    line_ptr = buf;
    inst_count = 0;
    while (*line_ptr) {
        char *nl = strchr(line_ptr, '\n');
        if (nl) { *nl = '\0'; nl++; }
        strcpy(line, line_ptr);
        parse_line(line, 2);
        if (!nl) break;
        line_ptr = nl;
    }

    /* Load memory if provided */
    if (argc > 2) {
        fd = sys_open(argv[2], 0, 0);
        if (fd >= 0) {
            char mbuf[1024];
            while (1) {
                uint64_t n = sys_read(fd, mbuf, sizeof(mbuf));
                if (n == 0 || n == (uint64_t)-1) break;
                /* parse "addr val" lines */
            }
            sys_close(fd);
        }
    }

    /* Execute */
    int pc = 0;
    while (pc < inst_count) {
        Inst i = program[pc];
        regs[0] = 0;
        int next_pc = pc + 1;

        if (i.op == OP_CUSTOM) {
            exec_custom_op(&i);
        } else if (i.op == OP_ECALL) {
            exec_ecall(&i);
        } else if (i.op == OP_HALT) {
            break;
        } else {
            switch (i.op) {
                case OP_ADDI: regs[i.rd] = regs[i.rs1] + i.imm; break;
                case OP_BEQ: {
                    int target = find_label(i.label_ref);
                    if (regs[i.rs1] == regs[i.rs2]) next_pc = target;
                    break;
                }
                case OP_BNE: {
                    int target = find_label(i.label_ref);
                    if (regs[i.rs1] != regs[i.rs2]) next_pc = target;
                    break;
                }
                case OP_J: {
                    int target = find_label(i.label_ref);
                    next_pc = target;
                    break;
                }
                case OP_LW: regs[i.rd] = mem[(regs[i.rs1] + i.imm) % MEM_SIZE]; break;
                case OP_SW: mem[(regs[i.rs1] + i.imm) % MEM_SIZE] = regs[i.rs2]; break;
                case OP_JALR: { int tmp = pc + 1; next_pc = regs[i.rs1] + i.imm; regs[i.rd] = tmp; break; }
                default: break;
            }
        }
        pc = next_pc;
    }

    /* Save memory if provided */
    if (argc > 3) {
        fd = sys_open(argv[3], 577, 0644); /* O_WRONLY|O_CREAT|O_TRUNC */
        if (fd >= 0) {
            for (int i = 0; i < MEM_SIZE; i++) {
                if (mem[i] != 0) {
                    char mbuf[64];
                    int len = 0;
                    /* write "addr val\n" */
                    uint64_t val = mem[i];
                    char *p = mbuf;
                    if (val == 0) *p++ = '0';
                    else {
                        char tmp[32]; int t = 0;
                        while (val) { tmp[t++] = '0' + val % 10; val /= 10; }
                        while (t--) *p++ = tmp[t];
                    }
                    *p++ = ' '; *p++ = '0' + (i / 1000); *p++ = '0' + (i / 100 % 10); *p++ = '0' + (i / 10 % 10); *p++ = '0' + (i % 10);
                    *p++ = '\n';
                    sys_write(fd, mbuf, p - mbuf);
                }
            }
            sys_close(fd);
        }
    }
    return 0;
}