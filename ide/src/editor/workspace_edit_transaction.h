#ifndef XIDE_WORKSPACE_EDIT_TRANSACTION_H
#define XIDE_WORKSPACE_EDIT_TRANSACTION_H
#include "editor/workspace_edit_documents.h"
#include "platform/workspace_edit_journal.h"
#include "platform/workspace_edit_aliases.h"
typedef struct xide_edit_transaction xide_edit_transaction;
typedef struct xide_edit_transaction_ticket xide_edit_transaction_ticket;
enum xide_edit_transaction_phase {XIDE_MIX_PREPARED=1,XIDE_MIX_CAPTURED,XIDE_MIX_STAGED,XIDE_MIX_ARMED,XIDE_MIX_READY,XIDE_MIX_UI_COMMITTED,XIDE_MIX_UI_REJECTED,XIDE_MIX_COMMITTED,XIDE_MIX_ROLLED_BACK,XIDE_MIX_IN_DOUBT,XIDE_MIX_FAILED};
typedef struct xide_edit_transaction_status {int phase,committed,rolledBack,needsRecovery;size_t closed,open,applied,restored,knownBytes;xide_open_edit_outcome editor;} xide_edit_transaction_status;
/* Explicit owner/worker pipeline, no automatic protocol activation. All step,
 * status and destroy calls are sequential and synchronized by their dispatcher;
 * Cancel alone may run concurrently. Work has no GUI/store/ticket pointers.
 * Default256/max512MiB conservative aggregate admission, with component limits
 * preserved; half the remaining quota is the editor Stage partition, charged
 * in full because the SDK payload is opaque. Includes shared-lease charges,
 * Spool's codec reservation,
 *16MiB file-gate,16MiB alias gate and1MiB journal reserves. Allocator/cache/OS overhead additional.
 * Worker projection shares bodies; dedicated recovery directory is never reused. */
xide_edit_transaction* xideEditTransactionPrepare(xide_workspace_edit*,const char* directory,size_t budget,int (*cancelled)(void*),void*,xide_model_error*);
/* Worker, PREPARED/unattached only: copy additional protected file/root paths.
 * Dedicated recovery root is always protected. Max256 including that root,
 *1MiB copied path bytes per call; no source I/O. Failure adds none. These paths
 * do not replace complete host source/request/configuration/consent policy. */
int xideEditTransactionProtect(xide_edit_transaction*,const xide_edit_alias_exclusion*,size_t,xide_model_error*);
/* Owner capture/preflight; no source I/O. Capture all named open model IDs/paths (4096/1MiB),
 * recheck the complete roster at Arm/Publish. Store storage outlives the ticket,
 * even if Clear/context destruction cancels the pending work. */
int xideEditTransactionCapture(xide_document_store*,xide_edit_transaction*,xide_edit_transaction_ticket**);
int xideEditTransactionStage(xide_edit_transaction*,int (*cancelled)(void*),void*,xide_model_error*);
int xideEditTransactionArm(xide_edit_transaction_ticket*,int (*hostUiGuard)(void*),void*);
/* Required detached worker policy; APPLY must verify complete source/physical/
 * request/project/config/consent and cached-reader quiescence. RESTORE authorizes
 * undo of this operation even after cancellation; do not require the old request
 * to remain current. SIZE_MAX asks for group admission. Byte locks additionally
 * stop ordinary reads through other handles, not mapped/cached readers. */
int xideEditTransactionApply(xide_edit_transaction*,int (*workerGuard)(void*,size_t,int),void*,int (*cancelled)(void*),void*,xide_model_error*);
/* Owner publication; explicit outcome.committed survives later errors/closure.
 * Publishing intent was persisted by Apply on the worker. A failed UI fence
 * preserves user edits and requires Finish to restore the closed members. */
int xideEditTransactionPublish(xide_edit_transaction_ticket*,int (*hostUiGuard)(void*),void*,xide_open_edit_outcome*);
/* Worker: COMMITTED never rolls back, even if cancelled or journal completion
 * fails. A definite non-publication rolls back under durable restore intents.
 * No source mutation/replay is hidden in destruction. */
int xideEditTransactionFinish(xide_edit_transaction*,int (*workerGuard)(void*,size_t,int),void*,xide_model_error*);
void xideEditTransactionCancel(xide_edit_transaction*);
int xideEditTransactionStatus(const xide_edit_transaction*,xide_edit_transaction_status*);
int xideEditTransactionTicketDestroy(xide_edit_transaction_ticket*);
/* Ticket must be gone, all worker calls complete, and any applied sources settled
 * or explicitly reported IN_DOUBT. IN_DOUBT data remains in the recovery scope;
 * caller must coordinate its recovery/readers before resuming affected work. */
int xideEditTransactionDestroy(xide_edit_transaction*);
#endif
