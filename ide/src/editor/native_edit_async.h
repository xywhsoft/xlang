#ifndef XIDE_NATIVE_EDIT_ASYNC_H
#define XIDE_NATIVE_EDIT_ASYNC_H
#include "editor/native_edit.h"
typedef struct xide_native_edit_async xide_native_edit_async;
enum xide_native_async_state {XIDE_NATIVE_IDLE,XIDE_NATIVE_NEW,
 XIDE_NATIVE_PREPARING,XIDE_NATIVE_PREPARED,XIDE_NATIVE_CAPTURED,
 XIDE_NATIVE_STAGING,XIDE_NATIVE_STAGED,XIDE_NATIVE_ARMED,
 XIDE_NATIVE_APPLYING,XIDE_NATIVE_READY,XIDE_NATIVE_PUBLISHED,
 XIDE_NATIVE_FINISHING,XIDE_NATIVE_FINISHED,XIDE_NATIVE_FAILED,
 XIDE_NATIVE_RETIRING,XIDE_NATIVE_RETIRED,XIDE_NATIVE_DROPPING};
typedef struct xide_native_async_status {
 enum xide_native_async_state state;uint64_t token,workerThread,retiredThread;
 int active,cancelled,applyAllowed,restoreAllowed;
 xide_edit_transaction_status transaction;xide_model_error error;
 const xide_edit_candidate* candidate;const char* directory;
} xide_native_async_status;
/* One owner and one persistent XRT worker. Only the owner accesses the ticket;
 * no GUI/store/ticket pointer enters a worker job or its policy callbacks.
 * All normal operations are nonblocking; wake only requests another frame.
 * Metadata submit copies <=1MiB, max255 protected paths and64KiB directory;
 * retain the original candidate, never copy its bodies. Submit starts NO I/O.
 * This dispatcher does not grant consent or acquire a source barrier. Capture,
 * Arm, Publish, Detach and Destroy return XUI status; other int APIs return bool. */
xide_native_edit_async* xideNativeAsyncCreate(void (*wake)(void*),void*);
uint64_t xideNativeAsyncSubmit(xide_native_edit_async*,xide_edit_candidate*,
 const char* directory,const xide_edit_alias_exclusion*,size_t,size_t budget);
/* Owner receipt, not proof: the host must first establish and keep the actual
 * complete reader barrier, physical/protected policy and explicit consent.
 * Revoke apply on stale input. Restore is separately held through cancellation
 * until undo/commit/recovery settles. Worker group fences independently compare
 * the captured package-path environment; restore does not require it current. */
int xideNativeAsyncAuthorize(xide_native_edit_async*,uint64_t,int apply,int restore);
int xideNativeAsyncPrepare(xide_native_edit_async*,uint64_t);
int xideNativeAsyncCapture(xide_native_edit_async*,uint64_t,xide_document_store*);
int xideNativeAsyncStage(xide_native_edit_async*,uint64_t);
int xideNativeAsyncArm(xide_native_edit_async*,uint64_t,int (*guard)(void*),void*);
int xideNativeAsyncApply(xide_native_edit_async*,uint64_t);
int xideNativeAsyncPublish(xide_native_edit_async*,uint64_t,int (*guard)(void*),void*,xide_open_edit_outcome*);
int xideNativeAsyncFinish(xide_native_edit_async*,uint64_t);
/* Detach on owner. A READY work becomes definitely unpublished, allowing worker
 * Finish to restore it. Never releases source pins or performs rollback here. */
int xideNativeAsyncDetach(xide_native_edit_async*,uint64_t);
/* Worker retirement closes resources after a settled outcome; metadata and
 * candidate remain alive for barrier Release. Then Drop frees large snapshots
 * on worker. IN_DOUBT retirement requires finalShutdown=1, never authorizes
 * recovery or resuming source readers. Ordinary callers keep that hold. */
int xideNativeAsyncRetire(xide_native_edit_async*,uint64_t,int finalShutdown);
int xideNativeAsyncDrop(xide_native_edit_async*,uint64_t);
void xideNativeAsyncCancel(xide_native_edit_async*,uint64_t);
/* Copies cached small status only, never touches a busy work. Borrowed metadata
 * ends at Drop/Destroy; NativeWork retirement itself preserves the candidate. */
int xideNativeAsyncStatus(xide_native_edit_async*,xide_native_async_status*);
/* Final shutdown only: cancel, wait for the physical worker step, detach owner
 * ticket, finish definite nonpublication/commit, retire while preserving recovery
 * bytes, release candidate on worker, join. Keep store/context/host graph alive
 * for this call. Does not resume source readers. Refuses owner reentrancy. */
int xideNativeAsyncDestroy(xide_native_edit_async*);
#endif
