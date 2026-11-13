#ifndef XIDE_EDIT_CALL_H
#define XIDE_EDIT_CALL_H
#include "services/edit_request.h"
#include "services/language_client.h"
#define XIDE_RENAME_CANDIDATE_METHOD "xlang/renameCandidate"
typedef struct xide_edit_call xide_edit_call;
typedef struct xide_edit_reply xide_edit_reply;
typedef struct xide_edit_call_intent {const char* uri;xstrview newName;xide_lsp_position position;int version;uint64_t identity,revision;} xide_edit_call_intent;
enum xide_edit_call_state {XIDE_EDIT_CALL_PREPARED,XIDE_EDIT_CALL_QUEUED,XIDE_EDIT_CALL_REPLIED,XIDE_EDIT_CALL_CANCELLED};
enum xide_edit_call_error {XIDE_EDIT_CALL_OK,XIDE_EDIT_CALL_ARGUMENT,XIDE_EDIT_CALL_STALE,XIDE_EDIT_CALL_MEMORY,XIDE_EDIT_CALL_SHAPE,XIDE_EDIT_CALL_PEER,XIDE_EDIT_CALL_REPLAY};
/* Worker-only immutable intent/packet preparation. connection is independently
 * copied from the actual owner-held transport, never a wire claim. Bind the
 * reserved ID/token, exact synced URI/version/model and UTF16 position; collect
 * complete local scopes and encode one metadata request on the existing batch.
 * Name is bounded nonempty UTF8 intent (max4096 bytes), not identifier approval.
 * URI max262144 bytes; no document body copy/hash, compiler or source I/O.
 * A request can produce only one successful call, even after call destruction.
 * Existing8MiB frame and scope/context limits apply; shared snapshots, temporary
 * metadata/context/codec work and allocator overhead are additional. */
xide_edit_call* xideEditCallCreate(xide_edit_request*,uint64_t connection,const char* uri,
 int line,int character,xstrview newName,int (*cancelled)(void*),void*,enum xide_edit_call_error*);
xide_edit_call* xideEditCallRetain(xide_edit_call*);
/* Last release can free a large packet/response and retained text/catalog:
 * worker or final shutdown only. */
void xideEditCallRelease(xide_edit_call*);
const xide_edit_request* xideEditCallRequest(const xide_edit_call*);
const xide_rename_scope* xideEditCallScope(const xide_edit_call*);
uint64_t xideEditCallConnection(const xide_edit_call*);
int xideEditCallIntent(const xide_edit_call*,xide_edit_call_intent*);
enum xide_edit_call_state xideEditCallState(const xide_edit_call*);
/* One sending owner, after the host's full Current fence. Success atomically
 * admits its private encoded packet to the SAME transport exactly once. Failure
 * keeps the packet; a fresh Current fence is required before any retry. */
int xideEditCallSend(xide_edit_call*,xide_language_client*);
/* Thread-safe local irreversible invalidation, no protocol cancellation send. */
void xideEditCallCancel(xide_edit_call*);
/* Worker-only ROUTING admission. peer and bytes come from the actual transport
 * and original parser Content-Length. Require strict2.0 integer-ID response,
 * queued call, current local token, exact result method/token/input receipt and
 * scopeCount, or a standard error. Success TAKES one owned root reference;
 * failure takes nothing. Only one reply can claim the call. No JSON re-encoding
 * or deep clone. This DOES NOT decode/validate guards, WorkspaceEdit, source
 * coverage, static analysis, current GUI/disk, consent or write permission.
 * Candidate() remains UNTRUSTED until those separate worker/owner gates pass. */
xide_edit_reply* xideEditCallReply(xide_edit_call*,uint64_t peer,xvalue* root,size_t bytes,enum xide_edit_call_error*);
/* Local irreversible flags only. Still run the owner's complete Current fence. */
int xideEditReplyCurrent(const xide_edit_reply*);
int xideEditReplyError(const xide_edit_reply*,int* code,xstrview* message);
const xvalue* xideEditReplyCandidate(const xide_edit_reply*);
/* Borrow the original bound call while reply remains alive; no new authority. */
const xide_edit_call* xideEditReplyCall(const xide_edit_reply*);
void xideEditReplyRelease(xide_edit_reply*);
#endif
