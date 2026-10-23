#ifndef XLANG_ANALYSIS_COMPLETION_SESSION_H
#define XLANG_ANALYSIS_COMPLETION_SESSION_H
#include "analysis/session.h"
#include "analysis/semantic.h"
typedef struct xls_completion_session xls_completion_session;
typedef struct xls_completion_stats {
    uint64_t builds,hits,invalidations;
    size_t documents,textBytes;
    uint64_t cursorFullParses,cursorFragmentParses,cursorReusedDeclarations,cursorParsedBytes;
    uint64_t cursorSyntaxHits,cursorFreshBuilds;
} xls_completion_stats;
/* One owner, one retained exact cursor projection. Hits revalidate disk,
 * import candidates and observed environment; different inputs/cursors still
 * run the shared semantic query. Entry cursor queries can reuse immutable
 * syntax; imports and semantic checking still run afresh. Other query files
 * and failed cache construction use the original native query path, counted
 * explicitly by cursorFreshBuilds. Returned reference outlives the session. */
xls_completion_session* xlsCompletionSessionCreate(const xls_session_limits* limits);
void xlsCompletionSessionDestroy(xls_completion_session* session);
/* UTF-8 offset after BOM. Increasing same-body revisions can reuse facts;
 * wire versions are separate. Failure/cancellation preserves the old batch. */
xls_semantic_completion* xlsCompletionSessionQuery(xls_completion_session* session,
    const xls_analysis_document* documents,size_t count,const char* entryKey,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* NULL entryKey requires a context and selects its entry. The query remains
 * an actual opened overlay. Exact results retain/revalidate disk input proofs;
 * changed inputs/cursors still run the shared fresh path, counted explicitly. */
xls_semantic_completion* xlsCompletionSessionQueryInContext(xls_completion_session* session,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entryKey,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity);
void xlsCompletionSessionStats(const xls_completion_session* session,xls_completion_stats* stats);
#endif
