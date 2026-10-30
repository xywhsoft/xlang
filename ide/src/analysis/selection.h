#ifndef XLANG_ANALYSIS_SELECTION_H
#define XLANG_ANALYSIS_SELECTION_H
#include "analysis/session.h"

#define XLS_SELECTION_POSITIONS 256u
#define XLS_SELECTION_DEPTH 128u
typedef struct xls_selection xls_selection;
typedef struct xls_selection_range {
    size_t start,end;xls_position startPosition,endPosition;
} xls_selection_range;
/* Raw UTF-8 offsets include the source BOM. Positions must be scalar/CRLF
 * boundaries. One shared-parser traversal serves the whole batch, preserving
 * its order and duplicate positions. Ranges grow strictly outward and finish
 * at the full document; parser errors give an empty cursor range and document.
 * Output owns only offsets. No AST, protocol or UI type escapes this boundary.
 * At most 8MiB candidate capacity and 128 levels per position. Invalid input,
 * budget/depth exhaustion, cancellation or allocation failure returns NULL. */
xls_selection* xlsSelectionCreate(const xls_syntax* syntax,const size_t* offsets,
    size_t count,int (*cancelled)(void*),void* user);
void xlsSelectionDestroy(xls_selection* selection);
size_t xlsSelectionCount(const xls_selection* selection);
size_t xlsSelectionDepth(const xls_selection* selection,size_t position);
int xlsSelectionGet(const xls_selection* selection,size_t position,size_t level,
    xls_selection_range* range);
#endif
