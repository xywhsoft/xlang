#ifndef XLANG_ANALYSIS_SIGNATURE_SESSION_H
#define XLANG_ANALYSIS_SIGNATURE_SESSION_H
#include "analysis/session.h"
#include "analysis/signature.h"
typedef struct xls_signature_session xls_signature_session;
typedef struct xls_signature_stats {
    uint64_t builds,hits,invalidations,indexBuilds,indexHits,caretHits;
    size_t documents,textBytes;
    uint64_t inputScopeHits;
} xls_signature_stats;
/* One owner,independent from ordinary cursor completion. Exact results recheck
 * captured disk/resolution inputs; caret-only changes share a proven immutable
 * call index; closed calls in the same proven argument may share an immutable
 * semantic model with independent offset views. Original frontend input usage
 * may ignore unobserved body edits with unchanged overlay membership;entry and
 * query bodies remain exact,inputScopeHits counts such reuse. Query edits rebuild that index; compatible signatures may restore the
 * existing query-neutral semantic function memo. Cancellation commits no batch.
 * Returned references remain valid after replacement/session destruction. */
xls_signature_session* xlsSignatureSessionCreate(const xls_session_limits* limits);
void xlsSignatureSessionDestroy(xls_signature_session* session);
xls_signature_help* xlsSignatureSessionQueryInContext(xls_signature_session* session,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entry,const char* query,size_t offset,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
void xlsSignatureSessionStats(const xls_signature_session* session,xls_signature_stats* stats);
#endif
