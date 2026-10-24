#ifndef XLANG_ANALYSIS_WORKER_H
#define XLANG_ANALYSIS_WORKER_H
#include "analysis/session.h"
#include "analysis/folding.h"
#include "analysis/check_session.h"
#include "analysis/completion_session.h"
#include "analysis/signature_session.h"

typedef struct xls_worker xls_worker;
typedef struct xls_query xls_query;
enum xls_query_state { XLS_QUERY_FAILED=-1,XLS_QUERY_PENDING=0,XLS_QUERY_READY=1,XLS_QUERY_CANCELLED=2 };
typedef struct xls_worker_load { size_t queued,running; uint64_t submitted,completed,cancelled,rejected; } xls_worker_load;

/* One XRT task-pool worker owns the analysis session. Submissions are bounded
 * and nonblocking. Destroy cancels accepted work and joins before freeing the
 * session. Retained result snapshots remain valid after worker destruction. */
xls_worker* xlsWorkerCreate(const xls_session_limits* limits,size_t queueLimit);
void xlsWorkerDestroy(xls_worker* worker);
int xlsWorkerLoad(xls_worker* worker,xls_worker_load* load);
/* Capture a complete immutable open-document input set. No body is copied;
 * each text snapshot is retained. The worker prunes closed documents, updates
 * current inputs and parses the target. Failure consumes none of the inputs. */
xls_query* xlsWorkerSyntax(xls_worker* worker,const xls_analysis_document* documents,
                         size_t count,const char* target);
xls_query* xlsWorkerFolding(xls_worker* worker,const xls_analysis_document* documents,
                          size_t count,const char* target,size_t rangeLimit);
/* Full semantic/typed-IR check on this actor, retaining the same input batch
 * and guarded checked result across queries. Use a separate actor for slow
 * background checks when foreground latency must remain independent. */
xls_query* xlsWorkerCheck(xls_worker* worker,const xls_analysis_document* documents,
                        size_t count,const char* target);
/* Same core context contract as check.h. Captures one immutable reference
 * before queueing; caller can immediately release its context afterwards.
 * With a non-NULL context, NULL target/entry selects context.entry without
 * inventing an opened document. Disk inputs remain guarded on cache hits. */
xls_query* xlsWorkerCheckInContext(xls_worker* worker,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target);
/* Exact cursor projections use their own retained session on this actor.
 * Use a dedicated foreground actor to avoid the full-check queue. */
xls_query* xlsWorkerCompletion(xls_worker* worker,const xls_analysis_document* documents,
    size_t count,const char* entry,const char* queryKey,size_t offset);
xls_query* xlsWorkerCompletionInContext(xls_worker* worker,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entry,const char* queryKey,size_t offset);
/* A separate retained signature session; ordinary completion/check state never
 * aliases a signature projection. Prefer a foreground actor for these queries. */
xls_query* xlsWorkerSignatureInContext(xls_worker* worker,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* entry,const char* queryKey,size_t offset);
void xlsQueryCancel(xls_query* query);
/* Register once from the submitting owner. Wake can run on the completing
 * worker or synchronously if already ready; it must be short/thread-safe and
 * must not destroy this query. Destroy removes and joins an active callback. */
int xlsQuerySetWake(xls_query* query,void (*wake)(void*),void* user);
void xlsQueryDestroy(xls_query* query);
/* A READY call transfers a syntax reference; output strings borrow it. Stats
 * count actual parser calls on this worker. Every other state clears outputs. */
enum xls_query_state xlsQueryResult(xls_query* query,xls_analysis_snapshot* result,
                                   xls_session_stats* stats,uint64_t* threadId);
/* Borrowed from a READY folding query, until that query is destroyed. */
const xls_folding* xlsQueryFolding(const xls_query* query);
/* READY transfers one checked-result reference, independent of query/actor
 * destruction. Counters describe actual full builds and validated reuse.
 * Every other state clears outputs; a failed analysis copies its error. */
enum xls_query_state xlsQueryCheckResult(xls_query* query,xls_semantic_check** result,
    xls_check_stats* stats,uint64_t* threadId,char* error,size_t capacity);
enum xls_query_state xlsQueryCompletionResult(xls_query* query,xls_semantic_completion** result,
    xls_completion_stats* stats,uint64_t* threadId,char* error,size_t capacity);
enum xls_query_state xlsQuerySignatureResult(xls_query* query,xls_signature_help** result,
    xls_signature_stats* stats,uint64_t* threadId,char* error,size_t capacity);
#endif
