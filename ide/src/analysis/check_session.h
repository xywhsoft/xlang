#ifndef XLANG_ANALYSIS_CHECK_SESSION_H
#define XLANG_ANALYSIS_CHECK_SESSION_H
#include "analysis/session.h"
#include "analysis/check.h"

typedef struct xls_check_session xls_check_session;
typedef struct xls_check_stats {
    uint64_t builds,hits,invalidations;
    size_t documents,textBytes;
    uint64_t entryFullParses,entryFragmentParses,entryReusedDeclarations,entryParsedBytes,entrySyntaxHits;
    uint64_t semanticFunctionsChecked,semanticFunctionsReused,semanticContextHits,semanticMemoBytes;
} xls_check_stats;
/* One owner thread; retains one checked compilation and its immutable input
 * batch, bounded by the existing native check limits. No transport/GUI types.
 * A changed body/identity/path/input set/entry/index mode/working directory
 * rebuilds. Exact-input reuse still revalidates disk files, negative import
 * candidates and observed package environment. Rebuilds reuse the immutable
 * entry syntax when possible, copying a private mutable tree for unchanged
 * full semantic/IR verification. Dependency parsing is still performed anew. */
xls_check_session* xlsCheckSessionCreate(const xls_session_limits* limits);
void xlsCheckSessionDestroy(xls_check_session* session);
/* Returned reference is independent of session replacement/destruction.
 * Same identity rejects decreasing revisions and changed text at an existing
 * revision. Same body at a newer revision can reuse facts; wire versions are
 * the adapter's responsibility. Failure/cancellation preserves the last batch. */
xls_semantic_check* xlsCheckSessionCheck(xls_check_session* session,
    const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Explicit contexts are retained by the accepted batch/result. Context
 * changes invalidate cached semantics and syntax baselines; full equality
 * permits independently captured equivalent descriptors to hit the cache.
 * NULL target selects context.entry. The shared frontend loads/guards a disk
 * entry without a synthetic overlay; exact queries retain its checked result.
 * Its syntax is not yet retained across rebuilds; entry syntax counters count
 * session-built overlay syntax only. */
xls_semantic_check* xlsCheckSessionCheckInContext(xls_check_session* session,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity);
void xlsCheckSessionStats(const xls_check_session* session,xls_check_stats* stats);
#endif
