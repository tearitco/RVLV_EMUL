/*
 * pnr.c - XO.SFPGA.NN Place & Route Tool (standalone executable)
 *
 * Parses HDL0 source (named-signal text format) and emits a resolved
 * bitstream file (physical wire indices) that fpga.c can load.
 *
 * Build: gcc -o pnr pnr.c
 * Usage: ./pnr <input.hdl0> <output.bit>
 *
 * HDL0 input format:
 *   .LUT name [in0, in1, ...] = {tt0, tt1, ...}
 *   .PIN name = signal_name
 *   .CONFIG name = value
 *   .CONNECT src -> dst
 *   .END
 *
 * Bitstream output format (consumed by fpga.c):
 *   .FABRIC rows cols
 *   .LUT clb_row clb_col lut_idx n_inputs in0 in1 ... out_wire tt0,tt1,...
 *   .PIN pin_id dir wire
 *   .CONFIG name value
 *   .CONNECT src_wire dst_wire
 *   .END
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_LUTS     128
#define MAX_REGS     64
#define MAX_CONNS    128
#define MAX_PINS     16
#define MAX_SIGNS    256
#define MAX_NAME     64
#define MAX_WIRES    256
#define MAX_LUT_INPUTS 8

typedef struct {
    char name[MAX_NAME];
    uint8_t wire_id;
    uint8_t is_input;
    uint8_t is_output;
} signal_t;

typedef struct {
    uint8_t clb_row;
    uint8_t clb_col;
    uint8_t lut_idx;
    uint8_t n_inputs;
    char inputs[MAX_LUT_INPUTS][MAX_NAME];
    char output_name[MAX_NAME];
    uint8_t truth_table[16];
    uint8_t tt_count;
} pnr_lut_t;

typedef struct {
    uint8_t clb_row;
    uint8_t clb_col;
    uint8_t reg_idx;
    char d_inputs[4][MAX_NAME];
    char clk_src[MAX_NAME];
    char output_name[MAX_NAME];
    uint8_t clk_en;
} pnr_reg_t;

typedef struct {
    char src[MAX_NAME];
    char dst[MAX_NAME];
} conn_t;

typedef struct {
    char name[MAX_NAME];
    uint8_t pin_id;
    uint8_t dir;
    char signal[MAX_NAME];
} pin_t;

typedef struct {
    pnr_lut_t luts[MAX_LUTS];
    uint8_t n_luts;
    pnr_reg_t regs[MAX_REGS];
    uint8_t n_regs;
    conn_t connects[MAX_CONNS];
    uint8_t n_connects;
    pin_t pins[MAX_PINS];
    uint8_t n_pins;
    signal_t symbols[MAX_SIGNS];
    uint16_t n_symbols;
    uint16_t next_wire;
    uint8_t clk_wire;
    uint8_t rows;
    uint8_t cols;
    char config_name[MAX_NAME];
    int32_t config_value;
    uint8_t has_config;
} ctx_t;

static char *skip_ws_ret(char *p) {
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static char *parse_word(char *p, char *out, size_t out_sz) {
    p = skip_ws_ret(p);
    size_t i = 0;
    while (*p && !isspace((unsigned char)*p) && i < out_sz - 1)
        out[i++] = *p++;
    out[i] = '\0';
    return p;
}

static char *parse_signal_list(char *p, char inputs[][MAX_NAME], int *count, int max) {
    p = skip_ws_ret(p);
    if (*p == '[') p++;
    *count = 0;
    while (*p && *p != ']' && *count < max) {
        p = skip_ws_ret(p);
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }
        char tmp[MAX_NAME];
        size_t i = 0;
        while (*p && *p != ',' && *p != ']' && !isspace((unsigned char)*p) && i < MAX_NAME - 1)
            tmp[i++] = *p++;
        tmp[i] = '\0';
        if (i > 0 && *count < max) strcpy(inputs[(*count)++], tmp);
    }
    if (*p == ']') p++;
    return p;
}

static char *parse_braced_tt(char *p, uint8_t tt[16], int max) {
    p = skip_ws_ret(p);
    if (*p == '{') p++;
    int idx = 0;
    while (*p && *p != '}' && idx < max) {
        p = skip_ws_ret(p);
        if (*p == '}' || !*p) break;
        if (*p == ',') { p++; continue; }
        char *end = p;
        long v = strtol(p, &end, 0);
        if (end != p) {
            tt[idx++] = (uint8_t)(v & 0xFF);
            p = end;
        } else p++;
    }
    while (idx < 16) tt[idx++] = 0;
    return p;
}

static uint8_t resolve_signal(ctx_t *ctx, const char *name) {
    if (!name || !*name) return 0;
    if (strcmp(name, "0") == 0) return 0;
    if (strcmp(name, "1") == 0) return 1;
    if (strcmp(name, "clk") == 0) {
        if (!ctx->clk_wire) ctx->clk_wire = ctx->next_wire++;
        return ctx->clk_wire;
    }
    size_t len = strlen(name);
    if (len >= 4 && strcmp(name + len - 4, ".out") == 0) {
        char stripped[MAX_NAME];
        size_t slen = len - 4;
        if (slen >= MAX_NAME) slen = MAX_NAME - 1;
        memcpy(stripped, name, slen);
        stripped[slen] = '\0';
        for (int i = 0; i < ctx->n_symbols; i++) {
            if (strcmp(ctx->symbols[i].name, stripped) == 0)
                return ctx->symbols[i].wire_id;
        }
        name = stripped;
    }
    if (len >= 3 && strcmp(name + len - 3, ".q") == 0) {
        char stripped[MAX_NAME];
        size_t slen = len - 3;
        if (slen >= MAX_NAME) slen = MAX_NAME - 1;
        memcpy(stripped, name, slen);
        stripped[slen] = '\0';
        for (int i = 0; i < ctx->n_symbols; i++) {
            if (strcmp(ctx->symbols[i].name, stripped) == 0)
                return ctx->symbols[i].wire_id;
        }
        name = stripped;
    }
    for (int i = 0; i < ctx->n_symbols; i++) {
        if (strcmp(ctx->symbols[i].name, name) == 0)
            return ctx->symbols[i].wire_id;
    }
    if (ctx->n_symbols >= MAX_SIGNS) return 0;
    signal_t *s = &ctx->symbols[ctx->n_symbols++];
    strncpy(s->name, name, MAX_NAME - 1);
    s->wire_id = ctx->next_wire++;
    if (ctx->next_wire >= MAX_WIRES) ctx->next_wire = MAX_WIRES - 1;
    return s->wire_id;
}

static int parse_hdl0(ctx_t *ctx, const char *filename) {
    memset(ctx, 0, sizeof(ctx_t));
    ctx->next_wire = 2;
    ctx->rows = 4;
    ctx->cols = 4;

    resolve_signal(ctx, "clk");
    resolve_signal(ctx, "0");
    resolve_signal(ctx, "1");

    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof(line), f)) {
        char *p = skip_ws_ret(line);
        if (*p == '#' || *p == '\0' || *p == '\n') continue;

        char keyword[MAX_NAME];
        p = parse_word(p, keyword, sizeof(keyword));
        if (keyword[0] == '\0') continue;

        if (strcmp(keyword, ".LUT") == 0 || strcmp(keyword, ".LUT3") == 0 ||
            strcmp(keyword, ".LUT4") == 0) {
            if (ctx->n_luts >= MAX_LUTS) continue;
            pnr_lut_t *lut = &ctx->luts[ctx->n_luts++];
            p = parse_word(p, lut->output_name, MAX_NAME);
            char inputs[MAX_LUT_INPUTS][MAX_NAME];
            int n_in = 0;
            p = parse_signal_list(p, inputs, &n_in, MAX_LUT_INPUTS);
            for (int i = 0; i < n_in && i < MAX_LUT_INPUTS; i++)
                strncpy(lut->inputs[i], inputs[i], MAX_NAME - 1);
            lut->n_inputs = (uint8_t)n_in;
            p = parse_braced_tt(p, lut->truth_table, 16);
            if (n_in <= 3) lut->tt_count = 8;
            else lut->tt_count = 16;
            continue;
        }

        if (strcmp(keyword, ".REG") == 0) {
            if (ctx->n_regs >= MAX_REGS) { while (*p && *p != '\n') p++; continue; }
            pnr_reg_t *reg = &ctx->regs[ctx->n_regs++];
            memset(reg, 0, sizeof(pnr_reg_t));
            p = parse_word(p, reg->output_name, MAX_NAME);
            char dlist[MAX_NAME * 4];
            int d_count = 0;
            p = parse_signal_list(p, (char (*)[MAX_NAME])dlist, &d_count, 4);
            for (int i = 0; i < d_count && i < 4; i++)
                strncpy(reg->d_inputs[i], dlist + i * MAX_NAME, MAX_NAME - 1);
            reg->clk_en = 1;
            strcpy(reg->clk_src, "clk");
            while (p && *p && *p != '\n') {
                p = skip_ws_ret(p);
                if (*p == '\0' || *p == '\n') break;
                char opt[MAX_NAME];
                p = parse_word(p, opt, sizeof(opt));
                if (strncmp(opt, "clk_en=", 7) == 0)
                    reg->clk_en = (uint8_t)strtol(opt + 7, NULL, 0);
                else
                    strncpy(reg->clk_src, opt, MAX_NAME - 1);
            }
            continue;
        }

        if (strcmp(keyword, ".CONNECT") == 0) {
            if (ctx->n_connects >= MAX_CONNS) continue;
            char src[MAX_NAME], arrow[MAX_NAME], dst[MAX_NAME];
            p = parse_word(p, src, sizeof(src));
            p = parse_word(p, arrow, sizeof(arrow));
            p = parse_word(p, dst, sizeof(dst));
            strcpy(ctx->connects[ctx->n_connects].src, src);
            strcpy(ctx->connects[ctx->n_connects].dst, dst);
            ctx->n_connects++;
            continue;
        }

        if (strcmp(keyword, ".PIN") == 0) {
            if (ctx->n_pins >= MAX_PINS) continue;
            char name[MAX_NAME], eq[MAX_NAME], sig[MAX_NAME];
            p = parse_word(p, name, sizeof(name));
            p = parse_word(p, eq, sizeof(eq));
            p = parse_word(p, sig, sizeof(sig));
            pin_t *pin = &ctx->pins[ctx->n_pins++];
            strncpy(pin->name, name, MAX_NAME - 1);
            pin->pin_id = ctx->n_pins - 1;
            pin->dir = 1;
            strncpy(pin->signal, sig, MAX_NAME - 1);
            continue;
        }

        if (strcmp(keyword, ".CONFIG") == 0) {
            char cfg_name[MAX_NAME];
            p = parse_word(p, cfg_name, sizeof(cfg_name));
            p = skip_ws_ret(p);
            if (*p == '=') p++;
            p = skip_ws_ret(p);
            ctx->config_value = (int32_t)strtol(p, NULL, 0);
            strncpy(ctx->config_name, cfg_name, MAX_NAME - 1);
            ctx->has_config = 1;
            continue;
        }

        if (strcmp(keyword, ".END") == 0) break;
        while (*p && *p != '\n') p++;
    }
    fclose(f);
    return 0;
}

static void place_luts(ctx_t *ctx) {
    uint8_t lut_count = ctx->n_luts;
    uint8_t clbs_needed = (lut_count + 4 - 1) / 4;
    if (clbs_needed == 0) clbs_needed = 1;
    uint8_t total = ctx->rows * ctx->cols;
    if (clbs_needed > total) {
        ctx->rows = 8;
        ctx->cols = (clbs_needed + 7) / 8;
    }

    uint8_t clb_idx = 0;
    uint8_t lut_in_clb = 0;
    for (int i = 0; i < ctx->n_luts; i++) {
        ctx->luts[i].clb_row = clb_idx / ctx->cols;
        ctx->luts[i].clb_col = clb_idx % ctx->cols;
        ctx->luts[i].lut_idx = lut_in_clb;
        lut_in_clb++;
        if (lut_in_clb >= 4) {
            lut_in_clb = 0;
            clb_idx++;
        }
    }

    uint8_t reg_clb_idx = 0;
    uint8_t reg_in_clb = 0;
    for (int i = 0; i < ctx->n_regs; i++) {
        ctx->regs[i].clb_row = reg_clb_idx / ctx->cols;
        ctx->regs[i].clb_col = reg_clb_idx % ctx->cols;
        ctx->regs[i].reg_idx = reg_in_clb;
        reg_in_clb++;
        if (reg_in_clb >= 4) {
            reg_in_clb = 0;
            reg_clb_idx++;
        }
    }
}

static int emit_bitstream(ctx_t *ctx, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) return -1;

    fprintf(f, "# xo-sfpga bitstream v1 (emitted by pnr)\n");
    fprintf(f, "# source: HDL0\n");
    fprintf(f, ".FABRIC %d %d\n", ctx->rows, ctx->cols);

    if (ctx->has_config) {
        fprintf(f, ".CONFIG %s=%d\n", ctx->config_name, ctx->config_value);
    }

    for (int i = 0; i < ctx->n_luts; i++) {
        const pnr_lut_t *lut = &ctx->luts[i];
        uint8_t out_wire = resolve_signal(ctx, lut->output_name);
        uint8_t n_in = lut->n_inputs;
        if (n_in > 8) n_in = 8;
        fprintf(f, ".LUT %d %d %d %d",
                lut->clb_row, lut->clb_col, lut->lut_idx, n_in);
        for (int j = 0; j < n_in; j++) {
            fprintf(f, " %d", resolve_signal(ctx, lut->inputs[j]));
        }
        fprintf(f, " %d {", out_wire);
        int tt_count = 16;
        for (int j = 0; j < tt_count; j++) {
            fprintf(f, "%d%s", lut->truth_table[j], j < tt_count - 1 ? "," : "");
        }
        fprintf(f, "}\n");
    }

    for (int i = 0; i < ctx->n_regs; i++) {
        const pnr_reg_t *reg = &ctx->regs[i];
        uint8_t out_wire = resolve_signal(ctx, reg->output_name);
        uint8_t clk_src = resolve_signal(ctx, reg->clk_src);
        fprintf(f, ".REG %d %d %d", reg->clb_row, reg->clb_col, reg->reg_idx);
        for (int j = 0; j < 4; j++) {
            if (reg->d_inputs[j][0])
                fprintf(f, " %d", resolve_signal(ctx, reg->d_inputs[j]));
            else
                fprintf(f, " 0");
        }
        fprintf(f, " %d %d %d\n", out_wire, reg->clk_en, clk_src);
    }

    for (int i = 0; i < ctx->n_pins; i++) {
        const pin_t *pin = &ctx->pins[i];
        uint8_t w = resolve_signal(ctx, pin->signal);
        fprintf(f, ".PIN %d %d %d\n", pin->pin_id, pin->dir, w);
    }

    for (int i = 0; i < ctx->n_connects; i++) {
        uint8_t src = resolve_signal(ctx, ctx->connects[i].src);
        uint8_t dst = resolve_signal(ctx, ctx->connects[i].dst);
        fprintf(f, ".CONNECT %d %d\n", src, dst);
    }

    fprintf(f, ".END\n");
    fclose(f);
    return 0;
}

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <input.hdl0> <output.bit>\n", prog);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    ctx_t ctx;
    if (parse_hdl0(&ctx, argv[1]) != 0) {
        fprintf(stderr, "pnr: error: cannot parse %s\n", argv[1]);
        return 1;
    }

    place_luts(&ctx);

    if (emit_bitstream(&ctx, argv[2]) != 0) {
        fprintf(stderr, "pnr: error: cannot write %s\n", argv[2]);
        return 1;
    }

    printf("pnr: compiled %s -> %s\n", argv[1], argv[2]);
    printf("  LUTs: %d, Pins: %d, Connects: %d, Fabric: %dx%d\n",
           ctx.n_luts, ctx.n_pins, ctx.n_connects, ctx.rows, ctx.cols);
    printf("  Symbols resolved: %d\n", ctx.n_symbols);
    return 0;
}
