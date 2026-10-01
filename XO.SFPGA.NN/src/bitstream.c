#include "bitstream.h"
#include "fpga.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int bitstream_load(bitstream_t *bs, const char *filename) {
    memset(bs, 0, sizeof(bitstream_t));
    bs->magic = BITSTREAM_MAGIC;
    bs->version = BITSTREAM_VERSION;

    FILE *f = fopen(filename, "r");
    if (!f) {
        return -1;
    }

    bs->num_entries = 0;
    char line[512];
    while (fgets(line, sizeof(line), f) && bs->num_entries < 256) {
        bitstream_entry_t *e = &bs->entries[bs->num_entries];
        memset(e, 0, sizeof(bitstream_entry_t));

        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        if (strncmp(p, ".LUT", 4) == 0) {
            e->op = OP_LUT3;
            sscanf(line, ".LUT %d", &e->lut_id);
            bs->num_entries++;
        } else if (strncmp(p, ".REG", 4) == 0) {
            e->op = OP_REG;
            sscanf(line, ".REG %d", &e->reg_id);
            bs->num_entries++;
        } else if (strncmp(p, ".CONNECT", 8) == 0) {
            e->op = OP_CONNECT;
            bs->num_entries++;
        } else if (strncmp(p, ".PIN", 4) == 0) {
            e->op = OP_PIN;
            bs->num_entries++;
        }
    }
    fclose(f);
    return bs->num_entries;
}

int bitstream_apply(fpga_t *fpga, const bitstream_t *bs) {
    fpga->n_rows = bs->rows;
    fpga->n_cols = bs->cols;

    for (uint16_t i = 0; i < bs->num_entries; i++) {
        const bitstream_entry_t *e = &bs->entries[i];
        switch (e->op) {
            case OP_LUT3:
            case OP_LUT4:
                break;
            case OP_REG:
                break;
            case OP_CONNECT:
                break;
            case OP_PIN:
                break;
        }
    }
    return 0;
}
