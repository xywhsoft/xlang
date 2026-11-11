#ifndef XIDE_WORKSPACE_EDIT_WRITE_H
#define XIDE_WORKSPACE_EDIT_WRITE_H
#include "platform/workspace_edit_spool.h"
typedef struct xide_edit_file_write xide_edit_file_write;
enum xide_edit_file_phase {XIDE_EDIT_FILE_BEFORE=1,XIDE_EDIT_FILE_AFTER,XIDE_EDIT_FILE_PARTIAL,XIDE_EDIT_FILE_RESTORED,XIDE_EDIT_FILE_IN_DOUBT};
enum xide_edit_file_intent {XIDE_EDIT_FILE_APPLY=1,XIDE_EDIT_FILE_RESTORE};
typedef struct xide_edit_file_status {
 int phase,applyAttempted,restoreAttempted,touched,changed;
 size_t document,knownPrefix,attemptedPrefix;
 xide_file_snapshot before,current;
 int currentVerified;
} xide_edit_file_status;
/* Single-file worker write attempt derived from a FRESH prepared package.
 * Owns verified before/encoded-after XRT buffers, no GUI pointers. Spool and its
 * physical source gate MUST outlive this object and all operations. Max16MiB
 * each body; default128/max256MiB known storage budget for this object/buffers.
 * This is not the combined plan/spool/multi-file budget. No source I/O changes
 * occur during construction. An open overlay or reopened package is refused. */
xide_edit_file_write* xideEditFileWritePrepare(xide_edit_spool*,size_t document,size_t budget,xide_model_error*);
int xideEditFileWriteStatus(const xide_edit_file_write*,xide_edit_file_status*);
/* Worker-only fresh recheck of a verified before/after/restored observation.
 * No write or callback. Never promotes an unknown partial state to permission. */
int xideEditFileWriteVerify(xide_edit_file_write*,xide_model_error*);
/* Own known buffers/wrapper; equal baseline/output share one immutable buffer.
 * Does not include Spool, allocator internals or OS handles. */
size_t xideEditFileWriteStorage(const xide_edit_file_write*);
/* REQUIRED worker-safe host fence. Returning true must establish the enclosing
 * request/source/physical/consent policy AND durably record recoverable intent
 * before any mutation; a generic callback returning true is not a journal.
 * Called between complete local source checks; never receives a GUI pointer
 * from this module. The complete recovery package must remain on disk.
 * One apply attempt only. Cancel is observed before and between write blocks;
 * failure may leave PARTIAL and must be followed by an authorized Restore or
 * durable in-doubt handling. No callback is run during file writes. */
int xideEditFileWriteApply(xide_edit_file_write*,int (*fence)(void*,int),void*,int (*cancelled)(void*),void*,xide_model_error*);
/* One restore attempt. Requires its own durable host intent before writes;
 * accepts exact verified AFTER or a proven prefix produced by this apply.
 * Refuses arbitrary external bytes/identity/metadata changes. Once restoration
 * starts, cancellation is deliberately not honored: finish repair or report
 * IN_DOUBT. A failed restore is never retried implicitly. */
int xideEditFileWriteRestore(xide_edit_file_write*,int (*fence)(void*,int),void*,xide_model_error*);
/* Does not write/restore/retire packages. Readiness is not a multi-file commit
 * or permission to publish GUI text. Keep failures/pending packages for the
 * future actor/recovery coordinator; standard protocol entry remains disabled. */
void xideEditFileWriteDestroy(xide_edit_file_write*);
#endif
