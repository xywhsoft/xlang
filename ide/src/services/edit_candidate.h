#ifndef XIDE_EDIT_CANDIDATE_H
#define XIDE_EDIT_CANDIDATE_H
#include "services/edit_call.h"
#include "services/rename_evidence.h"
#include "protocol/workspace_edit.h"
typedef struct xide_edit_candidate xide_edit_candidate;
/* Worker-only DATA admission of an actual routed native reply. Bind call and
 * reply, independently import complete provider coverage, reject unsynchronized
 * named models touched by any source/resolver proof, bind every target to local
 * explicit .xl membership, prepare exact open snapshots or bounded closed UTF8
 * decoded text, and require only the requested replacement at one consistent
 * original spelling including the requested cursor. Final fresh evidence sweep
 * follows plan construction. Native semantic checking is the actual RPC's
 * contract; this does not rerun a compiler or grant physical/write authority.
 * Call/reply remain stable during construction. No borrowed reply DOM survives.
 * Default/max256MiB combined known evidence/plan quota; existing per-component
 * bounds and temporary codec/read/DOM/allocator work are additional. All source
 * reads/hashes/body preparation and last release belong on worker/final shutdown. */
xide_edit_candidate* xideEditCandidatePrepare(xide_edit_call*,const xide_edit_reply*,
 size_t budget,int (*cancelled)(void*),void*,xide_model_error*);
void xideEditCandidateRelease(xide_edit_candidate*);
const xide_workspace_edit* xideEditCandidatePlan(const xide_edit_candidate*);
xide_workspace_edit* xideEditCandidateRetainPlan(const xide_edit_candidate*);
const xide_edit_call* xideEditCandidateCall(const xide_edit_candidate*);
/* Immutable/atomic local fence only. Host must still repeat its full Current,
 * physical pin/alias/writability, consent and actual transaction gates. */
int xideEditCandidateCurrent(const xide_edit_candidate*);
int xideEditCandidateValidate(xide_edit_candidate*,int (*cancelled)(void*),void*,xide_model_error*);
#endif
