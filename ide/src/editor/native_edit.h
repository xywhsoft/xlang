#ifndef XIDE_NATIVE_EDIT_H
#define XIDE_NATIVE_EDIT_H
#include "services/edit_candidate.h"
#include "editor/workspace_edit_transaction.h"
typedef struct xide_native_edit xide_native_edit;
typedef struct xide_native_edit_ticket xide_native_edit_ticket;
/* Worker-only native candidate transaction preparation. Retain the actual
 * admitted candidate, pin every unedited disk dependency with XRT read-only
 * sharing, then validate its original complete guard set. Reuse the existing
 * mixed transaction for target pins, codec/spool, journal, publication/rollback.
 * Host supplies the complete protected file/root set and dedicated recovery
 * directory. Default256/max512MiB combined KNOWN candidate/policy/transaction
 * admission;16MiB policy reserve. Shared request snapshots, XRT/OS handles and
 * allocator internals additional. No automatic UI/protocol activation. */
xide_native_edit* xideNativeEditPrepare(xide_edit_candidate*,const char* directory,
 const xide_edit_alias_exclusion*,size_t,size_t budget,int (*cancelled)(void*),void*,xide_model_error*);
/* Owner capture must match the COMPLETE original request model roster, even
 * retained/hidden named models. Store/ticket storage/lifetimes follow the mixed
 * transaction contract. No source reads or writes. */
int xideNativeEditCapture(xide_document_store*,xide_native_edit*,xide_native_edit_ticket**);
int xideNativeEditStage(xide_native_edit*,int (*cancelled)(void*),void*,xide_model_error*);
/* Required host UI guard proves original connection/workspace/configuration,
 * full source-reader barrier, consent and physical policy outside this module.
 * Native wrapper also fences the original complete model roster before/after. */
int xideNativeEditArm(xide_native_edit_ticket*,int (*hostUiGuard)(void*),void*);
/* Required detached host fence: group SIZE_MAX checks all remaining host
 * policy; per-file fences may check its atomic admission/consent. This wrapper
 * freshly validates pinned metadata, original resolver/environment and all
 * immutable reports at group boundaries without rehashing each dependency per
 * target. Closed writer data is verified independently by the existing writer.
 * RESTORE ignores old candidate/request cancellation and reuses owned pins;
 * host must still authorize rollback/readers and durable recovery policy. */
int xideNativeEditApply(xide_native_edit*,int (*workerGuard)(void*,size_t,int),void*,int (*cancelled)(void*),void*,xide_model_error*);
int xideNativeEditPublish(xide_native_edit_ticket*,int (*hostUiGuard)(void*),void*,xide_open_edit_outcome*);
int xideNativeEditFinish(xide_native_edit*,int (*workerGuard)(void*,size_t,int),void*,xide_model_error*);
void xideNativeEditCancel(xide_native_edit*);
int xideNativeEditStatus(const xide_native_edit*,xide_edit_transaction_status*);
int xideNativeEditTicketDestroy(xide_native_edit_ticket*);
int xideNativeEditDestroy(xide_native_edit*);
#endif
