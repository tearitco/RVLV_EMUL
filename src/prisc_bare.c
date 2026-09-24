/* prisc+x_bare.c - Bare-metal rewrite of prisc+x VM
 * No libc dependencies - minimal inline everything
 * Targets: rvemu bare-metal (RISC-V QEMU virt)
 */

/* ===== Minimal type definitions ===== */
typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef int int32_t;
typedef long int64_t;
#define NULL ((void*)0)

/* ===== Minimal libc replacements ===== */

static size_t strlen(const char *s) {
    size_t n = 0;
    while (s[n]) n++;
    return n;
}

static void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}

static void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

static int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++));
    return dest;
}

static char *strcat(char *dest, const char *src) {
    char *d = dest + strlen(dest);
    while ((*d++ = *src++));
    return dest;
}

static char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char*)haystack;
    for (; *haystack; haystack++) {
        const char *h = haystack, *n = needle;
        while (*h && *n && *h == *n) { h++; n++; }
        if (!*n) return (char*)haystack;
    }
    return NULL;
}

static int atoi(const char *s) {
    int n = 0, neg = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') n = n * 10 + (*s++ - '0');
    return neg ? -n : n;
}

static int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int isdigit(int c) { return c >= '0' && c <= '9'; }

/* Missing string/memory functions */
static void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

static char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    while (n-- && (*d++ = *src++));
    while (n--) *d++ = '\0';
    return dest;
}

static char *strchr(const char *s, int c) {
    while (*s && *s != (char)c) s++;
    return *s == (char)c ? (char*)s : NULL;
}

/* va_list for variadic functions - must be before sscanf */
typedef __builtin_va_list va_list;
#define va_start(v, l) __builtin_va_start(v, l)
#define va_end(v) __builtin_va_end(v)
#define va_arg(v, t) __builtin_va_arg(v, t)

/* Minimal sscanf - only supports %s, %d, %x, %*s, %*d formats used in parser */
static int sscanf(const char *str, const char *format, ...) {
    va_list args;
    va_start(args, format);
    int count = 0;
    const char *s = str;
    const char *f = format;
    
    while (*f && *s) {
        while (*f == ' ' || *f == '\t' || *f == '\n') f++;
        while (*s == ' ' || *s == '\t' || *s == '\n') s++;
        
        if (*f != '%') {
            if (*f != *s) break;
            f++; s++;
            continue;
        }
        
        f++;
        if (*f == '*') { f++; /* skip assignment */ }
        
        int width = 0;
        while (*f >= '0' && *f <= '9') {
            width = width * 10 + (*f - '0');
            f++;
        }
        
        if (*f == 's') {
            char *dest = va_arg(args, char*);
            int i = 0;
            while (*s && *s != ' ' && *s != '\t' && *s != '\n' && (width == 0 || i < width)) {
                dest[i++] = *s++;
            }
            dest[i] = '\0';
            count++;
        } else if (*f == 'd') {
            int *dest = va_arg(args, int*);
            int neg = 0, val = 0;
            while (*s == ' ' || *s == '\t') s++;
            if (*s == '-') { neg = 1; s++; }
            while (*s >= '0' && *s <= '9' && (width == 0 || val < 1000000)) {
                val = val * 10 + (*s++ - '0');
            }
            *dest = neg ? -val : val;
            count++;
        } else if (*f == 'x') {
            int *dest = va_arg(args, int*);
            int val = 0;
            while (*s == ' ' || *s == '\t') s++;
            while ((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'f') || (*s >= 'A' && *s <= 'F')) {
                int digit = 0;
                if (*s >= '0' && *s <= '9') digit = *s - '0';
                else if (*s >= 'a' && *s <= 'f') digit = 10 + (*s - 'a');
                else digit = 10 + (*s - 'A');
                val = val * 16 + digit;
                s++;
            }
            *dest = val;
            count++;
        } else if (*f == '[') {
            /* %[^"] format - simplified */
            f++; /* skip [ */
            char exclude = 0;
            if (*f == '^') { exclude = 1; f++; }
            char end_char = *f; f++; /* get the char before ] */
            f++; /* skip ] */
            
            char *dest = va_arg(args, char*);
            int i = 0;
            while (*s && (exclude ? (*s != end_char) : (*s == end_char)) && (width == 0 || i < width)) {
                dest[i++] = *s++;
            }
            dest[i] = '\0';
            count++;
        } else {
            break;
        }
        f++;
    }
    va_end(args);
    return count;
}

/* ===== RVEMU syscall interface ===== */

#define SYS_WRITE   64
#define SYS_READ    63
#define SYS_EXIT    93
#define SYS_BRK     214
#define SYS_OPEN    1024
#define SYS_CLOSE   57
#define SYS_FSTAT   80
#define SYS_LSEEK   62
#define SYS_SPAWN   400  /* Custom: spawn ELF binary */

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

static uint64_t sys_spawn(const char *path, char *const argv[], char *const envp[]) {
    return syscall8(SYS_SPAWN, (uint64_t)path, (uint64_t)argv, (uint64_t)envp, 0, 0, 0, 0);
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

/* Forward declarations for functions used before definition */
static int sscanf(const char *str, const char *format, ...);
static void *memset(void *s, int c, size_t n);
static char *strncpy(char *dest, const char *src, size_t n);
static char *strchr(const char *s, int c);

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
    add_custom_op("builtin_spawn", "builtin", "builtin_spawn", "Spawn ELF binary");
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
        } else if (strcmp(part, "slit") == 0) {
            /* SLIT sd, "literal" - store literal in string register sd */
            sscanf(line, "%*s s%d, \"%1023[^\"]\"", &i->sd, i->literal_arg);
            i->op = OP_SLIT;
        } else if (strcmp(part, "scpy") == 0) {
            /* SCPY sd, ss1 - copy string register ss1 to sd */
            sscanf(line, "%*s s%d, s%d", &i->sd, &i->ss1);
            i->op = OP_SCPY;
        } else if (strcmp(part, "sappend") == 0) {
            /* SAPPEND sd, ss1 - append ss1 to sd */
            sscanf(line, "%*s s%d, s%d", &i->sd, &i->ss1);
            i->op = OP_SAPPEND;
        } else if (strcmp(part, "sfmt") == 0) {
            /* SFMT sd, ss1, ss2, ... - format string with args */
            sscanf(line, "%*s s%d, s%d, s%d, s%d", &i->sd, &i->ss1, &i->ss2, &i->ss3);
            i->op = OP_SFMT;
        } else if (strcmp(part, "sread") == 0) {
            /* SREAD sd, ss1, len - read len bytes from file ss1 into sd */
            sscanf(line, "%*s s%d, s%d, %d", &i->sd, &i->ss1, &i->imm);
            i->op = OP_SREAD;
        } else if (strcmp(part, "ssplit") == 0) {
            /* SSPLIT sd, ss1, "delim" - split ss1 by delim into sd (array-like) */
            sscanf(line, "%*s s%d, s%d, \"%1023[^\"]\"", &i->sd, &i->ss1, i->literal_arg);
            i->op = OP_SSPLIT;
        } else if (strcmp(part, "sfind") == 0) {
            /* SFIND sd, ss1, "substr" - find substr in ss1, store index in sd (as int) */
            sscanf(line, "%*s s%d, s%d, \"%1023[^\"]\"", &i->sd, &i->ss1, i->literal_arg);
            i->op = OP_SFIND;
        } else if (strcmp(part, "slen") == 0) {
            /* SLEN sd, ss1 - store length of ss1 in sd (as int) */
            sscanf(line, "%*s s%d, s%d", &i->sd, &i->ss1);
            i->op = OP_SLEN;
        } else if (strcmp(part, "sfopen") == 0) {
            /* SFOPEN sd, ss1, "mode" - open file ss1 with mode, store fd in sd */
            sscanf(line, "%*s s%d, s%d, \"%1023[^\"]\"", &i->sd, &i->ss1, i->literal_arg);
            i->op = OP_SFOPEN;
        } else if (strcmp(part, "sfappend") == 0) {
            /* SFAPPEND ss1, ss2 - append string ss2 to file ss1 */
            sscanf(line, "%*s s%d, s%d", &i->ss1, &i->ss2);
            i->op = OP_SFAPPEND;
        } else if (strcmp(part, "swrite") == 0) {
            /* SWRITE ss1, ss2 - write string ss2 to file ss1 */
            sscanf(line, "%*s s%d, s%d", &i->ss1, &i->ss2);
            i->op = OP_SWRITE;
        } else if (strcmp(part, "sfclose") == 0) {
            /* SFCLOSE ss1 - close file ss1 */
            sscanf(line, "%*s s%d", &i->ss1);
            i->op = OP_SFCLOSE;
        } else if (strcmp(part, "sbeq") == 0) {
            /* SBEQ ss1, ss2, label - branch if string eq */
            sscanf(line, "%*s s%d, s%d, %s", &i->ss1, &i->ss2, i->label_ref);
            i->op = OP_SBEQ;
        } else if (strcmp(part, "sbne") == 0) {
            /* SBNE ss1, ss2, label - branch if string ne */
            sscanf(line, "%*s s%d, s%d, %s", &i->ss1, &i->ss2, i->label_ref);
            i->op = OP_SBNE;
        } else if (strcmp(part, "strim") == 0) {
            /* STRIM sd, ss1 - trim whitespace from ss1 into sd */
            sscanf(line, "%*s s%d, s%d", &i->sd, &i->ss1);
            i->op = OP_STRIM;
        } else if (strcmp(part, "satoi") == 0) {
            /* SATOI sd, ss1 - convert string ss1 to int, store in sd */
            sscanf(line, "%*s s%d, s%d", &i->sd, &i->ss1);
            i->op = OP_SATOI;
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
    } else if (strcmp(i->custom_name, "builtin_spawn") == 0) {
        /* builtin_spawn x10 - spawn ELF at path in sreg[x10] */
        const char *path = sregs[regs[i->rs1]];
        sys_spawn(path, NULL, NULL);
    }
    /* builtin_halt does nothing */
}

/* String operations */
static void sreg_clear(int sd) {
    sregs[sd][0] = '\0';
}

static void sreg_copy(int sd, int ss) {
    strcpy(sregs[sd], sregs[ss]);
}

static void sreg_append(int sd, int ss) {
    strcat(sregs[sd], sregs[ss]);
}

static void exec_slit(Inst *i) {
    sreg_clear(i->sd);
    strcpy(sregs[i->sd], i->literal_arg);
}

static void exec_scpy(Inst *i) {
    sreg_copy(i->sd, i->ss1);
}

static void exec_sappend(Inst *i) {
    sreg_append(i->sd, i->ss1);
}

static void exec_sfmt(Inst *i) {
    /* Simple format: just concatenate for now */
    sreg_clear(i->sd);
    if (i->ss1 >= 0) strcat(sregs[i->sd], sregs[i->ss1]);
    if (i->ss2 >= 0) strcat(sregs[i->sd], sregs[i->ss2]);
    if (i->ss3 >= 0) strcat(sregs[i->sd], sregs[i->ss3]);
}

static void exec_sread(Inst *i) {
    /* Read from file (fd in sregs[ss1]) into sd */
    int fd = atoi(sregs[i->ss1]);
    char buf[1024];
    uint64_t n = sys_read(fd, buf, i->imm > 0 ? i->imm : sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        sreg_clear(i->sd);
        strcpy(sregs[i->sd], buf);
    } else {
        sreg_clear(i->sd);
    }
}

static void exec_ssplit(Inst *i) {
    /* Simple: just copy for now - full split needs array support */
    sreg_copy(i->sd, i->ss1);
}

static void exec_sfind(Inst *i) {
    char *found = strstr(sregs[i->ss1], i->literal_arg);
    int idx = found ? (int)(found - sregs[i->ss1]) : -1;
    /* Store as string representation of int */
    char buf[32];
    int n = idx, neg = 0, pos = 0;
    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) buf[pos++] = '0';
    else {
        char tmp[32]; int t = 0;
        while (n) { tmp[t++] = '0' + n % 10; n /= 10; }
        if (neg) buf[pos++] = '-';
        while (t--) buf[pos++] = tmp[t];
    }
    buf[pos] = '\0';
    sreg_clear(i->sd);
    strcpy(sregs[i->sd], buf);
}

static void exec_slen(Inst *i) {
    int len = (int)strlen(sregs[i->ss1]);
    char buf[32];
    int n = len, pos = 0;
    if (n == 0) buf[pos++] = '0';
    else {
        char tmp[32]; int t = 0;
        while (n) { tmp[t++] = '0' + n % 10; n /= 10; }
        while (t--) buf[pos++] = tmp[t];
    }
    buf[pos] = '\0';
    sreg_clear(i->sd);
    strcpy(sregs[i->sd], buf);
}

static void exec_sfopen(Inst *i) {
    int flags = 0;
    if (strcmp(i->literal_arg, "w") == 0) flags = 577; /* O_WRONLY|O_CREAT|O_TRUNC */
    else if (strcmp(i->literal_arg, "a") == 0) flags = 1089; /* O_WRONLY|O_CREAT|O_APPEND */
    else flags = 0; /* O_RDONLY */
    uint64_t fd = sys_open(sregs[i->ss1], flags, 0644);
    char buf[32];
    int n = (int)fd, pos = 0;
    if (n == 0) buf[pos++] = '0';
    else {
        char tmp[32]; int t = 0;
        while (n) { tmp[t++] = '0' + n % 10; n /= 10; }
        while (t--) buf[pos++] = tmp[t];
    }
    buf[pos] = '\0';
    sreg_clear(i->sd);
    strcpy(sregs[i->sd], buf);
}

static void exec_sfappend(Inst *i) {
    int fd = atoi(sregs[i->ss1]);
    sys_write(fd, sregs[i->ss2], strlen(sregs[i->ss2]));
}

static void exec_swrite(Inst *i) {
    int fd = atoi(sregs[i->ss1]);
    sys_write(fd, sregs[i->ss2], strlen(sregs[i->ss2]));
}

static void exec_sfclose(Inst *i) {
    int fd = atoi(sregs[i->ss1]);
    sys_close(fd);
}

static void exec_sbeq(Inst *i) {
    int target = find_label(i->label_ref);
    if (strcmp(sregs[i->ss1], sregs[i->ss2]) == 0) {
        /* PC will be set by main loop */
        regs[15] = target; /* Use r15 as temp for branch target */
    }
}

static void exec_sbne(Inst *i) {
    int target = find_label(i->label_ref);
    if (strcmp(sregs[i->ss1], sregs[i->ss2]) != 0) {
        regs[15] = target;
    }
}

static void exec_strim(Inst *i) {
    char *s = sregs[i->ss1];
    char *start = s;
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) end--;
    end[1] = '\0';
    sreg_clear(i->sd);
    strcpy(sregs[i->sd], start);
}

static void exec_satoi(Inst *i) {
    int val = atoi(sregs[i->ss1]);
    char buf[32];
    int n = val, neg = 0, pos = 0;
    if (n < 0) { neg = 1; n = -n; }
    if (n == 0) buf[pos++] = '0';
    else {
        char tmp[32]; int t = 0;
        while (n) { tmp[t++] = '0' + n % 10; n /= 10; }
        if (neg) buf[pos++] = '-';
        while (t--) buf[pos++] = tmp[t];
    }
    buf[pos] = '\0';
    sreg_clear(i->sd);
    strcpy(sregs[i->sd], buf);
}

static void exec_string_op(Inst *i) {
    switch (i->op) {
        case OP_SLIT:    exec_slit(i);    break;
        case OP_SCPY:    exec_scpy(i);    break;
        case OP_SAPPEND: exec_sappend(i); break;
        case OP_SFMT:    exec_sfmt(i);    break;
        case OP_SREAD:   exec_sread(i);   break;
        case OP_SSPLIT:  exec_ssplit(i);  break;
        case OP_SFIND:   exec_sfind(i);   break;
        case OP_SLEN:    exec_slen(i);    break;
        case OP_SFOPEN:  exec_sfopen(i);  break;
        case OP_SFAPPEND:exec_sfappend(i);break;
        case OP_SWRITE:  exec_swrite(i);  break;
        case OP_SFCLOSE: exec_sfclose(i); break;
        case OP_SBEQ:    exec_sbeq(i);    break;
        case OP_SBNE:    exec_sbne(i);    break;
        case OP_STRIM:   exec_strim(i);   break;
        case OP_SATOI:   exec_satoi(i);   break;
        default: break;
    }
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
        } else if (i.op >= OP_SLIT && i.op <= OP_SATOI) {
            exec_string_op(&i);
            /* Handle string branch ops */
            if (i.op == OP_SBEQ || i.op == OP_SBNE) {
                if (regs[15] != 0) {
                    next_pc = regs[15];
                    regs[15] = 0;
                }
            }
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