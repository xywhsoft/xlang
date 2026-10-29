/* 表对象只持有单元格；排版和 Unicode 宽度在 render 单元。 */
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "tabulate_internal.h"
#include <string.h>

#define XTAB_DOMAIN "xlang.extlib.tabulate"

void xtab_error(xerrkind kind, const char* message)
{
    xrtSetErrorInfo(kind, XTAB_DOMAIN, 0, message);
}

static void xtab_clear_row(xtab_row* row)
{
    size_t i;
    for (i = 0u; i < row->count; ++i) xrtFree(row->cells[i].text);
    memset(row, 0, sizeof(*row));
}

static size_t xtab_row_bytes(const xtab_row* row)
{
    size_t i, total = 0u;
    for (i = 0u; i < row->count; ++i) total += row->cells[i].bytes;
    return total;
}

static void xtab_remeasure(xtab_table* table)
{
    size_t row, column;
    table->columns = table->headers.count;
    memset(table->widths, 0, sizeof(table->widths));
    for (column = 0u; column < table->headers.count; ++column)
        table->widths[column] = table->headers.cells[column].width;
    for (row = 0u; row < table->row_count; ++row) {
        const xtab_row* current = &table->rows[row];
        if (current->count > table->columns) table->columns = current->count;
        for (column = 0u; column < current->count; ++column)
            if (current->cells[column].width > table->widths[column])
                table->widths[column] = current->cells[column].width;
    }
}

void* xtab_create(void)
{
    xrtClearError();
    xtab_table* table = (xtab_table*)xrtMalloc(sizeof(*table));
    if (table != NULL) memset(table, 0, sizeof(*table));
    return table;
}

void xtab_destroy(void* handle)
{
    xtab_table* table = (xtab_table*)handle;
    size_t i;
    if (table == NULL) return;
    xtab_clear_row(&table->pending);
    xtab_clear_row(&table->headers);
    for (i = 0u; i < table->row_count; ++i) xtab_clear_row(&table->rows[i]);
    xrtFree(table->rows);
    xrtFree(table);
}

static int xtab_begin(xtab_table* table, int kind)
{
    xrtClearError();
    if (table == NULL || table->pending_kind != 0) {
        xtab_error(XERR_VALUE, "table is null or another row is pending");
        return 0;
    }
    if (kind == 2 && table->row_count >= XTAB_MAX_ROWS) {
        xtab_error(XERR_RANGE, "table exceeds 256 rows");
        return 0;
    }
    table->pending_kind = kind;
    return 1;
}

int xtab_begin_headers(void* handle) { return xtab_begin((xtab_table*)handle, 1); }
int xtab_begin_row(void* handle) { return xtab_begin((xtab_table*)handle, 2); }

int xtab_append_cell(void* handle, const char* text, long long length)
{
    xtab_table* table = (xtab_table*)handle;
    xtab_cell* cell;
    size_t base, width;
    char* copy;
    if (table == NULL || table->pending_kind == 0 || text == NULL ||
        length < 0 || length > XTAB_MAX_CELL) {
        xtab_error(XERR_VALUE, "invalid table, cell or cell length");
        xtab_abort(table);
        return 0;
    }
    if (table->pending.count >= XTAB_MAX_COLS) {
        xtab_error(XERR_RANGE, "table exceeds 32 columns");
        xtab_abort(table);
        return 0;
    }
    base = table->source_bytes;
    if (table->pending_kind == 1) base -= xtab_row_bytes(&table->headers);
    if ((size_t)length > XTAB_MAX_SOURCE - base - table->pending_bytes) {
        xtab_error(XERR_RANGE, "table exceeds 1 MiB of cell text");
        xtab_abort(table);
        return 0;
    }
    if (memchr(text, '\0', (size_t)length) != NULL ||
        !xtab_measure(text, (size_t)length, &width)) {
        xtab_error(XERR_VALUE, "cell contains NUL, control character or invalid UTF-8");
        xtab_abort(table);
        return 0;
    }
    copy = (char*)xrtMalloc((size_t)length + 1u);
    if (copy == NULL) { xtab_abort(table); return 0; }
    memcpy(copy, text, (size_t)length);
    copy[length] = '\0';
    cell = &table->pending.cells[table->pending.count++];
    cell->text = copy;
    cell->bytes = (size_t)length;
    cell->width = width;
    table->pending_bytes += (size_t)length;
    return 1;
}

int xtab_commit(void* handle)
{
    xtab_table* table = (xtab_table*)handle;
    xtab_row* rows;
    size_t capacity;
    if (table == NULL || table->pending_kind == 0) {
        xtab_error(XERR_VALUE, "there is no pending row");
        return 0;
    }
    if (table->pending_kind == 2 && table->pending.count == 0u) {
        xtab_error(XERR_VALUE, "a data row cannot be empty");
        xtab_abort(table);
        return 0;
    }
    if (table->pending_kind == 1) {
        table->source_bytes -= xtab_row_bytes(&table->headers);
        xtab_clear_row(&table->headers);
        table->headers = table->pending;
    } else {
        if (table->row_count == table->row_capacity) {
            capacity = table->row_capacity == 0u ? 4u : table->row_capacity * 2u;
            if (capacity > XTAB_MAX_ROWS) capacity = XTAB_MAX_ROWS;
            rows = (xtab_row*)xrtRealloc(table->rows, capacity * sizeof(*rows));
            if (rows == NULL) { xtab_abort(table); return 0; }
            table->rows = rows;
            table->row_capacity = capacity;
        }
        table->rows[table->row_count++] = table->pending;
    }
    memset(&table->pending, 0, sizeof(table->pending));
    table->source_bytes += table->pending_bytes;
    table->pending_bytes = 0u;
    table->pending_kind = 0;
    xtab_remeasure(table);
    return 1;
}

void xtab_abort(void* handle)
{
    xtab_table* table = (xtab_table*)handle;
    if (table == NULL) return;
    xtab_clear_row(&table->pending);
    table->pending_bytes = 0u;
    table->pending_kind = 0;
}

static int xtab_write_row(void* handle, const char* const* cells,
    size_t count, int headers)
{
    size_t i;
    xrtClearError();
    if (count > XTAB_MAX_COLS || (count != 0u && cells == NULL)) {
        xtab_error(XERR_VALUE, "invalid cell array or column count");
        return 0;
    }
    if (!(headers ? xtab_begin_headers(handle) : xtab_begin_row(handle))) return 0;
    for (i = 0u; i < count; ++i) {
        size_t length = 0u;
        if (cells[i] == NULL) {
            xtab_error(XERR_VALUE, "cell is null");
            xtab_abort(handle);
            return 0;
        }
        while (length <= XTAB_MAX_CELL && cells[i][length] != '\0') ++length;
        if (!xtab_append_cell(handle, cells[i], (long long)length)) {
            xtab_abort(handle);
            return 0;
        }
    }
    if (!xtab_commit(handle)) { xtab_abort(handle); return 0; }
    return 1;
}

int xtab_set_headers(void* table, const char* const* cells, size_t count)
{ return xtab_write_row(table, cells, count, 1); }

int xtab_add_row(void* table, const char* const* cells, size_t count)
{ return xtab_write_row(table, cells, count, 0); }

int64_t xtab_row_count(void* handle)
{
    xrtClearError();
    if (handle == NULL) { xtab_error(XERR_VALUE, "table is null"); return -1; }
    return (long long)((xtab_table*)handle)->row_count;
}

int64_t xtab_col_count(void* handle)
{
    xrtClearError();
    if (handle == NULL) { xtab_error(XERR_VALUE, "table is null"); return -1; }
    return (long long)((xtab_table*)handle)->columns;
}
