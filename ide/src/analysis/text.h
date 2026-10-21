#ifndef XLANG_ANALYSIS_TEXT_H
#define XLANG_ANALYSIS_TEXT_H
#include <stddef.h>

/* Immutable UTF-8 source, independently owned from transport and editor state. */
typedef struct xls_text xls_text;
typedef struct xls_position { size_t line, character; } xls_position;
typedef struct xls_text_edit {
    const char* text;
    size_t length;
    xls_position start, end;
    int hasRange, hasRangeLength;
    size_t rangeLength;
} xls_text_edit;
enum xls_text_error {
    XLS_TEXT_OK, XLS_TEXT_ARGUMENT, XLS_TEXT_ENCODING,
    XLS_TEXT_RANGE, XLS_TEXT_LIMIT, XLS_TEXT_MEMORY
};

xls_text* xlsTextCreate(const char* text, size_t length, size_t limit, enum xls_text_error* error);
xls_text* xlsTextRetain(xls_text* text);
void xlsTextRelease(xls_text* text);
const char* xlsTextData(const xls_text* text);
size_t xlsTextSize(const xls_text* text);
size_t xlsTextLines(const xls_text* text);
/* UTF-16 positions. Columns beyond EOL clamp to EOL; nonexistent lines and
 * surrogate-interior positions fail. Newlines retain original LF/CRLF/CR. */
int xlsTextOffset(const xls_text* text, xls_position position, size_t* offset);
/* Indexed UTF-8 byte offset to UTF-16 position; rejects scalar/CRLF interiors. */
int xlsTextPosition(const xls_text* text, size_t offset, xls_position* position);
/* One scalar/CRLF-aligned edit. Result borrows updated text until it is released.
 * Returns 1 for a change, 0 for identical content, -1 for invalid arguments. */
int xlsTextDiff(const xls_text* base, const xls_text* updated, xls_text_edit* edit);
/* Changes are sequential against the preceding change's output. Failure leaves
 * base unchanged. An empty change batch retains base. Returned result is owned.
 * M1 copies the text and rebuilds its line index; this is not semantic reuse. */
xls_text* xlsTextApply(const xls_text* base, const xls_text_edit* edits, size_t count,
                     size_t limit, enum xls_text_error* error);
const char* xlsTextError(enum xls_text_error error);
#endif
