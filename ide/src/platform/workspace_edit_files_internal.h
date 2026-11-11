#ifndef XIDE_WORKSPACE_EDIT_FILES_INTERNAL_H
#define XIDE_WORKSPACE_EDIT_FILES_INTERNAL_H
#include "platform/workspace_edit_files.h"
/* Private worker adapter for the disk writer. Borrow only until gate destruction;
 * no ownership transfer or authority to write. Recheck the selected target's
 * pinned ancestors and source path/identity, single-link/writable state, without requiring original
 * lengths/hashes (a previous transaction member may already have changed).
 * The writer separately proves its own complete expected snapshot before I/O.
 * Never expose this handle to UI/protocol callers. Sequential gate use only. */
int xideWorkspaceEditFilesBorrowWriteHandle(xide_workspace_edit_files*,size_t,void**,xide_model_error*);
/* Explicit transaction-only byte-range isolation; normal reads through other
 * handles fail while held, including same-process reads. Nonblocking whole-group
 * acquisition unwinds prefixes. Mapped I/O, source policy and application-level
 * cached/prepared readers still require the host's separate fence. Sequential
 * use only; the owning gate outlives all leases and closes any remaining locks. */
int xideWorkspaceEditFilesLockReaders(xide_workspace_edit_files*,xide_model_error*);
int xideWorkspaceEditFilesUnlockReaders(xide_workspace_edit_files*,xide_model_error*);
#endif
