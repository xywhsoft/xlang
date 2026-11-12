#ifndef XIDE_EDIT_REQUEST_VIEW_H
#define XIDE_EDIT_REQUEST_VIEW_H
#include "app/xide_app.h"
#include "services/edit_request.h"
/* UI owner, one outstanding edit request per primary view. Capture only an
 * acknowledged, exact native sync/catalog and reserve an unused transport ID.
 * Return owns a reference for worker handoff; this does NOT send a request or
 * accept its reply. Caller binds the response to this reserved ID and token.
 * Keep app/view lifetime stable through explicit End and barrier release. */
xide_edit_request* xideLanguageEditRequestCapture(xide_app*,xide_model_error*);
/* Owner-only full local fence: complete model metadata and canonical current
 * analysis catalog/package/compiler. No body scan; bounded config preparation
 * still has allocation/path costs. Call at admission/Arm/Publish, not per key. */
int xideLanguageEditRequestCurrent(xide_app*,xide_edit_request*);
/* Constant-time owner identity and irreversible flag check, no disk/model scan.
 * Does not replace Current or the source/physical/consent/recovery policies. */
int xideLanguageEditRequestOwned(xide_app*,const xide_edit_request*);
/* Exact active request only. Invalidates, unlinks and drops the view reference;
 * caller keeps its reference and releases large immutable assets off-thread. */
int xideLanguageEditRequestEnd(xide_app*,xide_edit_request*);
#endif
