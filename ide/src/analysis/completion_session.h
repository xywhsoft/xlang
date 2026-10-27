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
    uint64_t inputScopeHits;
    uint64_t wordViews,wordProofNodes;
} xls_completion_stats;
/* One owner, one retained exact cursor projection. Hits revalidate disk,
 * import candidates and observed environment. Unobserved body changes may hit
 * when the original lookup mask and unchanged overlay membership prove them
 * irrelevant;entry/query bodies stay exact,inputScopeHits counts these hits.
 * Proven edits/caret movement inside the same original-parser token may
 * directly read independent candidate/source/memo/topology units. Both input
 * bodies are bound exactly;all parser values and induced coordinates match;
 * filtering never broadens beyond the original unit's prefix. wordViews and
 * wordProofNodes count this construction separately from full semantic builds.
 * Other changed inputs/cursors run the shared semantic query.
 * Opened entry/import query files can reuse
 * immutable cursor syntax,including project entries that are closed. Source
 * loading/reporting,imports and semantic checking still run. Failed cursor
 * cache construction/copy uses the original shared native query path,
 * counted by cursorFreshBuilds. An ordinary entry AST may now be reused by
 * that path; the result's EntryWork reports its actual parse/copy work.
 * Module prerequisites still run. Final IR annotations are deferred only for
 * successful private cursor results and materialized before full AST exposure.
 * Query-neutral functions
 * may reuse bounded immutable semantic facts,including query-source siblings
 * with exact old/current marker provenance and exclusion proofs. The active
 * projection and uncertain identities/coordinates use the original checker.
 * Returned reference outlives the session. */
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
