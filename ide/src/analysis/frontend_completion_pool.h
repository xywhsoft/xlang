#ifndef XLS_FRONTEND_COMPLETION_POOL_H
#define XLS_FRONTEND_COMPLETION_POOL_H
#include "src/sem/xl6_completion.h"
/* Passive observation after the ORIGINAL private-name filter. Selections are
 * borrowed only within their current result. A persistent unit later clones
 * its minimal location facts; no previous semantic/AST owner is retained. */
typedef struct xls_completion_pool {
    XL6_CompletionQuery query;
    xstrbuf strings;
    size_t admittedBytes;
} xls_completion_pool;
/* Offset records stay valid while the XRT string buffer grows. Only End
 * publishes native item pointers,after the final buffer address is fixed. */
typedef struct xls_completion_pool_record {
    size_t name,canonical;
    const char* kind;
    XL6_SymbolId symbolId;
    const XL6_AstNode* selection;
} xls_completion_pool_record;
typedef struct xls_completion_pool_build {
    xls_completion_pool_record* items;
    size_t count,capacity,bytes;
    const char* error;
} xls_completion_pool_build;
typedef struct xls_completion_pool_scope {
    struct xls_completion_pool_scope* previous;
    struct xls_completion_pool_scope** slot;
    XL6_CompletionQuery* target;
    xls_completion_pool_build build;
    xstrbuf strings;
    size_t limit;
    int failed;
} xls_completion_pool_scope;
void xlsCompletionPoolBegin(xls_completion_pool_scope*,XL6_CompletionQuery*,size_t);
/* Balanced even after native refusal/cancellation. Zero only disables this
 * optional producer; it never changes the original filtered query/error. */
int xlsCompletionPoolEnd(xls_completion_pool_scope*,xls_completion_pool*);
void xlsCompletionPoolFree(xls_completion_pool*);
/* Context teardown refuses and frees any still-linked optional producer.
 * Its stack owner can subsequently End safely;the native query stays owned
 * by its caller. Normal paired scopes never take this recovery path. */
void xlsCompletionPoolAbort(xls_completion_pool_scope**);
#endif
