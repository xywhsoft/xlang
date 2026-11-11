#ifndef XIDE_WORKSPACE_EDIT_H
#define XIDE_WORKSPACE_EDIT_H
#include "protocol/lsp_wire.h"
#include "analysis/text.h"

typedef struct xide_workspace_edit xide_workspace_edit;
typedef struct xide_workspace_edit_input {
 const char* uri;xls_text* text;uint64_t identity,revision;int hasVersion,version;
} xide_workspace_edit_input;
typedef struct xide_workspace_edit_proof {
 int present,overlay,version;size_t normalizedSize,rawSize;
 unsigned char normalizedSha256[32],rawSha256[32];
} xide_workspace_edit_proof;
typedef struct xide_workspace_edit_document {
 const char *uri,*path;xls_text *before,*after;size_t input,first,count;
 uint64_t identity,revision;int hasVersion,version,changed;
 xide_workspace_edit_proof proof;
} xide_workspace_edit_document;
typedef struct xide_workspace_text_edit {
 size_t document,ordinal,afterStart,afterEnd,annotation;
 xide_lsp_position start,end;
} xide_workspace_text_edit;
typedef struct xide_workspace_edit_annotation {xstrview id,label,description;int needsConfirmation;} xide_workspace_edit_annotation;
typedef struct xide_workspace_edit_info {size_t documents,edits,annotations,changedDocuments,bytes;int needsConfirmation;} xide_workspace_edit_info;
enum xide_workspace_edit_error {
 XIDE_WORKSPACE_EDIT_OK,XIDE_WORKSPACE_EDIT_ARGUMENT,XIDE_WORKSPACE_EDIT_SHAPE,
 XIDE_WORKSPACE_EDIT_UNSUPPORTED,XIDE_WORKSPACE_EDIT_SOURCE,XIDE_WORKSPACE_EDIT_VERSION,
 XIDE_WORKSPACE_EDIT_RANGE,XIDE_WORKSPACE_EDIT_OVERLAP,XIDE_WORKSPACE_EDIT_PROOF,
 XIDE_WORKSPACE_EDIT_LIMIT,XIDE_WORKSPACE_EDIT_MEMORY,XIDE_WORKSPACE_EDIT_CANCELLED
};
/* Worker-only all-or-none PREPARATION, no editor/disk writes. Accept versioned
 * documentChanges (preferred) or changes, and annotated TextEdits. Resource and
 * snippet operations are unsupported; never return a completed prefix.
 * Inputs are immutable request-time LF/no-BOM snapshots, not recaptured reply-
 * time text. Retain their existing xls_sync leases without copying before text.
 * Strict UTF16 positions; all edits address one original snapshot. Stable wire
 * order at equal starts; overlap is an error. Normalize inserted CR/CRLF to LF.
 * Preserve annotation approval requirements and known xlang source-proof fields.
 * requireProof checks normalized body/version claims; closed raw digest remains
 * a claim for the later pinned-file gate. It is NOT verified against a disk here.
 * Max512 inputs/docs,65536 edits,4096 annotations,8MiB/file,64MiB before+after
 * text separately,1MiB input URI bytes. Default128MiB/max256MiB charges known
 * retained snapshot storage (conservatively per doc), arrays/strings and scratch.
 * Allocator, URI conversion, coordinate mapper and xlsTextCreate transient work
 * are additional; this is not process peak/RSS or a cancellation latency bound.
 * No language analysis, membership, physical alias/write permission, toolkit
 * commit/rollback or durable recovery is implied. Keep rename capability gated
 * until the complete application transaction exists. No borrowed DOM survives. */
xide_workspace_edit* xideWorkspaceEditPrepare(const xvalue*,const xide_workspace_edit_input*,size_t,
 int requireProof,size_t budget,int (*cancelled)(void*),void*,enum xide_workspace_edit_error*);
/* Worker-only projection for mixed transactions. Select strictly increasing
 * original document rows. Share existing immutable before/after text leases;
 * do not scan/copy/rebuild their bodies or retain the parent/closed targets.
 * Own copied metadata, remap document/edit/used-annotation indexes and recompute
 * confirmation requirements. Document.input and edit.ordinal keep original
 * request/wire identities. Same default128/max256MiB known-storage policy as
 * Prepare; bounded stack scratch/allocator overhead are additional. No source
 * recapture, approval or mutation. Empty selection is valid. */
xide_workspace_edit* xideWorkspaceEditSelect(const xide_workspace_edit*,const size_t*,size_t,
 size_t budget,int (*cancelled)(void*),void*,enum xide_workspace_edit_error*);
xide_workspace_edit* xideWorkspaceEditRetain(xide_workspace_edit*);
void xideWorkspaceEditRelease(xide_workspace_edit*);
int xideWorkspaceEditInfo(const xide_workspace_edit*,xide_workspace_edit_info*);
int xideWorkspaceEditDocumentAt(const xide_workspace_edit*,size_t,xide_workspace_edit_document*);
int xideWorkspaceEditAt(const xide_workspace_edit*,size_t,xide_workspace_text_edit*);
int xideWorkspaceEditAnnotationAt(const xide_workspace_edit*,size_t,xide_workspace_edit_annotation*);
/* Owner commit fence, no allocation/body scan. Require the SAME immutable text
 * lease plus exact identity/revision/version and captured URI spelling. Checking
 * all rows is necessary but not sufficient for editor/disk atomic application. */
int xideWorkspaceEditStillCurrent(const xide_workspace_edit*,size_t,const xide_workspace_edit_input*);
#endif
