#ifndef XIDE_RENAME_SCOPE_H
#define XIDE_RENAME_SCOPE_H
#include "services/analysis_catalog.h"
#include "services/semantic_rename.h"
#ifdef XLS_COMPILER_XRT
typedef struct xide_rename_scope xide_rename_scope;
#define XIDE_RENAME_SCOPE_BYTES (128u*1024u*1024u)
enum xide_rename_scope_error {
 XIDE_RENAME_SCOPE_OK,XIDE_RENAME_SCOPE_ARGUMENT,XIDE_RENAME_SCOPE_LIMIT,
 XIDE_RENAME_SCOPE_MEMORY,XIDE_RENAME_SCOPE_CANCELLED
};
/* Immutable whole-workspace membership. Reuse the existing catalog's sorted
 * project index; retain explicit standalone xlang file contexts alongside it.
 * At most64 total scopes. A standalone contributes its entry only, never its
 * directory/imports. A selected project does not hide other owners of a shared
 * file. No filesystem/environment/CWD access or manifest/compiler work.
 * All returned bits address ScopeAt order: catalog projects, then standalone
 * files. Caller keeps its configuration/publication epoch with this snapshot.
 * Default/max128MiB charges the full retained catalog, standalone contexts,
 * keys and object, even if another caller shares those allocations. Allocator
 * metadata, callers' other references and transient path conversion are extra.
 * Construction cancellation polls between bounded file contexts. */
xide_rename_scope* xideRenameScopeCreate(xide_analysis_catalog*,xls_context* const* files,
 size_t count,size_t budget,int (*cancelled)(void*),void*,enum xide_rename_scope_error*);
xide_rename_scope* xideRenameScopeRetain(xide_rename_scope*);
void xideRenameScopeRelease(xide_rename_scope*);
size_t xideRenameScopeBytes(const xide_rename_scope*);
size_t xideRenameScopeCount(const xide_rename_scope*);
const xls_context* xideRenameScopeAt(const xide_rename_scope*,size_t);
/* Success with zero bits means unconfigured. Native lookup allocates nothing;
 * path lookup requires an absolute path and prepares a temporary lexical native
 * key with the same existing catalog convention. Neither checks physical file
 * identity or OS write permission. Contexts are exact analysis identities, not
 * a claim that all their files have been checked. */
int xideRenameScopeOwnersNative(const xide_rename_scope*,const wchar_t*,uint64_t*);
int xideRenameScopeOwners(const xide_rename_scope*,const char*,uint64_t*);
/* A source declaration must be in an explicitly configured .xl file. Reject
 * unlisted imports, library-only sources and C/manifest declarations. Missing
 * membership is success with zero bits. This only selects scopes to prove:
 * family closure, access/writability, all references, fresh inputs, conflicts,
 * post-edit binding and a versioned atomic edit transaction remain mandatory.
 * Do not authorize edits or advertise renameProvider from this value alone. */
int xideRenameScopeTarget(const xide_rename_scope*,const xide_rename_target*,uint64_t*);
#endif
#endif
