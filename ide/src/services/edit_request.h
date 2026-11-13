#ifndef XIDE_EDIT_REQUEST_H
#define XIDE_EDIT_REQUEST_H
#include "protocol/document_sync.h"
#include "model/workspace.h"
#include "services/rename_scope.h"

typedef struct xide_edit_request xide_edit_request;
typedef struct xide_edit_request_model {
 uint64_t identity,revision;int open,dirty,external;
 const char *path,*language;
} xide_edit_request_model;
enum xide_edit_request_reason {XIDE_EDIT_REQUEST_DOCUMENT=1,XIDE_EDIT_REQUEST_DISK=2,
 XIDE_EDIT_REQUEST_CONFIGURATION=4,XIDE_EDIT_REQUEST_CONNECTION=8,
 XIDE_EDIT_REQUEST_CANCELLED=16,XIDE_EDIT_REQUEST_ENDED=32};
/* Owner-only capture. The callback supplies a stable complete Document roster,
 * at most4096 slots and1MiB known model metadata/strings. One XRT allocation
 * owns the roster and complete workspace file paths (up to65536 slots,2MiB
 * path metadata,262144 bytes per UTF-8 path). NULL files means an empty
 * workspace. The caller keeps both inputs stable throughout capture. Clone
 * existing sync entries and retain the producer's
 * immutable catalog, without copying/scanning bodies or retaining GUI pointers.
 * token and reserved positive request ID belong to one live primary view.
 * No transport send, reply acceptance, source closure or write authority. */
xide_edit_request* xideEditRequestCreate(uint64_t token,int request,const xls_sync*,xvalue* catalog,
 const xide_files*,size_t count,int (*modelAt)(void*,size_t,xide_edit_request_model*),void*,xide_model_error*);
xide_edit_request* xideEditRequestRetain(xide_edit_request*);
/* Potential last text/catalog disposal: worker or final shutdown only. */
void xideEditRequestRelease(xide_edit_request*);
/* Thread-safe irreversible invalidation. Existing reasons are never cleared. */
void xideEditRequestInvalidate(xide_edit_request*,unsigned reasons);
unsigned xideEditRequestInvalidated(const xide_edit_request*);
uint64_t xideEditRequestToken(const xide_edit_request*);
int xideEditRequestId(const xide_edit_request*);
size_t xideEditRequestModelCount(const xide_edit_request*);
/* Copies immutable metadata with borrowed strings; zeros output on failure. */
int xideEditRequestModelAt(const xide_edit_request*,size_t,xide_edit_request_model*);
int xideEditRequestModelMatches(const xide_edit_request*,size_t,const xide_edit_request_model*);
size_t xideEditRequestWorkspaceCount(const xide_edit_request*);
const char* xideEditRequestWorkspaceAt(const xide_edit_request*,size_t);
/* Owner boundary only, ordered exact spelling, no allocations or file I/O.
 * A workspace list mutation invalidates its request even without a catalog
 * change. Display IDs/kinds are not analysis identities. */
int xideEditRequestWorkspaceMatches(const xide_edit_request*,const xide_files*);
/* Worker-only metadata preparation from the independently captured request.
 * Reuse the common catalog and full rename scope. Include unopened workspace
 * .xl roots and named open xlang models; explicitly project-owned paths stay
 * project roles, never acquire an implicit standalone role. Deduplicate lexical
 * aliases among standalone roots. At most64 total roles; fail the whole result
 * on missing capacity, invalid metadata, cancellation or request invalidation.
 * No source/manifest/environment/alias I/O or compiler. Retained scope budget
 * excludes transient keys/contexts/catalog construction. No coverage, request
 * transport freshness or write authority is granted. Caller retains request. */
xide_rename_scope* xideEditRequestScope(const xide_edit_request*,size_t budget,
 int (*cancelled)(void*),void*,enum xide_rename_scope_error*);
/* Immutable borrowed snapshots, safe concurrent reads while retained. The
 * original connection/sync/view may already be gone. Never mutate the catalog
 * or cast away const on this private sync. Text leases retain their ownership. */
const xls_sync* xideEditRequestSync(const xide_edit_request*);
const xvalue* xideEditRequestCatalog(const xide_edit_request*);
size_t xideEditRequestMetadata(const xide_edit_request*);
#endif
