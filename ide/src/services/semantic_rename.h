#ifndef XIDE_SEMANTIC_RENAME_H
#define XIDE_SEMANTIC_RENAME_H
#include "services/semantic_reference_shard.h"
#ifdef XLS_COMPILER_XRT
typedef struct xide_rename_target xide_rename_target;
enum xide_rename_target_error {
 XIDE_RENAME_TARGET_OK, XIDE_RENAME_TARGET_ABSENT, XIDE_RENAME_TARGET_ARGUMENT,
 XIDE_RENAME_TARGET_CAPTURE, XIDE_RENAME_TARGET_COORDINATE,
 XIDE_RENAME_TARGET_LIMIT, XIDE_RENAME_TARGET_MEMORY, XIDE_RENAME_TARGET_CANCELLED
};
typedef struct xide_rename_target_info {
 const char* path; /* independently owned query path */
 xide_lsp_position start,end; /* UTF16, using the caller's verified BOM convention */
 xstrview name; /* exact declaration name, not a canonical/type spelling */
 xide_semantic_source declaration;
 size_t declarationStart,declarationEnd,bytes;
} xide_rename_target_info;
/* Worker-only semantic candidate, using the original checked selection and
 * reference identity. Refine qualified occurrence spans to the exact native
 * IDENT at the cursor, and require its bytes to equal the declaration name.
 * No parsing/rechecking, disk access, spelling-based symbol lookup or AST loan.
 * The returned immutable value outlives check/cache/source-vector destruction.
 * Default16MiB/max64MiB counts its owned target/key/path only, not borrowed
 * navigation or transient construction. Cancellation polls at entry/exit and
 * bounded scans; existing navigation construction is not preempted here.
 * This is NOT writable membership, family/override closure, freshness, name
 * conflict or post-rename binding proof. It must not authorize edits by itself.
 * No standard rename capability is enabled by this foundation. */
xide_rename_target* xideRenameTargetCreate(xls_projection_store*,xls_semantic_check*,
 const xide_semantic_source*,size_t count,const char* path,int line,int character,
 size_t budget,int (*cancelled)(void*),void* user,enum xide_rename_target_error*);
xide_rename_target* xideRenameTargetRetain(xide_rename_target*);
void xideRenameTargetRelease(xide_rename_target*);
int xideRenameTargetInfo(const xide_rename_target*,xide_rename_target_info*);
/* Independent reference to the exact source/span identity, for the existing
 * cross-owner reference-shard query. Caller releases it through the key API. */
xide_reference_key* xideRenameTargetRetainKey(const xide_rename_target*);
#endif
#endif
