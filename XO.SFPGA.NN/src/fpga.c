/*
 * fpga.c - XO.SFPGA.NN FPGA Emulator (standalone executable)
 *
 * Discrete 7400-series FPGA emulator using LUT-style logic ICs.
 * Consumes a text bitstream file and simulates the FPGA fabric.
 *
 * Build: gcc -o fpga fpga.c
 * Usage: ./fpga <bitstream.bit> [cycles] [output.trace]
 *        ./fpga --test
 *
 * The bitstream format is text-based (defined in architecture.md):
 *   .FABRIC rows cols
 *   .LUT clb_row clb_col lut_idx n_inputs in0 in1 ... out_wire tt0,tt1,...,tt15
 *   .REG clb_row clb_col reg_idx d0 d1 d2 d3 out_wire clk_en clk_src
 *   .CONNECT src_wire dst_wire
 *   .PIN pin_id dir wire
 *   .CONFIG name value
 *   .END
 */
#include <fpga_hw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define LUT_SIZE_4    FPGA_LUT_SIZE
#define BUS_WIDTH     FPGA_BUS_WIDTH
#define MAX_CLBS      FPGA_MAX_CLBS
#define MAX_LUTS_PER_CLB  FPGA_MAX_LUTS_PER_CLB
#define MAX_REGS_PER_CLB  FPGA_MAX_REGS_PER_CLB
#define MAX_WIRES     FPGA_MAX_WIRES
#define MAX_SWITCHBOXES FPGA_MAX_SWITCHBOXES
#define MAX_PINS      FPGA_MAX_PINS
#define SB_BUS_DIRECT FPGA_SB_BUS_DIRECT

#define OP_LUT3       0
#define OP_LUT4       1
#define OP_REG        2
#define OP_CONNECT    3
#define OP_PIN        4
#define OP_CONFIG     5

#define BITSTREAM_MAGIC FPGA_BITSTREAM_MAGIC
#define BITSTREAM_VERSION FPGA_BITSTREAM_VERSION

typedef fpga_lut_t lut_t;
typedef fpga_reg_t reg_t;
typedef fpga_bus_t bus_t;
typedef fpga_switchbox_conn_t switchbox_conn_t;
typedef fpga_clb_t clb_t;
typedef fpga_switchbox_t switchbox_t;
typedef fpga_bs_entry_t bs_entry_t;
typedef fpga_bitstream_t bitstream_t;
typedef fpga_t fpga_t;

static void skip_ws(char **p) {
    while (isspace((unsigned char)**p)) (*p)++;
}

static int parse_uint(char **p) {
    skip_ws(p);
    char *end;
    int val = (int)strtoul(*p, &end, 0);
    *p = end;
    return val;
}

static void parse_tt16(char *s, char *end, uint8_t tt[16]) {
    int idx = 0;
    char *tok = s;
    while (tok < end && idx < 16) {
        skip_ws(&tok);
        if (*tok == '\0' || tok >= end) break;
        if (*tok == ',') { tok++; continue; }
        if (*tok == '}') break;
        while (tok < end && isspace((unsigned char)*tok)) tok++;
        if (tok >= end) break;
        if (*tok == ',' || *tok == '}') break;
        char *val_end = tok;
        long v = strtol(tok, &val_end, 0);
        if (val_end != tok) {
            tt[idx++] = (uint8_t)(v & 0xFF);
            tok = val_end;
        } else {
            tok++;
        }
    }
    while (idx < 16) tt[idx++] = 0;
}

int fpga_bs_load(bitstream_t *bs, const char *filename) {
    memset(bs, 0, sizeof(bitstream_t));
    bs->magic = BITSTREAM_MAGIC;
    bs->version = BITSTREAM_VERSION;

    FILE *f = fopen(filename, "r");
    if (!f) return -1;

    char line[512];
    while (fgets(line, sizeof(line), f) && bs->num_entries < 256) {
        char *p = line;
        skip_ws(&p);
        if (*p == '#' || *p == '\0') continue;

        bs_entry_t *e = &bs->entries[bs->num_entries];
        memset(e, 0, sizeof(bs_entry_t));

        if (strncmp(p, ".FABRIC", 7) == 0) {
            p += 7;
            bs->rows = (uint8_t)parse_uint(&p);
            bs->cols = (uint8_t)parse_uint(&p);
            continue;
        }
        if (strncmp(p, ".LUT", 4) == 0) {
            p += 4;
            e->op = OP_LUT4;
            e->clb_row = (uint8_t)parse_uint(&p);
            e->clb_col = (uint8_t)parse_uint(&p);
            e->lut_idx = (uint8_t)parse_uint(&p);
            e->n_inputs = (uint8_t)parse_uint(&p);
            for (int i = 0; i < e->n_inputs && i < BUS_WIDTH; i++)
                e->inputs[i] = (uint8_t)parse_uint(&p);
            e->output_wire = (uint8_t)parse_uint(&p);
            skip_ws(&p);
            char *tt_start = strchr(p, '{');
            if (tt_start) {
                char *tt_end = strchr(tt_start, '}');
                if (tt_end) parse_tt16(tt_start + 1, tt_end, e->truth_table);
            }
            bs->num_entries++;
            continue;
        }
        if (strncmp(p, ".REG", 4) == 0) {
            p += 4;
            e->op = OP_REG;
            e->clb_row = (uint8_t)parse_uint(&p);
            e->clb_col = (uint8_t)parse_uint(&p);
            e->reg_idx = (uint8_t)parse_uint(&p);
            e->d[0] = (uint8_t)parse_uint(&p);
            e->d[1] = (uint8_t)parse_uint(&p);
            e->d[2] = (uint8_t)parse_uint(&p);
            e->d[3] = (uint8_t)parse_uint(&p);
            e->output_wire = (uint8_t)parse_uint(&p);
            e->clk_en = (uint8_t)parse_uint(&p);
            e->clk_src = (uint8_t)parse_uint(&p);
            bs->num_entries++;
            continue;
        }
        if (strncmp(p, ".CONNECT", 8) == 0) {
            p += 8;
            e->op = OP_CONNECT;
            e->src_wire = (uint8_t)parse_uint(&p);
            e->dst_wire = (uint8_t)parse_uint(&p);
            bs->num_entries++;
            continue;
        }
        if (strncmp(p, ".PIN", 4) == 0) {
            p += 4;
            e->op = OP_PIN;
            e->pin_id = (uint8_t)parse_uint(&p);
            e->pin_dir = (uint8_t)parse_uint(&p);
            e->src_wire = (uint8_t)parse_uint(&p);
            bs->num_entries++;
            continue;
        }
        if (strncmp(p, ".CONFIG", 7) == 0) {
            p += 7;
            e->op = OP_CONFIG;
            skip_ws(&p);
            char *name_end = p;
            while (*name_end && !isspace((unsigned char)*name_end) && *name_end != '=')
                name_end++;
            size_t nlen = (size_t)(name_end - p);
            if (nlen >= 31) nlen = 30;
            strncpy(e->config_name, p, nlen);
            e->config_name[nlen] = '\0';
            p = name_end;
            if (*p == '=') p++;
            skip_ws(&p);
            e->config_value = (int32_t)parse_uint(&p);
            bs->num_entries++;
            continue;
        }
        if (strncmp(p, ".END", 4) == 0) break;
    }
    fclose(f);
    return (int)bs->num_entries;
}

int fpga_bs_apply(fpga_t *fpga, const bitstream_t *bs) {
    if (!fpga || !bs) return -1;
    if (bs->magic != BITSTREAM_MAGIC) return -1;

    memset(fpga, 0, sizeof(fpga_t));
    fpga->wires[0] = 0;
    fpga->wires[1] = 1;
    fpga->n_rows = bs->rows ? bs->rows : 4;
    fpga->n_cols = bs->cols ? bs->cols : 4;
    fpga->clk_div = 1;

    for (uint16_t i = 0; i < bs->num_entries; i++) {
        const bs_entry_t *e = &bs->entries[i];
        if (e->clb_row * 16 + e->clb_col >= MAX_CLBS) continue;
        clb_t *clb = &fpga->grid[e->clb_row * fpga->n_cols + e->clb_col];

        switch (e->op) {
            case OP_LUT3:
            case OP_LUT4: {
                if (e->lut_idx >= MAX_LUTS_PER_CLB) continue;
                lut_t *lut = &clb->luts[e->lut_idx];
                memset(lut, 0, sizeof(lut_t));
                lut->n_inputs = e->n_inputs;
                if (lut->n_inputs > BUS_WIDTH) lut->n_inputs = BUS_WIDTH;
                for (int j = 0; j < lut->n_inputs; j++)
                    lut->inputs[j] = e->inputs[j];
                memcpy(lut->truth_table, e->truth_table, LUT_SIZE_4);
                lut->output_wire = e->output_wire;
                break;
            }
            case OP_REG: {
                if (e->reg_idx >= MAX_REGS_PER_CLB) continue;
                reg_t *reg = &clb->regs[e->reg_idx];
                memset(reg, 0, sizeof(reg_t));
                for (int j = 0; j < 4; j++)
                    reg->d[j] = e->d[j];
                reg->output_wire = e->output_wire;
                reg->clk_en = e->clk_en;
                break;
            }
            case OP_CONNECT:
                fpga->wires[e->dst_wire] = fpga->wires[e->src_wire];
                break;
            case OP_PIN:
                fpga->io_pins[e->pin_id].values[0] = e->src_wire;
                fpga->pin_dir[e->pin_id] = e->pin_dir;
                break;
            case OP_CONFIG:
                if (strcmp(e->config_name, "clk_div") == 0)
                    fpga->clk_div = (uint8_t)e->config_value;
                break;
        }
    }
    return 0;
}

__attribute__((unused))
static void lut_init(lut_t *lut) {
    memset(lut->truth_table, 0, LUT_SIZE_4);
    lut->n_inputs = 0;
    lut->output_wire = 0;
    lut->output = 0;
}

static uint8_t lut_eval(lut_t *lut, fpga_t *fpga) {
    uint8_t index = 0;
    for (int i = 0; i < lut->n_inputs; i++) {
        index |= (fpga->wires[lut->inputs[i]] << i);
    }
    if (lut->n_inputs <= 3) index &= 0x07;
    else index &= 0x0F;
    lut->output = lut->truth_table[index];
    fpga->wires[lut->output_wire] = lut->output;
    return lut->output;
}

static void reg_cycle(reg_t *reg, fpga_t *fpga) {
    if (reg->clk && reg->clk_en) {
        for (int i = 0; i < 4; i++) {
            reg->q[i] = fpga->wires[reg->d[i]];
        }
    }
    uint8_t val = 0;
    for (int i = 0; i < 4; i++) val |= (reg->q[i] << i);
    fpga->wires[reg->output_wire] = val;
}

static void switchbox_eval(fpga_t *fpga) {
    for (uint8_t s = 0; s < fpga->n_switchboxes; s++) {
        switchbox_t *sb = &fpga->sb[s];
        for (uint8_t c = 0; c < sb->n_conns; c++) {
            switchbox_conn_t *conn = &sb->conns[c];
            if (conn->src_bus == SB_BUS_DIRECT && conn->dst_bus == SB_BUS_DIRECT) {
                fpga->wires[conn->dst_bit] = fpga->wires[conn->src_bit];
            }
        }
    }
}

void fpga_cycle(fpga_t *fpga) {
    fpga->clk_counter++;
    if (fpga->clk_counter >= fpga->clk_div) {
        fpga->clk_counter = 0;
        fpga->clk = !fpga->clk;
    }

    /* Drive input pins: copy external value to wire so LUTs can read it */
    for (uint8_t p = 0; p < MAX_PINS; p++) {
        if (fpga->pin_dir[p] == 0) {
            uint8_t wire = fpga->io_pins[p].values[0];
            fpga->wires[wire] = fpga->io_pins[p].values[1];
        }
    }

    switchbox_eval(fpga);

    for (uint8_t r = 0; r < fpga->n_rows; r++) {
        for (uint8_t c = 0; c < fpga->n_cols; c++) {
            clb_t *clb = &fpga->grid[r * fpga->n_cols + c];
            for (uint8_t l = 0; l < MAX_LUTS_PER_CLB; l++) {
                lut_t *lut = &clb->luts[l];
                if (lut->n_inputs > 0) lut_eval(lut, fpga);
            }
        }
    }

    for (uint8_t r = 0; r < fpga->n_rows; r++) {
        for (uint8_t c = 0; c < fpga->n_cols; c++) {
            clb_t *clb = &fpga->grid[r * fpga->n_cols + c];
            for (uint8_t rg = 0; rg < MAX_REGS_PER_CLB; rg++) {
                reg_t *reg = &clb->regs[rg];
                if (reg->clk_en) {
                    reg->clk = fpga->clk;
                    reg_cycle(reg, fpga);
                }
            }
        }
    }

    /* Read output pins: copy wire value to values[1] for CPU access */
    for (uint8_t p = 0; p < MAX_PINS; p++) {
        if (fpga->pin_dir[p] != 0) {
            uint8_t wire = fpga->io_pins[p].values[0];
            fpga->io_pins[p].values[1] = fpga->wires[wire];
        }
    }
}

void fpga_set_wire(fpga_t *fpga, uint8_t wire, uint8_t value) {
    fpga->wires[wire] = value;
}

uint8_t fpga_get_wire(fpga_t *fpga, uint8_t wire) {
    return fpga->wires[wire];
}

void fpga_set_pin(fpga_t *fpga, uint8_t pin, uint8_t value) {
    if (pin < MAX_PINS && fpga->pin_dir[pin] == 0)
        fpga->io_pins[pin].values[1] = value;
}

uint8_t fpga_get_pin(fpga_t *fpga, uint8_t pin) {
    if (pin < MAX_PINS) return fpga->io_pins[pin].values[1];
    return 0;
}

#ifdef FPGA_STANDALONE

static int tpass = 0, tfail = 0;

static void check_int(int got, int expected, const char *desc) {
    if (got == expected) { tpass++; }
    else { tfail++; printf("FAIL: %s (got %d, expected %d)\n", desc, got, expected); }
}

static void check(int cond, const char *desc) {
    if (cond) tpass++;
    else { tfail++; printf("FAIL: %s\n", desc); }
}

static void test_lut_and(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));
    fpga.n_rows = 4; fpga.n_cols = 4;
    lut_t *lut = &fpga.grid[0].luts[0];
    lut_init(lut);
    lut->n_inputs = 2;
    lut->inputs[0] = 10; lut->inputs[1] = 11;
    lut->output_wire = 20;
    lut->truth_table[0] = 0; lut->truth_table[1] = 0;
    lut->truth_table[2] = 0; lut->truth_table[3] = 1;

    fpga_set_wire(&fpga, 10, 0); fpga_set_wire(&fpga, 11, 0);
    check_int(lut_eval(lut, &fpga), 0, "AND(0,0)");
    fpga_set_wire(&fpga, 10, 1); fpga_set_wire(&fpga, 11, 1);
    check_int(lut_eval(lut, &fpga), 1, "AND(1,1)");
    check_int(fpga_get_wire(&fpga, 20), 1, "AND output wire");
}

static void test_lut_mux4(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));
    lut_t *lut = &fpga.grid[0].luts[0];
    lut_init(lut);
    lut->n_inputs = 3;
    lut->inputs[0] = 10; lut->inputs[1] = 11; lut->inputs[2] = 12;
    lut->output_wire = 20;
    for (int i = 0; i < 8; i++) lut->truth_table[i] = 0;
    lut->truth_table[1] = 1; lut->truth_table[3] = 1;
    lut->truth_table[6] = 1; lut->truth_table[7] = 1;

    fpga_set_wire(&fpga, 10, 1); fpga_set_wire(&fpga, 11, 0); fpga_set_wire(&fpga, 12, 0);
    check_int(lut_eval(lut, &fpga), 1, "MUX sel=0 d0=1 -> 1");
    fpga_set_wire(&fpga, 10, 0); fpga_set_wire(&fpga, 11, 1); fpga_set_wire(&fpga, 12, 1);
    check_int(lut_eval(lut, &fpga), 1, "MUX sel=1 (sel=3) d1=1 -> 1");
}

static void test_counter_pipeline(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));
    fpga.n_rows = 4; fpga.n_cols = 4;
    fpga.clk_div = 1;

    lut_t *lut = &fpga.grid[0].luts[0];
    lut_init(lut);
    lut->n_inputs = 2;
    lut->inputs[0] = 10;
    lut->inputs[1] = 30;
    lut->output_wire = 20;
    lut->truth_table[0] = 0; lut->truth_table[1] = 1;
    lut->truth_table[2] = 1; lut->truth_table[3] = 0;

    reg_t *reg = &fpga.grid[0].regs[0];
    memset(reg, 0, sizeof(reg_t));
    reg->d[0] = 20;
    reg->output_wire = 30;
    reg->clk_en = 1;

    fpga_set_wire(&fpga, 10, 1);
    uint8_t results[8];
    for (int cycle = 0; cycle < 8; cycle++) {
        fpga_cycle(&fpga);
        results[cycle] = fpga_get_wire(&fpga, 30);
    }
    int toggled = 0;
    for (int i = 1; i < 8; i++)
        if (results[i] != results[i-1]) toggled++;
    check(toggled >= 2, "T-flip-flop counter toggles");
}

static void test_bitstream_io(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));

    bitstream_t bs;
    memset(&bs, 0, sizeof(bs));
    bs.magic = BITSTREAM_MAGIC;
    bs.version = BITSTREAM_VERSION;
    bs.rows = 4; bs.cols = 4;
    bs.num_entries = 2;

    bs_entry_t *e = &bs.entries[0];
    e->op = OP_LUT4;
    e->clb_row = 0; e->clb_col = 0; e->lut_idx = 0;
    e->n_inputs = 2;
    e->inputs[0] = 10; e->inputs[1] = 11;
    e->output_wire = 20;
    e->truth_table[0] = 0; e->truth_table[1] = 0;
    e->truth_table[2] = 0; e->truth_table[3] = 1;

    e = &bs.entries[1];
    e->op = OP_PIN;
    e->pin_id = 0; e->pin_dir = 1; e->src_wire = 20;

    check_int(fpga_bs_apply(&fpga, &bs), 0, "bitstream_apply");

    fpga_set_wire(&fpga, 10, 1); fpga_set_wire(&fpga, 11, 1);
    fpga_cycle(&fpga);
    check_int(fpga_get_pin(&fpga, 0), 1, "Bitstream: AND(1,1) -> pin 0");
}

static void test_74hc595(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));
    lut_t *lut = &fpga.grid[0].luts[0];
    lut_init(lut);
    lut->n_inputs = 2;
    lut->inputs[0] = 30; lut->inputs[1] = 31;
    lut->output_wire = 32;
    lut->truth_table[0] = 0; lut->truth_table[1] = 1;
    lut->truth_table[2] = 0; lut->truth_table[3] = 1;

    fpga_set_wire(&fpga, 30, 1); fpga_set_wire(&fpga, 31, 1);
    check_int(lut_eval(lut, &fpga), 1, "74HC595: data=1,clk=1 -> Q=1");
}

static void test_74hc251(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));
    fpga.n_rows = 4; fpga.n_cols = 4;
    lut_t *lut = &fpga.grid[0].luts[0];
    lut_init(lut);
    lut->n_inputs = 3;
    lut->inputs[0] = 10; lut->inputs[1] = 11; lut->inputs[2] = 12;
    lut->output_wire = 20;
    for (int i = 0; i < 8; i++) lut->truth_table[i] = (i == 5) ? 1 : 0;

    fpga_set_wire(&fpga, 10, 1); fpga_set_wire(&fpga, 11, 0);
    fpga_set_wire(&fpga, 12, 1);
    fpga_cycle(&fpga);
    check_int(fpga_get_wire(&fpga, 20), 1, "74HC251: sel=5 -> D5=1");
}

static void test_74hc173(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));
    fpga.clk_div = 1;
    fpga.n_rows = 4; fpga.n_cols = 4;

    reg_t *reg = &fpga.grid[0].regs[0];
    memset(reg, 0, sizeof(reg_t));
    reg->d[0] = 10;
    reg->output_wire = 30;
    reg->clk_en = 1;

    fpga_set_wire(&fpga, 10, 1);
    fpga_cycle(&fpga);
    fpga_cycle(&fpga);
    check_int((fpga_get_wire(&fpga, 30) & 1), 1, "74HC173: captures on rising edge");
}

static void test_switchbox(void) {
    fpga_t fpga;
    memset(&fpga, 0, sizeof(fpga_t));
    fpga.n_rows = 1; fpga.n_cols = 1;
    fpga.n_switchboxes = 1;
    fpga.sb[0].n_conns = 0;
    switchbox_conn_t *conn = &fpga.sb[0].conns[fpga.sb[0].n_conns++];
    conn->src_bus = SB_BUS_DIRECT; conn->src_bit = 10;
    conn->dst_bus = SB_BUS_DIRECT; conn->dst_bit = 20;

    fpga_set_wire(&fpga, 10, 1);
    for (int c = 0; c < fpga.sb[0].n_conns; c++) {
        conn = &fpga.sb[0].conns[c];
        uint8_t src_val = fpga.wires[conn->src_bit];
        fpga.wires[conn->dst_bit] = src_val;
    }
    check_int(fpga_get_wire(&fpga, 20), 1, "Switchbox routes wire 10->20");
}

static void test_bitstream_save_load(void) {
    bitstream_t bs;
    memset(&bs, 0, sizeof(bs));
    bs.magic = BITSTREAM_MAGIC;
    bs.version = BITSTREAM_VERSION;
    bs.rows = 4; bs.cols = 4;
    bs.num_entries = 1;

    bs_entry_t *e = &bs.entries[0];
    e->op = OP_LUT4;
    e->clb_row = 0; e->clb_col = 0; e->lut_idx = 0;
    e->n_inputs = 2;
    e->inputs[0] = 10; e->inputs[1] = 11;
    e->output_wire = 20;
    e->truth_table[0] = 0; e->truth_table[1] = 0;
    e->truth_table[2] = 0; e->truth_table[3] = 1;

    FILE *f = fopen("/tmp/test_roundtrip.bit", "w");
    if (!f) { check(0, "save file open"); return; }
    fprintf(f, ".FABRIC 4 4\n");
    fprintf(f, ".LUT 0 0 0 2 10 11 20 {0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0}\n");
    fprintf(f, ".END\n");
    fclose(f);

    bitstream_t bs2;
    int rc = fpga_bs_load(&bs2, "/tmp/test_roundtrip.bit");
    check_int(rc, 1, "bitstream_load returns 1");
    check_int(bs2.entries[0].op, OP_LUT4, "Loaded LUT4");
    check_int(bs2.entries[0].n_inputs, 2, "Loaded n_inputs=2");
    check_int(bs2.entries[0].inputs[0], 10, "Loaded input[0]=10");
    check_int(bs2.entries[0].truth_table[3], 1, "Loaded tt[3]=1");
    check_int(bs2.entries[0].output_wire, 20, "Loaded output_wire=20");
}

static void run_tests(void) {
    test_lut_and();
    test_lut_mux4();
    test_counter_pipeline();
    test_bitstream_io();
    test_74hc595();
    test_74hc251();
    test_74hc173();
    test_switchbox();
    test_bitstream_save_load();

    printf("=== FPGA Test Results: %d passed, %d failed ===\n", tpass, tfail);
}

int main(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--test") == 0) {
        run_tests();
        return tfail > 0 ? 1 : 0;
    }

    const char *bitfile = argv[1];
    int cycles = 1;
    const char *outfile = NULL;
    const char *init_str = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--init") == 0 && i + 1 < argc) {
            init_str = argv[++i];
        } else if (strcmp(argv[i], "--out") == 0 && i + 1 < argc) {
            outfile = argv[++i];
        } else if (outfile == NULL && argv[i][0] != '-') {
            cycles = atoi(argv[i]);
        }
    }

    bitstream_t bs;
    if (fpga_bs_load(&bs, bitfile) < 0) {
        fprintf(stderr, "Error: cannot load bitstream %s\n", bitfile);
        return 1;
    }

    fpga_t fpga;
    if (fpga_bs_apply(&fpga, &bs) != 0) {
        fprintf(stderr, "Error: cannot apply bitstream\n");
        return 1;
    }

    if (init_str) {
        char buf[512];
        strncpy(buf, init_str, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, ",");
        while (tok) {
            char *eq = strchr(tok, '=');
            if (eq) {
                *eq = '\0';
                int wire = atoi(tok);
                int val = atoi(eq + 1);
                if (wire >= 0 && wire < MAX_WIRES)
                    fpga.wires[wire] = (uint8_t)val;
            }
            tok = strtok(NULL, ",");
        }
    }

    FILE *out = outfile ? fopen(outfile, "w") : stdout;
    if (!out) out = stdout;

    fprintf(out, "# xo-sfpga simulation trace\n");
    fprintf(out, "# bitstream=%s fabric=%dx%d cycles=%d\n",
            bitfile, bs.rows, bs.cols, cycles);

    fprintf(out, "cycle clk ");
    int n_pins = 0;
    for (int i = 0; i < MAX_PINS; i++) {
        if (fpga.io_pins[i].values[0] != 0 || i == 0) {
            fprintf(out, "p%d ", i);
            n_pins = i + 1;
        }
    }
    fprintf(out, "\n");

    for (int cycle = 0; cycle < cycles; cycle++) {
        fpga_cycle(&fpga);
        fprintf(out, "%5d %3d ", cycle, fpga.clk);
        for (int i = 0; i < n_pins; i++) {
            uint8_t w = fpga.io_pins[i].values[0];
            fprintf(out, "%4d ", fpga.wires[w]);
        }
        fprintf(out, "\n");
    }

    if (out != stdout) fclose(out);
    return 0;
}

#endif /* FPGA_STANDALONE */
