#include <stdint.h>
#include <string.h>

#ifndef XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_VALUE_CONTAINER
#endif
#include "xrt.h"
#include "gettext_plural.h"

typedef struct {
    const char *text;
    unsigned depth;
    uint64_t count;
    int valid;
} XgtExpr;

static void xgt_space(XgtExpr *expr) {
    while (*expr->text == ' ' || *expr->text == '\t') expr->text++;
}

static uint64_t xgt_expression(XgtExpr *expr, int minimum, int active);

static uint64_t xgt_atom(XgtExpr *expr, int active) {
    uint64_t value = 0;
    char op;
    xgt_space(expr);
    op = *expr->text;
    if (op == '!' || op == '-' || op == '+') {
        if (++expr->depth > 48) { expr->valid = 0; --expr->depth; return 0; }
        expr->text++;
        value = xgt_atom(expr, active);
        --expr->depth;
        return !active ? 0 : op == '!' ? !value : op == '-' ? 0 - value : value;
    }
    if (op == '(') {
        expr->text++;
        value = xgt_expression(expr, 0, active);
        xgt_space(expr);
        if (*expr->text != ')') expr->valid = 0;
        else expr->text++;
        return value;
    }
    if (op == 'n' && (expr->text[1] < 'a' || expr->text[1] > 'z')) {
        expr->text++;
        return active ? expr->count : 0;
    }
    if (op < '0' || op > '9') {
        expr->valid = 0;
        return 0;
    }
    do {
        unsigned digit = (unsigned)(*expr->text++ - '0');
        if (value > (UINT64_MAX - digit) / 10) expr->valid = 0;
        value = value * 10 + digit;
    } while (*expr->text >= '0' && *expr->text <= '9');
    return active ? value : 0;
}

enum { XGT_OR = 1, XGT_AND, XGT_EQ, XGT_NE, XGT_LT, XGT_LE,
    XGT_GT, XGT_GE, XGT_ADD, XGT_SUB, XGT_MUL, XGT_DIV, XGT_MOD };

static int xgt_operator(const char *text, int *width, int *precedence) {
    static const struct { const char *text; int op; int precedence; } ops[] = {
        {"||", XGT_OR, 1}, {"&&", XGT_AND, 2},
        {"==", XGT_EQ, 3}, {"!=", XGT_NE, 3},
        {"<=", XGT_LE, 4}, {">=", XGT_GE, 4},
        {"<", XGT_LT, 4}, {">", XGT_GT, 4},
        {"+", XGT_ADD, 5}, {"-", XGT_SUB, 5},
        {"*", XGT_MUL, 6}, {"/", XGT_DIV, 6}, {"%", XGT_MOD, 6}
    };
    size_t i;
    for (i = 0; i < sizeof(ops) / sizeof(ops[0]); i++) {
        size_t size = strlen(ops[i].text);
        if (strncmp(text, ops[i].text, size) == 0) {
            *width = (int)size;
            *precedence = ops[i].precedence;
            return ops[i].op;
        }
    }
    return 0;
}

static uint64_t xgt_apply(XgtExpr *expr, int op, uint64_t left,
    uint64_t right, int active) {
    if (!active) return 0;
    switch (op) {
        case XGT_OR: return left || right;
        case XGT_AND: return left && right;
        case XGT_EQ: return left == right;
        case XGT_NE: return left != right;
        case XGT_LT: return left < right;
        case XGT_LE: return left <= right;
        case XGT_GT: return left > right;
        case XGT_GE: return left >= right;
        case XGT_ADD: return left + right;
        case XGT_SUB: return left - right;
        case XGT_MUL: return left * right;
        case XGT_DIV:
        case XGT_MOD:
            if (right == 0) { expr->valid = 0; return 0; }
            return op == XGT_DIV ? left / right : left % right;
    }
    expr->valid = 0;
    return 0;
}

/* C 风格优先级；未选中的条件分支照常解析，但不会执行除法。 */
static uint64_t xgt_expression(XgtExpr *expr, int minimum, int active) {
    uint64_t left;
    if (++expr->depth > 48) { expr->valid = 0; return 0; }
    left = xgt_atom(expr, active);
    while (expr->valid) {
        int width = 0;
        int precedence = 0;
        int op;
        uint64_t right;
        xgt_space(expr);
        op = xgt_operator(expr->text, &width, &precedence);
        if (!op || precedence < minimum) break;
        expr->text += width;
        right = xgt_expression(expr, precedence + 1,
            active && (op != XGT_AND || left != 0) &&
            (op != XGT_OR || left == 0));
        left = xgt_apply(expr, op, left, right, active);
    }
    if (minimum == 0 && expr->valid && *expr->text == '?') {
        uint64_t yes, no;
        expr->text++;
        yes = xgt_expression(expr, 0, active && left != 0);
        xgt_space(expr);
        if (*expr->text != ':') expr->valid = 0;
        else expr->text++;
        no = xgt_expression(expr, 0, active && left == 0);
        left = left ? yes : no;
    }
    expr->depth--;
    return left;
}

static int xgt_rule(xvalue *catalog, char formula[257], unsigned *forms) {
    xvalue *header;
    xstrview text;
    const char *cursor, *end;
    int found = 0, have_forms = 0, have_formula = 0;
    *forms = 2;
    memcpy(formula, "n!=1", 5);
    if (!catalog || xrtValueType(catalog) != XVALUE_OBJECT) return 1;
    header = xrtValueObjectGet(catalog, (xstrview){"", 0});
    if (!header || xrtValueType(header) != XVALUE_STRING ||
        !xrtValueGetString(header, &text)) return 1;
    cursor = text.Data;
    end = cursor + text.Size;
    while (cursor < end) {
        const char *line_end = memchr(cursor, '\n', (size_t)(end - cursor));
        const char *part, *stop;
        if (!line_end) line_end = end;
        if (line_end - cursor >= 13 &&
            memcmp(cursor, "Plural-Forms:", 13) == 0) {
            if (found++) return 0;
            part = cursor + 13;
            while (part < line_end) {
                unsigned count = 0;
                while (part < line_end && (*part == ' ' || *part == '\t' ||
                    *part == ';' || *part == '\r')) part++;
                stop = memchr(part, ';', (size_t)(line_end - part));
                if (!stop) stop = line_end;
                if (stop - part >= 9 && memcmp(part, "nplurals=", 9) == 0) {
                    const char *p = part + 9;
                    if (have_forms++ || p == stop || *p < '0' || *p > '9') return 0;
                    while (p < stop && *p >= '0' && *p <= '9') {
                        count = count * 10 + (unsigned)(*p++ - '0');
                        if (count > 64) return 0;
                    }
                    while (p < stop && (*p == ' ' || *p == '\t' || *p == '\r')) p++;
                    if (p != stop || count == 0) return 0;
                    *forms = count;
                } else if (stop - part >= 7 && memcmp(part, "plural=", 7) == 0) {
                    const char *p = part + 7;
                    size_t size;
                    if (have_formula++) return 0;
                    while (p < stop && (*p == ' ' || *p == '\t')) p++;
                    while (stop > p && (stop[-1] == ' ' || stop[-1] == '\t' ||
                        stop[-1] == '\r')) stop--;
                    size = (size_t)(stop - p);
                    if (size == 0 || size > 256) return 0;
                    memcpy(formula, p, size);
                    formula[size] = '\0';
                } else if (part < stop) return 0;
                part = stop < line_end ? stop + 1 : line_end;
            }
        }
        cursor = line_end < end ? line_end + 1 : end;
    }
    return !found || (have_forms && have_formula);
}

int xgt_plural_index(xvalue *catalog, int64_t count, size_t *index) {
    char formula[257];
    unsigned forms;
    XgtExpr expr;
    uint64_t result;
    if (!index || count < 0 || !xgt_rule(catalog, formula, &forms)) return 0;
    expr.text = formula;
    expr.count = (uint64_t)count;
    expr.depth = 0;
    expr.valid = 1;
    result = xgt_expression(&expr, 0, 1);
    xgt_space(&expr);
    if (!expr.valid || *expr.text || result >= forms) return 0;
    *index = (size_t)result;
    return 1;
}

int xgt_plural_validate(xvalue *catalog) {
    static const int64_t samples[] = {0, 1, 2, 3, 5, 11, 21, 100, 1000};
    size_t i, index;
    for (i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        if (!xgt_plural_index(catalog, samples[i], &index)) return 0;
    }
    return 1;
}
