/*
 * tabulate_native.c - Terminal table formatting
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
extern void xrtSetError(const void *msg, int bFree);
static char tab_buf[16384];

#define TAB_MAX_COLS 32
#define TAB_MAX_ROWS 256
#define TAB_CELL_MAX 256

typedef struct {
    char headers[TAB_MAX_COLS][TAB_CELL_MAX];
    char rows[TAB_MAX_ROWS][TAB_MAX_COLS][TAB_CELL_MAX];
    int col_widths[TAB_MAX_COLS];
    int num_cols;
    int num_rows;
    int style;  /* 0=ascii, 1=rounded */
} XTab;

void *xtab_create(void) {
    XTab *t = (XTab *)calloc(1, sizeof(XTab));
    if (!t) { xrtSetError("tabulate: out of memory", 0); return NULL; }
    return t;
}

void xtab_destroy(void *ptr) {
    free(ptr);
}

void xtab_set_headers(void *ptr, const char *csv) {
    XTab *t = (XTab *)ptr;
    char buf[4096];
    char *tok;
    int col = 0;
    if (!t || !csv) return;
    strncpy(buf, csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    tok = strtok(buf, ",");
    while (tok && col < TAB_MAX_COLS) {
        /* trim spaces */
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
        strncpy(t->headers[col], tok, TAB_CELL_MAX - 1);
        int len = (int)strlen(tok);
        if (len > t->col_widths[col]) t->col_widths[col] = len;
        col++;
        tok = strtok(NULL, ",");
    }
    t->num_cols = col;
}

void xtab_add_row(void *ptr, const char *csv) {
    XTab *t = (XTab *)ptr;
    char buf[4096];
    char *tok;
    int col = 0;
    if (!t || !csv || t->num_rows >= TAB_MAX_ROWS) return;
    strncpy(buf, csv, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    tok = strtok(buf, ",");
    while (tok && col < TAB_MAX_COLS) {
        while (*tok == ' ') tok++;
        char *end = tok + strlen(tok) - 1;
        while (end > tok && *end == ' ') *end-- = '\0';
        strncpy(t->rows[t->num_rows][col], tok, TAB_CELL_MAX - 1);
        int len = (int)strlen(tok);
        if (len > t->col_widths[col]) t->col_widths[col] = len;
        col++;
        tok = strtok(NULL, ",");
    }
    if (col > t->num_cols) t->num_cols = col;
    t->num_rows++;
}

static void tab_separator(XTab *t, char *out, int *pos, int max) {
    int i, j;
    *pos += snprintf(out + *pos, max - *pos, "+");
    for (i = 0; i < t->num_cols; i++) {
        for (j = 0; j < t->col_widths[i] + 2; j++) {
            if (*pos < max - 1) out[(*pos)++] = '-';
        }
        if (*pos < max - 1) out[(*pos)++] = '+';
    }
    if (*pos < max - 1) out[(*pos)++] = '\n';
    out[*pos] = '\0';
}

static void tab_data_row(XTab *t, char cells[][TAB_CELL_MAX], char *out, int *pos, int max) {
    int i;
    *pos += snprintf(out + *pos, max - *pos, "|");
    for (i = 0; i < t->num_cols; i++) {
        *pos += snprintf(out + *pos, max - *pos, " %-*s |", t->col_widths[i], cells[i]);
    }
    if (*pos < max - 1) out[(*pos)++] = '\n';
    out[*pos] = '\0';
}

const char *xtab_render(void *ptr) {
    XTab *t = (XTab *)ptr;
    int pos = 0, i;
    int max = (int)sizeof(tab_buf);
    if (!t || t->num_cols == 0) return "";

    tab_buf[0] = '\0';
    tab_separator(t, tab_buf, &pos, max);
    if (t->headers[0][0]) {
        tab_data_row(t, t->headers, tab_buf, &pos, max);
        tab_separator(t, tab_buf, &pos, max);
    }
    for (i = 0; i < t->num_rows; i++) {
        tab_data_row(t, t->rows[i], tab_buf, &pos, max);
    }
    tab_separator(t, tab_buf, &pos, max);
    return tab_buf;
}

int xtab_row_count(void *ptr) {
    XTab *t = (XTab *)ptr;
    return t ? t->num_rows : 0;
}

int xtab_col_count(void *ptr) {
    XTab *t = (XTab *)ptr;
    return t ? t->num_cols : 0;
}
