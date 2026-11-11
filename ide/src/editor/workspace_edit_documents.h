#ifndef XIDE_WORKSPACE_EDIT_DOCUMENTS_H
#define XIDE_WORKSPACE_EDIT_DOCUMENTS_H
#include "editor/documents.h"
#include "protocol/workspace_edit.h"
#if !defined(XUI_CODE_DOCUMENT_BATCH) || XUI_CODE_DOCUMENT_BATCH<2
#error "WorkspaceEdit application requires the qualified XUI document-batch-v2 SDK"
#endif
typedef struct xide_open_edit_ticket xide_open_edit_ticket;
typedef struct xide_open_edit_work xide_open_edit_work;
typedef struct xide_open_edit_outcome {bool committed;size_t readingRestored,readingSkipped;int readingStatus;} xide_open_edit_outcome;
/* UI capture of an ALL-OPEN text plan; fail as a whole for closed/unknown rows.
 * Identity/revision must be the request-time model clock, never recaptured as a
 * new request. Max512 targets,4096 selections,1MiB path/language metadata.
 * Default128/max256MiB combined known storage (plan, baselines, metadata, map,
 * XUI capture/payload); allocator and observer/cache work are additional.
 * Does not scan/copy bodies. No disk read/write, physical alias or host policy.
 * Ticket is owner-only; store storage must outlive it (Clear is allowed).
 * Work owns the immutable leases; KEEP IT ALIVE until ticket destruction and
 * any Stage have finished. Ticket borrows its plan/baseline tokens, not ownership.
 * A worker never receives a ticket/store/widget/core pointer. */
int xideOpenEditCapture(xide_document_store*,xide_workspace_edit*,size_t budget,
 xide_open_edit_ticket**,xide_open_edit_work**);
/* Sequential worker operation, single attempt; cancel/user borrowed for this
 * call only. Even failure leaves Work caller-owned for off-UI retirement. */
int xideOpenEditStage(xide_open_edit_work*,int (*cancelled)(void*),void*);
/* Immutable worker view, borrowed until Work retirement. No GUI access. */
const xide_text_file* xideOpenEditBaselineAt(const xide_open_edit_work*,size_t);
/* Captured known bytes; once Stage is attempted, reserve its complete admitted
 * quota, including opaque SDK staging/payload storage. Not an exact heap size. */
size_t xideOpenEditReservation(const xide_open_edit_work*);
/* Owner-only preflight for a staged, unconsumed ticket; no host callback,
 * publication or disk access. Commit repeats its fences independently. */
int xideOpenEditValidate(xide_open_edit_ticket*,xide_open_edit_work*);
/* Host guard is REQUIRED and borrowed for this synchronous owner call. It must
 * fence connection/request/config/project/source/physical policy and annotation
 * consent. The module rechecks its own whole group after host callbacks, then
 * XUI fences toolkit state again. No implicit approval or protocol activation.
 * ALWAYS inspect outcome.committed, independently of return status. True means
 * all target bodies/models published, even if later cleanup/view work fails.
 * Reading positions use the captured state; movement before publication refuses
 * the plan. Callback changes/closure after publication skip restoration. SDK
 * selection normalization may scan a line/grapheme and still needs performance
 * admission. Flags/preferred-column intent are retained, not recomputed here. */
int xideOpenEditCommit(xide_open_edit_ticket*,xide_open_edit_work*,
 int (*hostGuard)(void*),void*,xide_open_edit_outcome*);
/* Destroy ticket on owner before retiring Work. Accepted-but-unpublished XUI
 * payload cleanup still belongs to that owner and can be heavy; not a latency
 * guarantee. Work retirement is GUI-independent and should run on a worker. */
int xideOpenEditTicketDestroy(xide_open_edit_ticket*);
void xideOpenEditWorkDestroy(xide_open_edit_work*);
#endif
