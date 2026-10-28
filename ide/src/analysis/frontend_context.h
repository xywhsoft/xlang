#ifndef XLANG_ANALYSIS_FRONTEND_CONTEXT_H
#define XLANG_ANALYSIS_FRONTEND_CONTEXT_H
#include "analysis/context.h"
/* Private call scope, never a process CWD/environment mutation. The caller
 * keeps context alive. Nested/default calls restore the preceding scope. */
struct xls_completion_pool_scope;
struct xls_raw_scope;
typedef struct xls_frontend_context_scope {
    const xls_context* context;
    struct xls_frontend_context_scope* previous;
    int failed;
    void* namespaceNames;
    struct xls_completion_pool_scope* completionPool;
    struct xls_raw_scope* rawBodies;
} xls_frontend_context_scope;
int xlsFrontendContextValid(const xls_context* context,const char* entry,char* error,size_t capacity);
int xlsFrontendContextsEqual(const xls_context* a,const xls_context* b);
void xlsFrontendContextBegin(xls_frontend_context_scope* scope,const xls_context* context);
int xlsFrontendContextEnd(xls_frontend_context_scope* scope);
const xls_context* xlsFrontendContextCurrent(void);
/* Private optional producer slot. Shares the existing call-scope TLS;no
 * additional thread-local lookup domain or heap sidecar is introduced. */
struct xls_completion_pool_scope** xlsFrontendCompletionPoolSlot(void);
struct xls_raw_scope** xlsFrontendRawBodySlot(void);
/* XRT-owned return values. Only the audited shared report/package adapters
 * use these: raw environment spelling is preserved in its observed proof. */
char* xlsFrontendContextPackagePath(void);
char* xlsFrontendContextAbsolute(const char* path);
/* Query-only export labels. Lookup/alias ownership uses XRT, scoped exactly
 * like the producing native frontend call; ordinary checks allocate nothing. */
int xlsFrontendNamespaceReady(void);
int xlsFrontendNamespaceStart(void);
int xlsFrontendNamespaceAdd(const char* internal,const char* display);
int xlsFrontendNamespaceActive(void);
size_t xlsFrontendNamespaceCount(const char* internal);
const char* xlsFrontendNamespaceName(const char* internal,size_t index);
#endif
