#ifndef XIDE_DIAGNOSTIC_STREAM_H
#define XIDE_DIAGNOSTIC_STREAM_H
#include "services/diagnostic_notice.h"
#include "protocol/document_sync.h"

#define XIDE_DIAGNOSTIC_BATCH_MESSAGES 128u
#define XIDE_DIAGNOSTIC_BATCH_BYTES (32u*1024u*1024u)
typedef struct xide_diagnostic_batch xide_diagnostic_batch;
typedef struct xide_diagnostic_stream xide_diagnostic_stream;
typedef struct xide_diagnostic_result {
 uint64_t session,serial,sourceEpoch,workerThreadId;
 int valid,hasSync;char syncDigest[65];size_t syncDocuments;
 size_t messages,wireBytes,count,unmappedOverlays;
 xide_dependency_stats registry;char error[256];
 xide_diagnostic_notice* notices[XIDE_DIAGNOSTIC_BATCH_MESSAGES];
} xide_diagnostic_result;
typedef struct xide_diagnostic_stream_stats {
 size_t queued,running,ready,outstanding,retired;
 uint64_t submitted,finished,cancelled,discarded,rejected;
} xide_diagnostic_stream_stats;

xide_diagnostic_batch* xideDiagnosticBatchCreate(void);
/* Success TAKES an exclusively owned immutable parsed root. wireBytes must be
 * its original Content-Length from the transport, not a JSON-size estimate.
 * At most128 roots/32MiB, no encoding, scanning or cloning on this call. */
int xideDiagnosticBatchAppend(xide_diagnostic_batch* batch,xvalue* message,size_t wireBytes);
size_t xideDiagnosticBatchCount(const xide_diagnostic_batch* batch);
size_t xideDiagnosticBatchBytes(const xide_diagnostic_batch* batch);
/* Heavy cleanup: worker, tests or final shutdown, not an interactive failure. */
void xideDiagnosticBatchRelease(xide_diagnostic_batch* batch);

/* One XRT worker owns the connection's registry and FIFO decode/map stream.
 * Two internal jobs/results total plus one retirement slot; no per-file worker.
 * All control calls use one owner. Wake only notifies the owner, never touches
 * UI, and outlives Destroy. The retirement slot queues one object; the worker
 * may simultaneously be releasing a previously dequeued object.
 * Returned notices/results/leases are caller-owned and outside these logical
 * queue budgets; this is not a process heap/RSS or latency guarantee. */
xide_diagnostic_stream* xideDiagnosticStreamCreate(void (*wake)(void*),void* user);
/* Connection/configuration reset ONLY, not every local edit. New nonzero
 * sessions increase strictly; repeating the current session is a no-op.
 * Invalidates old pending/ready work without join, JSON release or disk I/O. */
int xideDiagnosticStreamReset(xide_diagnostic_stream* stream,uint64_t session);
/* 1 accepted: TAKES batch. 0 internal capacity busy: takes nothing; retry.
 * -1 invalid/allocation/poisoned sync: takes nothing. Sync is owner-cloned by
 * retaining immutable metadata/text, without body scan/copy. Source epoch is
 * the EXACT sent-input snapshot represented by sync, not the latest edit hint.
 * serial is cleared on failure. Within one session, accepted batches are FIFO. */
int xideDiagnosticStreamSubmit(xide_diagnostic_stream* stream,uint64_t session,uint64_t sourceEpoch,
 xide_diagnostic_batch* batch,const xls_sync* sync,uint64_t* serial);
/* Negotiated0.27 IDE mode. The worker requires a canonical client-input serial
 * on every notice, bounded by the exact admitted-input ceiling, plus complete
 * overlay proof metadata on non-cleared notices. Failure rejects the ENTIRE
 * batch before any result can be installed, including when UI adoption slices.
 * Ownership and backpressure match Submit; the older0.26 API stays usable. */
int xideDiagnosticStreamSubmitCausal(xide_diagnostic_stream* stream,uint64_t session,uint64_t sourceEpoch,
 uint64_t inputCeiling,xide_diagnostic_batch* batch,const xls_sync* sync,uint64_t* serial);
xide_diagnostic_result* xideDiagnosticStreamTake(xide_diagnostic_stream* stream);
/* A valid result means structurally accepted wire data, NOT semantic freshness.
 * Matching overlay source leases are verified/mapped on the worker. Unmatched
 * versions/bodies keep unmapped notices for host stale-input handling. Negotiated
 * disk notices reuse the snapshot proof reader and normalized mapper here;
 * explicit jumps still recheck disk identity/body at the action boundary.
 * Host also checks causal input fences, local identities/revisions, dependency
 * changes, session and configuration. */
xide_diagnostic_notice* xideDiagnosticResultTakeNotice(xide_diagnostic_result* result,size_t index);
void xideDiagnosticResultRelease(xide_diagnostic_result* result);
/* Success TAKES one object to its non-UI release callback on the same worker.
 * False takes nothing. Release callbacks must not destroy/reenter the stream. */
int xideDiagnosticStreamRetire(xide_diagnostic_stream* stream,void* value,void (*release)(void*));
int xideDiagnosticStreamLoad(xide_diagnostic_stream* stream,xide_diagnostic_stream_stats* stats);
/* Final shutdown only: cancels/joins and frees internally owned jobs/results.
 * Already taken results/notices outlive the stream until caller release. */
void xideDiagnosticStreamDestroy(xide_diagnostic_stream* stream);
#endif
