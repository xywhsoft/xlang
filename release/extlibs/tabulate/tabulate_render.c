/* XRT 负责 UTF-8 解码与输出缓冲；终端宽度按常用 CJK/组合符范围估算。 */
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#include "tabulate_internal.h"
#include <string.h>

static size_t xtab_scalar_width(uint32 scalar)
{
    if ((scalar >= 0x0300u && scalar <= 0x036fu) ||
        (scalar >= 0x1ab0u && scalar <= 0x1affu) ||
        (scalar >= 0x1dc0u && scalar <= 0x1dffu) ||
        (scalar >= 0x20d0u && scalar <= 0x20ffu) ||
        (scalar >= 0xfe00u && scalar <= 0xfe0fu) ||
        (scalar >= 0xfe20u && scalar <= 0xfe2fu) ||
        (scalar >= 0xe0100u && scalar <= 0xe01efu)) return 0u;
    if ((scalar >= 0x1100u && scalar <= 0x115fu) ||
        scalar == 0x2329u || scalar == 0x232au ||
        (scalar >= 0x2e80u && scalar <= 0xa4cfu) ||
        (scalar >= 0xac00u && scalar <= 0xd7a3u) ||
        (scalar >= 0xf900u && scalar <= 0xfaffu) ||
        (scalar >= 0xfe10u && scalar <= 0xfe6fu) ||
        (scalar >= 0xff00u && scalar <= 0xff60u) ||
        (scalar >= 0xffe0u && scalar <= 0xffe6u) ||
        (scalar >= 0x1f300u && scalar <= 0x1faffu) ||
        (scalar >= 0x20000u && scalar <= 0x3fffdu)) return 2u;
    return 1u;
}

bool xtab_measure(const char* text, size_t length, size_t* width)
{
    size_t position = 0u, columns = 0u;
    while (position < length) {
        xstrview view = {text + position, length - position};
        uint32 scalar;
        size_t read;
        /* 不可见格式符和双向控制符会破坏终端列宽与阅读顺序。 */
        if (xrtUtf8Decode(view, &scalar, &read) != XUTF_OK || read == 0u ||
            scalar < 0x20u || (scalar >= 0x7fu && scalar <= 0x9fu) ||
            scalar == 0x00adu ||
            scalar == 0x061cu || (scalar >= 0x200bu && scalar <= 0x200fu) ||
            (scalar >= 0x202au && scalar <= 0x202eu) ||
            scalar == 0x2028u || scalar == 0x2029u ||
            (scalar >= 0x2060u && scalar <= 0x206fu) ||
            scalar == 0xfeffu) return false;
        columns += xtab_scalar_width(scalar);
        position += read;
    }
    *width = columns;
    return true;
}

static bool xtab_append(xbuffer* output, const void* data, size_t length)
{
    if (length == 0u) return true;
    if (length > XTAB_MAX_OUTPUT - 1u - output->Size) {
        xtab_error(XERR_RANGE, "rendered table exceeds 4 MiB");
        return false;
    }
    return xrtBufferAppend(output, (xbytesview){(cbytes)data, length});
}

static bool xtab_fill(xbuffer* output, char byte, size_t count)
{
    bytes target;
    if (count == 0u) return true;
    if (count > XTAB_MAX_OUTPUT - 1u - output->Size) {
        xtab_error(XERR_RANGE, "rendered table exceeds 4 MiB");
        return false;
    }
    target = xrtBufferAdd(output, count);
    if (target == NULL) return false;
    memset(target, byte, count);
    return true;
}

static bool xtab_border(xbuffer* output, const xtab_table* table)
{
    size_t column;
    for (column = 0u; column < table->columns; ++column) {
        if (!xtab_append(output, "+", 1u) ||
            !xtab_fill(output, '-', table->widths[column] + 2u)) return false;
    }
    return xtab_append(output, "+\n", 2u);
}

static bool xtab_row_text(xbuffer* output, const xtab_table* table,
    const xtab_row* row)
{
    size_t column;
    for (column = 0u; column < table->columns; ++column) {
        const xtab_cell* cell = column < row->count ? &row->cells[column] : NULL;
        size_t width = cell != NULL ? cell->width : 0u;
        if (!xtab_append(output, "| ", 2u) ||
            (cell != NULL && !xtab_append(output, cell->text, cell->bytes)) ||
            !xtab_fill(output, ' ', table->widths[column] - width) ||
            !xtab_append(output, " ", 1u)) return false;
    }
    return xtab_append(output, "|\n", 2u);
}

char* xtab_render(void* handle)
{
    xtab_table* table = (xtab_table*)handle;
    xbuffer output;
    char* result = NULL;
    size_t row;
    xrtClearError();
    if (table == NULL || table->pending_kind != 0) {
        xtab_error(XERR_VALUE, "table is null or a row is pending");
        return NULL;
    }
    if (!xrtBufferInit(&output)) return NULL;
    if (table->columns != 0u) {
        if (!xtab_border(&output, table)) goto done;
        if (table->headers.count != 0u) {
            if (!xtab_row_text(&output, table, &table->headers) ||
                !xtab_border(&output, table)) goto done;
        }
        for (row = 0u; row < table->row_count; ++row)
            if (!xtab_row_text(&output, table, &table->rows[row])) goto done;
        if (!xtab_border(&output, table)) goto done;
    }
    if (!xrtBufferAppendByte(&output, '\0')) goto done;
    result = (char*)xrtBufferTake(&output, NULL, NULL);
done:
    xrtBufferUnit(&output);
    return result;
}
