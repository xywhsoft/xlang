#ifndef XIDE_WORKSPACE_EDIT_SPOOL_H
#define XIDE_WORKSPACE_EDIT_SPOOL_H
#include "platform/workspace_edit_files.h"

typedef struct xide_edit_spool xide_edit_spool;
typedef struct xide_edit_spool_baseline {size_t document;const xide_text_file* file;} xide_edit_spool_baseline;
/* Worker-owned immutable PREPARED recovery package for a complete text plan.
 * Supply exactly one borrowed saved-baseline view for each versioned/open row;
 * keep its lease and raw/text buffers immutable for this call. Closed rows use
 * the verified pinned file gate. Preserve every target's LF before/after, saved
 * raw baseline, encoded raw after, identities/versions and annotation requests.
 * This does not change source files, editors or grant consent. Binding is an
 * optional <=64KiB object, cloned as recovery context, never authorization.
 * Directory is a dedicated recovery scope, not the session's checkpoint root.
 * Only one pending package per scope. Reuse the existing recovery lock/paths,
 * state-file publication, codecs, XRT hashing/map/JSON and file snapshots.
 * Max512 docs/2048 unique bodies,16MiB/body,512MiB existing+new stored bodies.
 * Default128/max256MiB reservation includes plan, supplied baseline capacities,
 * own arrays/keys, path bytes and48MiB codec scratch. Generic JSON/map/allocator
 * internals, the independently bounded file gate and OS handles are additional;
 * this is not process peak/RSS. Bodies are streamed/flushed and verified before
 * marker publication. A failed/uncertain marker write PRESERVES all payloads.
 * Successful Prepare retains closed-source pins until Destroy, but releases
 * the temporary plan lease; the package contains all text/metadata it needs. */
xide_edit_spool* xideEditSpoolPrepare(xide_workspace_edit*,const xide_edit_spool_baseline*,size_t,
 const char* directory,const xvalue* binding,size_t memoryBudget,int (*cancelled)(void*),void*,xide_model_error*);
/* Reopen only a prepared package, pin and hash every payload, verify all codec/
 * text links. Never changes source files or recaptures/authorizes their state.
 * Open does not obtain source pins: recovery must establish fresh source/model/
 * context/physical policy and explicit consent before a later apply operation. */
xide_edit_spool* xideEditSpoolOpen(const char*,int (*cancelled)(void*),void*,xide_model_error*);
/* Borrowed immutable JSON (format xlang-ide-edit-spool-1), alive until Destroy.
 * Includes documents, bodies, annotations and unconfirmed binding context. */
const xvalue* xideEditSpoolManifest(const xide_edit_spool*);
/* Caller owns returned XRT buffer; length excludes the extra NUL terminator.
 * Sequential access only. Verify bytes again against the pinned payload. */
unsigned char* xideEditSpoolReadBody(xide_edit_spool*,size_t,size_t*,int (*cancelled)(void*),void*,xide_model_error*);
/* Borrowed gate on a freshly prepared package, NULL on a reopened package.
 * It is not a cross-overlay alias or complete write/transaction policy. */
xide_workspace_edit_files* xideEditSpoolSources(xide_edit_spool*);
/* Always preserve published/uncertain recovery bytes. This only releases
 * memory, source/body/marker handles and the recovery lock. No GUI links. */
void xideEditSpoolDestroy(xide_edit_spool*);
#endif
