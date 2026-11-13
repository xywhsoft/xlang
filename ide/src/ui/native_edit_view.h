#ifndef XIDE_NATIVE_EDIT_VIEW_H
#define XIDE_NATIVE_EDIT_VIEW_H
#include "app/xide_app.h"
#include "editor/workspace_edit_transaction.h"
enum xide_native_apply_phase {XIDE_APPLY_IDLE,XIDE_APPLY_CONFIRM,XIDE_APPLY_HOLDING,
 XIDE_APPLY_PREPARING,XIDE_APPLY_STAGING,XIDE_APPLY_WRITING,XIDE_APPLY_FINISHING,
 XIDE_APPLY_RETIRING,XIDE_APPLY_RESUMING,XIDE_APPLY_DONE,XIDE_APPLY_CANCELLED,
 XIDE_APPLY_FAILED,XIDE_APPLY_RECOVERY};
typedef struct xide_native_apply_status {uint64_t token;int phase,active,barrierHeld;
 xide_edit_transaction_status transaction;xide_model_error error;const char* directory;
} xide_native_apply_status;
/* Internal owner integration, not a menu or protocol handler. Uses only this
 * view's exact negotiated/acknowledged admitted candidate. Caller supplies a
 * dedicated fresh recovery scope and COMPLETE protected paths; the production
 * policy builder and consent/recovery UI must precede public activation.
 * Begin copies bounded metadata and enters CONFIRM, with NO source I/O, reader
 * holds or writes. Plan is borrowed preview data, never write permission. */
uint64_t xideLanguageEditApplyBegin(xide_app*,const char* directory,
 const xide_edit_alias_exclusion*,size_t,size_t budget,xide_model_error*);
const xide_workspace_edit* xideLanguageEditApplyPlan(xide_app*,uint64_t);
/* Explicit consent for this exact token/candidate. Begin the actual six-reader
 * barrier; ordinary language Poll performs all asynchronous worker handoffs,
 * repeats full owner Current at Capture/Arm/Publish, invalidates task inputs,
 * closes source handles before resume and retires large snapshots off owner.
 * Cancellation never auto-resumes unverified/IN_DOUBT writes. */
int xideLanguageEditApplyConfirm(xide_app*,uint64_t,xide_model_error*);
int xideLanguageEditApplyCancel(xide_app*,uint64_t);
int xideLanguageEditApplyStatus(xide_app*,xide_native_apply_status*);
/* Called before stopping/destroying ANY participant, documents or context.
 * Final shutdown only; may wait for owned write/undo completion. Never replays
 * an uncertain journal or resumes readers. Also used by LanguageViewDestroy
 * when a test creates/destroys that view directly. */
int xideLanguageEditApplyShutdown(xide_app*);
#endif
