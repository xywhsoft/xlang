#ifndef XIDE_RENAME_PREPARE_H
#define XIDE_RENAME_PREPARE_H
#include "app_xrt.h"
#include "protocol/lsp_wire.h"
/* A bounded transport-thread mailbox, not another source reader or worker.
 * Owner arms/takes/cancels. Receive runs on the existing client worker; no GUI
 * access or source I/O. All matching DOMs, including malformed/late/duplicate
 * replies, are disposed there. Only one immutable string leaf reaches owner.
 * The reserved string-ID prefix is private to this mailbox. */
typedef struct xide_rename_prepare xide_rename_prepare;
typedef struct xide_rename_prepared {
 uint64_t token;int valid;xide_lsp_position start,end;xvalue* placeholder;
 char error[256];
} xide_rename_prepared;
xide_rename_prepare* xideRenamePrepareCreate(void (*wake)(void*),void*);
/* Produces a unique process-lifetime mailbox ID into buffer >= 40 bytes.
 * No DOM or text body scan; refusal does not arm a request. */
uint64_t xideRenamePrepareArm(xide_rename_prepare*,char* id,size_t capacity);
int xideRenamePrepareReceive(xide_rename_prepare*,xvalue* message,size_t wireBytes);
int xideRenamePrepareTake(xide_rename_prepare*,xide_rename_prepared*);
/* Anchor the returned single-line UTF16 range to the unchanged sent caret.
 * Traverse only <=65535 placeholder bytes; no document/whole-line reads.
 * This proves coordinate geometry, not source equality or semantic freshness.
 * Caller must compare the resulting source span and its captured cohort. */
int xideRenamePreparedSpan(const xide_rename_prepared*,xide_lsp_position point,
 int caret,int documentLength,int* start,int* end);
void xideRenamePreparedClear(xide_rename_prepared*);
void xideRenamePrepareCancel(xide_rename_prepare*);
/* Stop the transport before Destroy. No thread/join/large DOM disposal here. */
void xideRenamePrepareDestroy(xide_rename_prepare*);
#endif
