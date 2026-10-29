#ifndef XL_TABULATE_INTERNAL_H
#define XL_TABULATE_INTERNAL_H

#include "xrt.h"
#include "include/tabulate_native.h"

#define XTAB_MAX_COLS 32u
#define XTAB_MAX_ROWS 256u
#define XTAB_MAX_CELL 4096u
#define XTAB_MAX_SOURCE (1024u * 1024u)
#define XTAB_MAX_OUTPUT (4u * 1024u * 1024u)

typedef struct {
    char* text;
    size_t bytes;
    size_t width;
} xtab_cell;

typedef struct {
    xtab_cell cells[XTAB_MAX_COLS];
    size_t count;
} xtab_row;

typedef struct {
    xtab_row headers;
    xtab_row pending;
    xtab_row* rows;
    size_t row_count;
    size_t row_capacity;
    size_t source_bytes;
    size_t pending_bytes;
    size_t columns;
    size_t widths[XTAB_MAX_COLS];
    int pending_kind;
} xtab_table;

void xtab_error(xerrkind kind, const char* message);
bool xtab_measure(const char* text, size_t length, size_t* width);

#endif
