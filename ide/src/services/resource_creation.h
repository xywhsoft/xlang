#ifndef XLANG_IDE_RESOURCE_CREATION_H
#define XLANG_IDE_RESOURCE_CREATION_H
#include "model/resources.h"
#include "platform/file_rename.h"
typedef struct xide_resource_plan xide_resource_plan;
/* Resolves a project-relative filename and validates its extension/name. malloc/free. */
char* xideResourceCreationPath(const xide_project* project,const char* path,xide_model_error* error);
/* Read-only preparation; retains exact bytes, project export, manifest and parent identities.
   expectedManifest is optional. A present, saved project manifest is required. */
xide_resource_plan* xideResourcePlanPrepare(const xide_project* project,const char* path,const char* body,const xide_file_snapshot* expectedManifest,xide_model_error* error);
void xideResourcePlanDestroy(xide_resource_plan* plan);
const char* xideResourcePlanPath(const xide_resource_plan* plan);
/* Publishes a flushed ordinary file without replacing any target, then transfers
   preallocated references. Project remains dirty; its manifest is not saved here. */
int xideResourcePlanCommit(xide_project* project,xide_resource_plan* plan,xide_model_error* error);
#endif
