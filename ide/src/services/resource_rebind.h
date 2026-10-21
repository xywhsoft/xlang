#ifndef XLANG_IDE_RESOURCE_REBIND_H
#define XLANG_IDE_RESOURCE_REBIND_H
#include "editor/documents.h"
typedef struct xide_resource_rebind xide_resource_rebind;
/* Captures disk identities and current (possibly dirty) resource text. Only
   effective file-backed icon/manifest fields are changed; extensions stay raw. */
xide_resource_rebind* xideResourceRebindPrepare(const xide_workspace* workspace,xide_document_store* documents,const char* source,xide_model_error* error);
int xideResourceRebindRetarget(xide_resource_rebind* plan,const char* target,xide_model_error* error);
int xideResourceRebindValidate(const xide_resource_rebind* plan,xide_model_error* error);
size_t xideResourceRebindCount(const xide_resource_rebind* plan);
const char* xideResourceRebindPath(const xide_resource_rebind* plan,size_t index);
xide_document* xideResourceRebindDocument(const xide_resource_rebind* plan,size_t index);
int xideResourceRebindOpened(const xide_resource_rebind* plan,size_t index);
/* Immutable transaction payload; valid after Retarget/Open and until Destroy. */
const char* xideResourceRebindBefore(const xide_resource_rebind* plan,size_t index);
const char* xideResourceRebindAfter(const xide_resource_rebind* plan,size_t index);
const xide_file_snapshot* xideResourceRebindDisk(const xide_resource_rebind* plan,size_t index);
int xideResourceRebindApplied(const xide_resource_rebind* plan,size_t index);
/* Opens affected documents before the physical rename. Roll back newly opened
   clean documents on preflight failure. Apply is resumable after physical commit. */
int xideResourceRebindOpen(xide_resource_rebind* plan,xide_model_error* error);
void xideResourceRebindRollbackOpen(xide_resource_rebind* plan);
int xideResourceRebindApply(xide_resource_rebind* plan,xide_model_error* error);
void xideResourceRebindDestroy(xide_resource_rebind* plan);
#endif
