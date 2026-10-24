#ifndef XLANG_ANALYSIS_AST_INTERN_H
#define XLANG_ANALYSIS_AST_INTERN_H
#include "analysis/ast_clone.h"
typedef struct xls_ast_intern xls_ast_intern;
typedef struct xls_ast_intern_work {
    uint64_t references,uniqueRoots,hits,treeBytes,keyBytes,peakKeyBytes,fallbacks;
} xls_ast_intern_work;
/* Capture independent immutable roots with exact scalar/string/literal/source
 * and recursive tree values. Capacity,allocation owner and source refcounts
 * are ownership metadata,not identity. All returned roots borrow this owner;
 * callers must not mutate/free them. Clone before compiler use.
 * On success rewrite values to owned roots. Failure leaves every value/input
 * unchanged. Identity-index resource/byte failures use the original independent
 * clone forest,without changing the represented language or type values.
 * bytes bounds original clone data;keyBytes bounds logical temporary index
 * keys/values (max8MiB),not allocator/container overhead or a process RSS cap.
 * Zero keyBytes disables interning for correctness/performance comparison. */
xls_ast_intern* xlsAstInternCapture(const XL6_AstNode** values,size_t count,size_t bytes,size_t keyBytes,xls_ast_intern_work* work);
void xlsAstInternDestroy(xls_ast_intern* owner);
xls_ast_intern_work xlsAstInternWork(const xls_ast_intern* owner);
#endif
