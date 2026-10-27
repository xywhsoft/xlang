#ifndef XLANG_ANALYSIS_SESSION_STATE_H
#define XLANG_ANALYSIS_SESSION_STATE_H
#include "analysis/input_batch.h"
#include "analysis/check_session.h"
#include "analysis/completion_session.h"
#include "analysis/signature_session.h"
#include "analysis/dependency_session.h"

/* Worker-private borrowed routing state. It remains valid until the next
 * successful query or session destruction;not a source freshness proof. */
typedef struct xls_session_state {const xls_input_batch* inputs;const char* queryKey;} xls_session_state;
/* Allocation-free semantic-state checkpoint for the worker's final cancellation
 * gate. Restore consumes retained owners;actual work counters and source-only
 * parse aids may advance,but accepted facts/input history remain unchanged. */
typedef struct xls_semantic_checkpoint {
    xls_input_batch* inputs;xls_semantic_check* check;xls_semantic_completion* completion;
    xls_signature_help* signature;xls_dependency_capture* dependency;xls_syntax* syntax;const char* queryKey;
    uint64_t usedInputs;size_t offset,documents,textBytes;uint64_t memoBytes;
    int usageKnown,captureIndex;
} xls_semantic_checkpoint;
xls_semantic_checkpoint xlsCheckSessionCheckpoint(const xls_check_session* session);
xls_semantic_checkpoint xlsCompletionSessionCheckpoint(const xls_completion_session* session);
xls_semantic_checkpoint xlsSignatureSessionCheckpoint(const xls_signature_session* session);
xls_semantic_checkpoint xlsDependencySessionCheckpoint(const xls_dependency_session* session);
void xlsCheckSessionRestore(xls_check_session* session,xls_semantic_checkpoint* checkpoint);
void xlsCompletionSessionRestore(xls_completion_session* session,xls_semantic_checkpoint* checkpoint);
void xlsSignatureSessionRestore(xls_signature_session* session,xls_semantic_checkpoint* checkpoint);
void xlsDependencySessionRestore(xls_dependency_session* session,xls_semantic_checkpoint* checkpoint);
xls_session_state xlsCheckSessionState(const xls_check_session* session);
xls_session_state xlsCompletionSessionState(const xls_completion_session* session);
xls_session_state xlsSignatureSessionState(const xls_signature_session* session);
xls_session_state xlsDependencySessionState(const xls_dependency_session* session);
xls_dependency_capture* xlsDependencySessionCaptureWithHistory(xls_dependency_session* session,xls_context* context,
    const xls_input_batch* history,const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* The worker supplies its latest accepted full-stream revision history.
 * Semantic/syntax baselines stay local to the selected root. Public session
 * entrypoints continue using their own accepted batch as revision history.
 * This prevents switching roots from either accepting stale versions or
 * rejecting a valid stream based on an unrelated old root's input history. */
xls_semantic_check* xlsCheckSessionCheckWithHistory(xls_check_session* session,xls_context* context,
    const xls_input_batch* history,const xls_analysis_document* documents,size_t count,const char* target,
    int captureIndex,int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_semantic_completion* xlsCompletionSessionQueryWithHistory(xls_completion_session* session,xls_context* context,
    const xls_input_batch* history,const xls_analysis_document* documents,size_t count,const char* entry,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_signature_help* xlsSignatureSessionQueryWithHistory(xls_signature_session* session,xls_context* context,
    const xls_input_batch* history,const xls_analysis_document* documents,size_t count,const char* entry,
    const char* queryKey,size_t offset,int (*cancelled)(void*),void* user,char* error,size_t capacity);
#endif
