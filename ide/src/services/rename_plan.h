#ifndef XIDE_RENAME_PLAN_H
#define XIDE_RENAME_PLAN_H
#include "services/semantic_reference_shard.h"
#ifdef XLS_COMPILER_XRT
typedef struct xide_rename_plan xide_rename_plan;
typedef struct xide_rename_plan_input {xide_semantic_source source;const xvalue* document;} xide_rename_plan_input;
typedef struct xide_rename_plan_change {size_t document;xide_lsp_position start,end;} xide_rename_plan_change;
enum xide_rename_plan_error {XIDE_RENAME_PLAN_OK,XIDE_RENAME_PLAN_ARGUMENT,XIDE_RENAME_PLAN_NAME,XIDE_RENAME_PLAN_UNCHANGED,XIDE_RENAME_PLAN_SOURCE,XIDE_RENAME_PLAN_RANGE,XIDE_RENAME_PLAN_LIMIT,XIDE_RENAME_PLAN_MEMORY,XIDE_RENAME_PLAN_CANCELLED};
typedef struct xide_rename_plan_info {size_t documents,edits,bytes,proofBytes,textBytes;} xide_rename_plan_info;
typedef struct xide_rename_plan_document {
 const char *path,*uri; xstrview before,after; /* exact RAW UTF8, physical BOM retained */
 const xvalue* proof;int hasVersion,version,coordinatePrefix,kind;
 size_t first,count;
} xide_rename_plan_document;
typedef struct xide_rename_plan_edit {
 size_t document;xide_lsp_position start,end; /* original LSP convention */
 size_t afterStart,afterEnd; /* post-BOM byte offsets for native rechecking */
} xide_rename_plan_edit;
/* Pure candidate construction, no filesystem/analysis/edit application.
 * Original native source text is post-BOM, preserving CRLF/CR. Prove its
 * normalized digest and physical raw digest against the supplied document.
 * Keep overlay versions (all int32 values) and null versions for closed files.
 * Verify all UTF16 positions in one existing batch traversal per file, require
 * exact IDENT bytes/boundaries, and reject duplicate/overlapping ranges.
 * Sort docs/edits, copy each before/after once, and deep-clone source proofs.
 * Default/max128MiB admits owned payload, serialized proof charge and scratch;
 * borrowed inputs, DOM/allocator, coordinate mapper/hash internals are extra.
 * No conflicts/writability/family completeness/post-edit binding proof is
 * implied. Do not send/apply the candidate until those gates have succeeded. */
xide_rename_plan* xideRenamePlanCreate(const xide_rename_plan_input*,size_t,
 const xide_rename_plan_change*,size_t,xstrview oldName,xstrview newName,size_t,
 int (*cancelled)(void*),void*,enum xide_rename_plan_error*);
xide_rename_plan* xideRenamePlanRetain(xide_rename_plan*);
void xideRenamePlanRelease(xide_rename_plan*);
int xideRenamePlanInfo(const xide_rename_plan*,xide_rename_plan_info*);
int xideRenamePlanDocumentAt(const xide_rename_plan*,size_t,xide_rename_plan_document*);
int xideRenamePlanEditAt(const xide_rename_plan*,size_t,xide_rename_plan_edit*);
/* Map post-BOM source byte offsets for subsequent native identity comparison.
 * Preserve outside bytes and exact edit endpoints; reject edit interiors and
 * out-of-file offsets. A valid scalar boundary stays valid after mapping. */
int xideRenamePlanMapOffset(const xide_rename_plan*,size_t,size_t,size_t*);
/* Owned standard WorkspaceEdit.documentChanges. Optional negotiated xlang
 * source proof per TextDocumentEdit. 7MiB serialized limit. This is a draft,
 * not conflict/binding approval or a command to write files. */
xvalue* xideRenamePlanWorkspaceEdit(const xide_rename_plan*,int inputProof,
 int (*cancelled)(void*),void*);
#endif
#endif
