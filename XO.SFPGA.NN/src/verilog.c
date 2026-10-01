/*
 * verilog.c - XO.SFPGA.NN Verilog-to-HDL0 Compiler (standalone executable)
 *
 * Compiles a simple Verilog subset to HDL0 text format for the SFPGA toolchain.
 *
 * Build: gcc -o vlog verilog.c
 * Usage: ./vlog <input.v> <output.hdl0>
 *
 * Supported Verilog subset:
 *   - module, endmodule
 *   - input, output, wire, reg declarations
 *   - assign with expressions: &, |, ^, ~, &&, ||
 *   - always @(posedge clk) begin ... end (register inference)
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define VLOG_MAX_TOKS 1024
#define VLOG_MAX_NAME 128
#define VLOG_MAX_SIGNALS 64
#define VLOG_MAX_NODES 128

typedef enum {
    T_EOF, T_IDENT, T_NUM, T_STR,
    T_LBRACE, T_RBRACE, T_LPAREN, T_RPAREN,
    T_LBRACKET, T_RBRACKET, T_SEMI, T_COMMA,
    T_EQ, T_EQEQ, T_NE, T_AND, T_OR, T_XOR, T_NOT,
    T_LE, T_GE,
    T_SHL, T_SHR,
    T_PLUS, T_MINUS, T_STAR, T_CPL,
    KW_MODULE, KW_ENDMODULE, KW_INPUT, KW_OUTPUT,
    KW_WIRE, KW_REG, KW_ASSIGN, KW_ALWAYS,
    KW_BEGIN, KW_END, KW_IF, KW_ELSE,
    KW_POSEDGE, KW_NEGEDGE
} tok_t;

typedef struct {
    tok_t type;
    char text[VLOG_MAX_NAME];
    uint64_t val;
} token_t;

typedef struct {
    char name[VLOG_MAX_NAME];
    uint8_t wire_id;
    uint8_t is_reg;
    uint8_t is_input;
    uint8_t is_output;
} sig_t;

typedef enum { OP_CONST, OP_VAR, OP_AND, OP_OR, OP_XOR, OP_NOT } node_op_t;

typedef struct {
    node_op_t op;
    uint8_t a, b;
    uint8_t out_wire;
    uint8_t value;
    uint8_t is_const;
} node_t;

typedef struct {
    node_t nodes[VLOG_MAX_NODES];
    uint8_t n_nodes;
    uint8_t next_wire;
} node_graph_t;

typedef struct {
    token_t *tokens;
    int count;
    int pos;
} parser_t;

static char *skip_ws(char *p) {
    while (p && *p && isspace((unsigned char)*p)) p++;
    return p;
}

static tok_t tokenize_one(char **pp, token_t *tok) {
    char *p = skip_ws(*pp);
    *pp = p;
    if (!p || !*p) { tok->type = T_EOF; tok->text[0] = '\0'; return T_EOF; }
    char c = *p;

    if (strncmp(p, "//", 2) == 0) { p += 2; while (*p && *p != '\n') p++; *pp = p; return tokenize_one(pp, tok); }
    if (strncmp(p, "/*", 2) == 0) {
        p += 2; while (*p && !(p[0]=='*'&&p[1]=='/')) p++; if (*p) p += 2; *pp = p; return tokenize_one(pp, tok);
    }
    if (c=='{'){(*pp)++;tok->type=T_LBRACE;strcpy(tok->text,"{");return T_LBRACE;}
    if (c=='}'){(*pp)++;tok->type=T_RBRACE;strcpy(tok->text,"}");return T_RBRACE;}
    if (c=='('){(*pp)++;tok->type=T_LPAREN;strcpy(tok->text,"(");return T_LPAREN;}
    if (c==')'){(*pp)++;tok->type=T_RPAREN;strcpy(tok->text,")");return T_RPAREN;}
    if (c==';'){(*pp)++;tok->type=T_SEMI;strcpy(tok->text,";");return T_SEMI;}
    if (c==','){(*pp)++;tok->type=T_COMMA;strcpy(tok->text,",");return T_COMMA;}
    if (p[0]=='<'&&p[1]=='='){*pp+=2;tok->type=T_LE;strcpy(tok->text,"<=");return T_LE;}
    if (p[0]=='='&&p[1]=='='){*pp+=2;tok->type=T_EQEQ;strcpy(tok->text,"==");return T_EQEQ;}
    if (p[0]=='!'&&p[1]=='='){*pp+=2;tok->type=T_NE;strcpy(tok->text,"!=");return T_NE;}
    if (p[0]=='<'&&p[1]=='<'){*pp+=2;tok->type=T_SHL;strcpy(tok->text,"<<");return T_SHL;}
    if (p[0]=='>'&&p[1]=='>'){*pp+=2;tok->type=T_SHR;strcpy(tok->text,">>");return T_SHR;}
    if (p[0]=='='&&p[1]!='='){(*pp)++;tok->type=T_EQ;tok->text[0]='=';tok->text[1]='\0';return T_EQ;}
    if (c=='&'){(*pp)++;tok->type=T_AND;tok->text[0]='&';tok->text[1]='\0';return T_AND;}
    if (c=='|'){(*pp)++;tok->type=T_OR;tok->text[0]='|';tok->text[1]='\0';return T_OR;}
    if (c=='^'){(*pp)++;tok->type=T_XOR;tok->text[0]='^';tok->text[1]='\0';return T_XOR;}
    if (c=='~'){(*pp)++;tok->type=T_CPL;tok->text[0]='~';tok->text[1]='\0';return T_CPL;}
    if (c=='+'){(*pp)++;tok->type=T_PLUS;tok->text[0]='+';tok->text[1]='\0';return T_PLUS;}
    if (c=='-'){(*pp)++;tok->type=T_MINUS;tok->text[0]='-';tok->text[1]='\0';return T_MINUS;}
    if (c=='*'){(*pp)++;tok->type=T_STAR;tok->text[0]='*';tok->text[1]='\0';return T_STAR;}

    if (isdigit((unsigned char)c)) {
        char *end; tok->val = strtoull(p, &end, 0);
        size_t len = (size_t)(end - p); if (len >= VLOG_MAX_NAME) len = VLOG_MAX_NAME-1;
        memcpy(tok->text, p, len); tok->text[len]='\0'; *pp = end; tok->type = T_NUM; return T_NUM;
    }
    if (isalpha((unsigned char)c) || c=='_') {
        char *start = p;
        while (*p && (isalnum((unsigned char)*p) || *p=='_')) p++;
        size_t len = (size_t)(p - start);
        if (len >= VLOG_MAX_NAME) len = VLOG_MAX_NAME-1;
        memcpy(tok->text, start, len); tok->text[len]='\0'; *pp = p; tok->val = 0;
        if (strcmp(tok->text,"module")==0) return (tok->type=KW_MODULE,KW_MODULE);
        if (strcmp(tok->text,"endmodule")==0) return (tok->type=KW_ENDMODULE,KW_ENDMODULE);
        if (strcmp(tok->text,"input")==0) return (tok->type=KW_INPUT,KW_INPUT);
        if (strcmp(tok->text,"output")==0) return (tok->type=KW_OUTPUT,KW_OUTPUT);
        if (strcmp(tok->text,"wire")==0) return (tok->type=KW_WIRE,KW_WIRE);
        if (strcmp(tok->text,"reg")==0) return (tok->type=KW_REG,KW_REG);
        if (strcmp(tok->text,"assign")==0) return (tok->type=KW_ASSIGN,KW_ASSIGN);
        if (strcmp(tok->text,"always")==0) return (tok->type=KW_ALWAYS,KW_ALWAYS);
        if (strcmp(tok->text,"begin")==0) return (tok->type=KW_BEGIN,KW_BEGIN);
        if (strcmp(tok->text,"end")==0) return (tok->type=KW_END,KW_END);
        if (strcmp(tok->text,"if")==0) return (tok->type=KW_IF,KW_IF);
        if (strcmp(tok->text,"else")==0) return (tok->type=KW_ELSE,KW_ELSE);
        if (strcmp(tok->text,"posedge")==0) return (tok->type=KW_POSEDGE,KW_POSEDGE);
        if (strcmp(tok->text,"negedge")==0) return (tok->type=KW_NEGEDGE,KW_NEGEDGE);
        return (tok->type=T_IDENT,T_IDENT);
    }
        (*pp)++; tok->type = T_EOF; tok->text[0] = c; tok->text[1] = '\0'; return T_EOF;
}

static int tokenize(const char *src, token_t *tokens, int max) {
    int count = 0; char *p = (char *)src;
    while (count < max) {
        token_t tok; tok_t t = tokenize_one(&p, &tok);
        tokens[count++] = tok;
        if (t == T_EOF) break;
    }
    return count;
}

static sig_t *find_sig(sig_t *sigs, int n, const char *name) {
    for (int i = 0; i < n; i++) if (strcmp(sigs[i].name, name) == 0) return &sigs[i];
    return NULL;
}

static sig_t *add_sig(sig_t *sigs, int *n, const char *name, uint8_t is_reg) {
    if (*n >= VLOG_MAX_SIGNALS) return NULL;
    sig_t *s = &sigs[*n]; memset(s, 0, sizeof(sig_t));
    strncpy(s->name, name, VLOG_MAX_NAME - 1); s->is_reg = is_reg; s->wire_id = 0;
    (*n)++; return s;
}

static uint8_t add_const_node(node_graph_t *g, uint8_t val) {
    for (int i = 0; i < g->n_nodes; i++)
        if (g->nodes[i].op == OP_CONST && g->nodes[i].value == val)
            return g->nodes[i].out_wire;
    if (g->n_nodes >= VLOG_MAX_NODES) return 0;
    node_t *n = &g->nodes[g->n_nodes++];
    n->op = OP_CONST; n->out_wire = g->next_wire++; n->value = val; n->is_const = 1;
    return n->out_wire;
}

static uint8_t add_op_node(node_graph_t *g, node_op_t op, uint8_t a, uint8_t b) {
    for (int i = 0; i < g->n_nodes; i++)
        if (g->nodes[i].op == op && g->nodes[i].a == a && g->nodes[i].b == b)
            return g->nodes[i].out_wire;
    if (g->n_nodes >= VLOG_MAX_NODES) return 0;
    node_t *n = &g->nodes[g->n_nodes++];
    n->op = op; n->a = a; n->b = b; n->out_wire = g->next_wire++;
    return n->out_wire;
}

static token_t *peek(parser_t *p) { return p->pos >= p->count ? &p->tokens[p->count] : &p->tokens[p->pos]; }
static token_t *adv(parser_t *p) { token_t *t = peek(p); if (p->pos < p->count) p->pos++; return t; }
static int expect_tok(parser_t *p, tok_t type) { token_t *t = peek(p); if (t->type != type) return 0; adv(p); return 1; }

typedef struct {
    node_graph_t *g;
    sig_t *sigs;
    int n_sigs;
    uint8_t *next_wire;
} eval_ctx_t;

static uint8_t eval_term(parser_t *p, eval_ctx_t *st);

static uint8_t eval_or(parser_t *p, eval_ctx_t *st);

static uint8_t eval_term(parser_t *p, eval_ctx_t *st) {
    token_t *t = peek(p);
    if (t->type == T_NUM) { adv(p); return add_const_node(st->g, (uint8_t)t->val); }
    if (t->type == T_IDENT) {
        adv(p);
        sig_t *sig = find_sig(st->sigs, st->n_sigs, t->text);
        if (sig) return sig->wire_id;
        if (st->n_sigs < VLOG_MAX_SIGNALS) {
            sig_t *s = add_sig(st->sigs, &st->n_sigs, t->text, 0);
            if (s) { s->wire_id = (*st->next_wire)++; return s->wire_id; }
        }
        return 0;
    }
    if (t->type == T_LPAREN) {
        adv(p); uint8_t v = eval_or(p, st); expect_tok(p, T_RPAREN); return v;
    }
    if (t->type == T_CPL || t->type == T_NOT) {
        adv(p); uint8_t operand = eval_term(p, st); return add_op_node(st->g, OP_NOT, operand, 0);
    }
    adv(p); return 0;
}

static uint8_t eval_xor(parser_t *p, eval_ctx_t *st) {
    uint8_t left = eval_term(p, st); token_t *t;
    while ((t = peek(p))->type == T_XOR) { adv(p); uint8_t r = eval_term(p, st); left = add_op_node(st->g, OP_XOR, left, r); }
    return left;
}

static uint8_t eval_and(parser_t *p, eval_ctx_t *st) {
    uint8_t left = eval_xor(p, st); token_t *t;
    while ((t = peek(p))->type == T_AND) { adv(p); uint8_t r = eval_xor(p, st); left = add_op_node(st->g, OP_AND, left, r); }
    return left;
}

static uint8_t eval_or(parser_t *p, eval_ctx_t *st) {
    uint8_t left = eval_and(p, st); token_t *t;
    while ((t = peek(p))->type == T_OR) { adv(p); uint8_t r = eval_and(p, st); left = add_op_node(st->g, OP_OR, left, r); }
    return left;
}

static void compute_tt(const node_t *node, uint8_t n_inputs, uint8_t tt_inputs[4], uint8_t truth[16]) {
    if (node->is_const) {
        int count = 1 << n_inputs;
        for (int i = 0; i < count; i++) truth[i] = node->value;
        return;
    }
    uint8_t a = node->a, b = node->b;
    int count = 1 << n_inputs;
    uint8_t a_idx = 255, b_idx = 255;
    if (a < 2) a_idx = 254;
    if (b < 2) b_idx = 254;
    for (int i = 0; i < n_inputs && i < 4; i++) {
        if (tt_inputs[i] == a) a_idx = (uint8_t)i;
        if (tt_inputs[i] == b) b_idx = (uint8_t)i;
    }
    for (int i = 0; i < count; i++) {
        uint8_t av = (a < 2) ? (a == 1) : (a_idx < n_inputs) ? ((i >> a_idx) & 1) : 0;
        uint8_t bv = (b < 2) ? (b == 1) : (b_idx < n_inputs) ? ((i >> b_idx) & 1) : 0;
        uint8_t result;
        switch (node->op) {
            case OP_AND: result = av & bv; break;
            case OP_OR:  result = av | bv; break;
            case OP_XOR: result = av ^ bv; break;
            case OP_NOT: result = !av; break;
            default: result = 0; break;
        }
        truth[i] = result;
    }
}

int verilog_to_hdl0(const char *vlog, char *out, size_t out_sz) {
    token_t tokens[VLOG_MAX_TOKS];
    int ntok = tokenize(vlog, tokens, VLOG_MAX_TOKS);
    parser_t parser = { tokens, ntok, 0 };
    sig_t sigs[VLOG_MAX_SIGNALS]; int n_sigs = 0;
    node_graph_t graph = {0}; graph.next_wire = 8;

    token_t *t = peek(&parser);
    if (t->type == KW_MODULE) {
        adv(&parser);
        t = peek(&parser);
        if (t->type == T_IDENT) adv(&parser);
        while (peek(&parser)->type != KW_ENDMODULE && peek(&parser)->type != T_EOF) {
            t = peek(&parser);
            if (t->type == KW_INPUT) {
                adv(&parser);
                while (peek(&parser)->type != T_SEMI && peek(&parser)->type != T_EOF) {
                    t = peek(&parser); if (t->type == T_IDENT) { sig_t *s = add_sig(sigs, &n_sigs, t->text, 0); if (s) { s->is_input = 1; s->wire_id = graph.next_wire++; } } adv(&parser);
                }
                if (peek(&parser)->type == T_SEMI) adv(&parser);
            } else if (t->type == KW_OUTPUT) {
                adv(&parser);
                while (peek(&parser)->type != T_SEMI && peek(&parser)->type != T_EOF) {
                    t = peek(&parser); if (t->type == T_IDENT) { sig_t *s = add_sig(sigs, &n_sigs, t->text, 0); if (s) { s->is_output = 1; s->wire_id = graph.next_wire++; } } adv(&parser);
                }
                if (peek(&parser)->type == T_SEMI) adv(&parser);
            } else if (t->type == KW_WIRE || t->type == KW_REG) {
                int is_reg = (t->type == KW_REG); adv(&parser);
                while (peek(&parser)->type != T_SEMI && peek(&parser)->type != T_EOF) {
                    t = peek(&parser); if (t->type == T_IDENT) { sig_t *s = add_sig(sigs, &n_sigs, t->text, is_reg); if (s) s->wire_id = graph.next_wire++; } adv(&parser);
                }
                if (peek(&parser)->type == T_SEMI) adv(&parser);
            } else if (t->type == KW_ASSIGN) {
                adv(&parser);
                token_t *lhs = peek(&parser);
                if (lhs->type == T_IDENT) {
                    adv(&parser); expect_tok(&parser, T_EQ);
                        eval_ctx_t st = { &graph, sigs, n_sigs, &graph.next_wire };
                    uint8_t result = eval_or(&parser, &st);
                    sig_t *sig = find_sig(sigs, n_sigs, lhs->text);
                    if (!sig) sig = add_sig(sigs, &n_sigs, lhs->text, 0);
                    if (sig) sig->wire_id = result;
                }
                while (peek(&parser)->type != T_SEMI && peek(&parser)->type != T_EOF) adv(&parser);
                if (peek(&parser)->type == T_SEMI) adv(&parser);
            } else if (t->type == KW_ALWAYS) {
                adv(&parser); expect_tok(&parser, T_LPAREN);
                if (peek(&parser)->type == KW_POSEDGE) adv(&parser);
                expect_tok(&parser, T_IDENT); expect_tok(&parser, T_RPAREN);
                if (peek(&parser)->type == KW_BEGIN) adv(&parser);
                while (peek(&parser)->type != KW_END && peek(&parser)->type != T_EOF) {
                    t = peek(&parser);
                    if (t->type == T_IDENT && peek(&parser)[1].type == T_EQ) {
                        adv(&parser); expect_tok(&parser, T_EQ);
                    eval_ctx_t st = { &graph, sigs, n_sigs, &graph.next_wire };
                        uint8_t result = eval_or(&parser, &st);
                        sig_t *sig = find_sig(sigs, n_sigs, t->text);
                        if (!sig) sig = add_sig(sigs, &n_sigs, t->text, 0);
                        if (sig) sig->wire_id = result;
                    }
                    adv(&parser);
                }
                if (peek(&parser)->type == KW_END) adv(&parser);
            } else {
                adv(&parser);
            }
        }
        if (peek(&parser)->type == KW_ENDMODULE) adv(&parser);
    }

    int pos = 0;
    int n = snprintf(out + pos, out_sz - pos, "# Auto-generated from Verilog by vlog\n");
    pos += n; if (pos >= (int)out_sz) return -1;

    for (int i = 0; i < n_sigs; i++) {
        if (sigs[i].is_input) {
            n = snprintf(out + pos, out_sz - pos, ".PIN %s -> wire_%d\n", sigs[i].name, sigs[i].wire_id);
            pos += n; if (pos >= (int)out_sz) return -1;
        }
    }

    for (int i = 0; i < graph.n_nodes; i++) {
        const node_t *node = &graph.nodes[i];
        if (node->is_const) continue;
        char inputs_str[128] = ""; int offset = 0;
        uint8_t n_inputs = 0; uint8_t tt_inputs[4] = {0};
        switch (node->op) {
            case OP_AND: case OP_OR: case OP_XOR:
                n_inputs = 2; tt_inputs[0] = node->a; tt_inputs[1] = node->b;
                offset += snprintf(inputs_str + offset, sizeof(inputs_str) - offset, "wire_%d, wire_%d", node->a, node->b);
                break;
            case OP_NOT:
                n_inputs = 1; tt_inputs[0] = node->a;
                offset += snprintf(inputs_str + offset, sizeof(inputs_str) - offset, "wire_%d", node->a);
                break;
            default: continue;
        }
        uint8_t tt[16]; memset(tt, 0, sizeof(tt));
        compute_tt(node, n_inputs, tt_inputs, tt);
        n = snprintf(out + pos, out_sz - pos, ".LUT lut_%d [%s] = {", i, inputs_str);
        pos += n; if (pos >= (int)out_sz) return -1;
        int tt_count = (n_inputs <= 3) ? (1 << n_inputs) : 16;
        for (int j = 0; j < tt_count; j++) {
            n = snprintf(out + pos, out_sz - pos, "%d%s", tt[j], j < tt_count - 1 ? ", " : "");
            pos += n; if (pos >= (int)out_sz) return -1;
        }
        n = snprintf(out + pos, out_sz - pos, "}\n");
        pos += n; if (pos >= (int)out_sz) return -1;
    }

    for (int i = 0; i < n_sigs; i++) {
        if (sigs[i].is_output) {
            n = snprintf(out + pos, out_sz - pos, ".PIN %s = wire_%d\n", sigs[i].name, sigs[i].wire_id);
            pos += n; if (pos >= (int)out_sz) return -1;
        }
    }
    n = snprintf(out + pos, out_sz - pos, ".END\n");
    pos += n;
    return pos;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <input.v> <output.hdl0>\n", argv[0]);
        return 1;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { fprintf(stderr, "vlog: error: cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    if (size < 0 || size > 200000) { fprintf(stderr, "vlog: error: file too large\n"); fclose(f); return 1; }
    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return 1; }
    size_t rd = fread(buf, 1, (size_t)size, f); buf[rd] = '\0'; fclose(f);

    char out[16384];
    int rc = verilog_to_hdl0(buf, out, sizeof(out));
    free(buf);
    if (rc < 0) { fprintf(stderr, "vlog: error: conversion failed\n"); return 1; }

    FILE *of = fopen(argv[2], "w");
    if (!of) { fprintf(stderr, "vlog: error: cannot write %s\n", argv[2]); return 1; }
    fwrite(out, 1, (size_t)rc, of); fclose(of);

    printf("vlog: compiled %s -> %s (%d bytes HDL0)\n", argv[1], argv[2], rc);
    return 0;
}
