#ifndef XLANG_ANALYSIS_FOLDING_H
#define XLANG_ANALYSIS_FOLDING_H
#include "analysis/session.h"
typedef struct xls_folding xls_folding;
typedef struct xls_fold_range { size_t startLine,endLine; } xls_fold_range;
/* Structural ranges come only from the shared parser AST. Zero-based complete
 * lines, sorted by start then enclosing range first, with duplicates removed.
 * A range limit bounds output; zero requests an empty result. Invalid syntax
 * currently has no ranges. Cancellation/allocation failure returns NULL. */
xls_folding* xlsFoldingCreate(const xls_syntax* syntax,size_t limit,int (*cancelled)(void*),void* user);
void xlsFoldingDestroy(xls_folding* folding);
size_t xlsFoldingCount(const xls_folding* folding);
const xls_fold_range* xlsFoldingRanges(const xls_folding* folding);
#endif
