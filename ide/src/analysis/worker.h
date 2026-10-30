#ifndef XLANG_ANALYSIS_WORKER_H
#define XLANG_ANALYSIS_WORKER_H
#include "analysis/session.h"
#include "analysis/folding.h"
#include "analysis/selection.h"
#include "analysis/check_session.h"
#include "analysis/completion_session.h"
#include "analysis/signature_session.h"
#include "analysis/dependency_session.h"

typedef struct xls_worker xls_worker;
typedef struct xls_query xls_query;
enum xls_query_state { XLS_QUERY_FAILED=-1,XLS_QUERY_PENDING=0,XLS_QUERY_READY=1,XLS_QUERY_CANCELLED=2 };
typedef struct xls_worker_load { size_t queued,running; uint64_t submitted,completed,cancelled,rejected; } xls_worker_load;
typedef struct xls_worker_cache_limits {size_t sessions,inputStorageBytes;} xls_worker_cache_limits;
typedef struct xls_worker_cache_stats {
    uint64_t hits,misses,evictions,pruned,allocationFallbacks;
    size_t sessions,inputStorageBytes,sessionLimit,inputStorageLimit;
    int inputBudgetExceeded;
} xls_worker_cache_stats;

/* One XRT task-pool worker owns the analysis session. Submissions are bounded
 * and nonblocking. Destroy cancels accepted work and joins before freeing the
 * session. Retained result snapshots remain valid after worker destruction. */
xls_worker* xlsWorkerCreate(const xls_session_limits* limits,size_t queueLimit);
/* Default:8 retained semantic sessions,64MiB known input storage per worker.
 * Full context,kind,entry/query keys,paths and identities select a session.
 * Per-kind current input history gates versions across all cached roots.
 * LRU and input budget evict completed sessions;the active session can exceed
 * the input budget alone,so caching never reduces the language/input surface.
 * Input charge includes text capacities,context and known labels/batch;it
 * excludes parse aids,semantic AST/IR/memo,allocator metadata and caller-held
 * results. It is not a process RSS cap. Sharing is charged per cached root.
 * Limits:1..32 sessions and positive input storage budget<=SIZE_MAX/2. */
xls_worker* xlsWorkerCreateWithCache(const xls_session_limits* limits,size_t queueLimit,const xls_worker_cache_limits* cache);
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
/* Original parser/import/stdlib expansion only;no semantic or IR builders.
 * Uses the same actor,full input history,cancellation and shared LRU budget.
 * NULL target selects context.entry and permits zero open overlays. A READY
 * source-error result can have expanded=0;expanded never means program valid. */
xls_query* xlsWorkerDependenciesInContext(xls_worker* worker,xls_context* context,
    const xls_analysis_document* documents,size_t count,const char* target);
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
/* READY transfers an independent dependency reference. Other states clear
 * outputs,using the same bounded error and query lifetime contract above. */
enum xls_query_state xlsQueryDependencyResult(xls_query* query,xls_dependency_capture** result,
    xls_dependency_session_stats* stats,uint64_t* threadId,char* error,size_t capacity);
/* Value snapshot from a successful semantic query;no I/O or actor wait. Cache
 * hits mean routing to a retained session,not necessarily an exact fact hit.
 * Existing semantic stats continue reporting actual worker-wide work totals.
 * Failure/cancelled/syntax-only/absent query clears output and returns0. */
int xlsQuerySessionCache(const xls_query* query,xls_worker_cache_stats* stats);
/* Batched structural selection runs off-owner using the retained parser session.
 * Offsets are copied before submission; bounds and limits follow selection.h.
 * READY transfers a syntax snapshot via xlsQueryResult; selection borrows query. */
xls_query* xlsWorkerSelection(xls_worker* worker,const xls_analysis_document* documents,
    size_t count,const char* target,const size_t* offsets,size_t positions);
const xls_selection* xlsQuerySelection(const xls_query* query);
#endif
