#ifndef XIDE_WORKSPACE_FOLDERS_H
#define XIDE_WORKSPACE_FOLDERS_H
#include "protocol/lsp_wire.h"
typedef struct xide_workspace_folders xide_workspace_folders;
#define XIDE_WORKSPACE_FOLDERS_LIMIT 64u
#define XIDE_WORKSPACE_FOLDERS_BYTES (2u*1024u*1024u)
/* Immutable, retained root metadata. No filesystem/manifest access or cwd
 * changes. Prefer a present workspaceFolders (including null/[]), then a
 * present rootUri (including null), then rootPath. Absolute file roots only.
 * At most64 roots/2MiB known retained path/key/label bytes plus DOM overhead. */
xide_workspace_folders* xideWorkspaceFoldersRead(const xvalue* initializeParams,xide_model_error*);
/* Validate both arrays completely before applying removals then additions.
 * Failure leaves current unchanged and sets changed=0. Identity is the native
 * lexical path set, not array order, display labels or URI escaping. Metadata
 * may change with changed=0. Unknown removals/known additions are idempotent;
 * duplicate paths within one supplied array are rejected. */
xide_workspace_folders* xideWorkspaceFoldersChange(const xide_workspace_folders* current,
 const xvalue* params,int* changed,xide_model_error*);
xide_workspace_folders* xideWorkspaceFoldersRetain(xide_workspace_folders*);
void xideWorkspaceFoldersRelease(xide_workspace_folders*);
size_t xideWorkspaceFoldersCount(const xide_workspace_folders*);
int xideWorkspaceFoldersAt(const xide_workspace_folders*,size_t,const char** uri,const char** name,const char** path);
#endif
