#ifndef XIDE_C_BRIDGE_H
#define XIDE_C_BRIDGE_H
#include "services/language_check.h"
#include "protocol/lsp_wire.h"
typedef struct xide_c_bridge_spec {
 const char* compiler; const xvalue* projects;
 const xide_language_snapshot* documents; size_t count;
} xide_c_bridge_spec;
typedef struct xide_c_bridge_batch xide_c_bridge_batch;
/* Pure copying; projects contain entry/directory/shared/includes/cSources/files.
   Each C database file has one owning bridge. No UI objects cross threads. */
xide_c_bridge_batch* xideCBridgeCopy(const xide_c_bridge_spec* spec,xide_model_error* error);
/* Worker-only query/staging. Replace commands transactionally; real headers
   are byte/hash verified and owned until the C server has stopped. */
int xideCBridgePrepare(xide_c_bridge_batch* batch,xvalue** commands,int (*cancelled)(void*),void* user,xide_model_error* error);
const xvalue* xideCBridgeObservedPaths(xide_c_bridge_batch* batch);
int xideCBridgePrivatePath(xide_c_bridge_batch* batch,const char* path);
/* Worker-only: exact generated-header target in the querying file's project.
   Returns owned local source metadata; the standard server reply is unchanged. */
xvalue* xideCBridgeDefinition(xide_c_bridge_batch* batch,const char* queryPath,const xvalue* reply);
int xideCBridgeGuard(xide_c_bridge_batch* batch,xide_model_error* error);
/* Run after destroying the C server; private files are handle-owned and
   delete-on-close. Only owned, empty directories are removed. */
void xideCBridgeDestroy(xide_c_bridge_batch* batch);
#endif
