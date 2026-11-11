#ifndef XIDE_WORKSPACE_EDIT_FILES_H
#define XIDE_WORKSPACE_EDIT_FILES_H
#include "protocol/workspace_edit.h"
#include "platform/file_rename.h"
typedef struct xide_workspace_edit_files xide_workspace_edit_files;
/* Windows worker-owned physical gate for CLOSED files in a prepared plan.
 * Retains the plan; pins regular, writable, single-link local fixed-drive files
 * and all ordinary ancestors with read sharing only. Never writes source bytes.
 * Requires validated disk proofs, and links their raw hashes to decoded before
 * snapshots using the existing UTF8/BOM/EOL decoder. Open overlays are skipped:
 * this gate alone does not establish cross-overlay physical uniqueness.
 * Max512 closed files,4096 unique ancestors,32768 ancestor candidates,8MiB
 * ancestor path storage and64MiB raw input. Reject network/removable drives,
 * reparse components, alternate streams, devices and noncanonical components.
 * OS handles/locks live until Destroy. No callbacks or GUI objects are retained.
 * This is neither a disk/editor transaction nor permission to apply edits. */
xide_workspace_edit_files* xideWorkspaceEditFilesPin(const xide_workspace_edit*,int (*cancelled)(void*),void*,xide_model_error*);
void xideWorkspaceEditFilesDestroy(xide_workspace_edit_files*);
size_t xideWorkspaceEditFilesCount(const xide_workspace_edit_files*);
/* Borrowed immutable metadata; document is the row in the retained plan. */
int xideWorkspaceEditFilesAt(const xide_workspace_edit_files*,size_t,size_t* document,xide_file_snapshot*);
/* Worker-only final recheck: all pinned handles, current path identities and
 * full raw digests, with bounded reads. Still requires a subsequent UI revision
 * fence, global alias/write policy, annotation consent and real commit/recovery.
 * Non-reentrant; do not validate and destroy concurrently. Existing mapped
 * writers, filesystem filters and drive remapping are outside a user-mode lock
 * guarantee; do not replace this with a metadata-only test. */
int xideWorkspaceEditFilesValidate(xide_workspace_edit_files*,int (*cancelled)(void*),void*,xide_model_error*);
#endif
