#ifndef XIDE_TEMPLATE_GENERATION_H
#define XIDE_TEMPLATE_GENERATION_H
#include "services/template_registry.h"
typedef struct xide_template_job xide_template_job;
typedef struct xide_template_request {
    const xide_template_definition* definition;
    const xvalue* values;
    const char *name,*location,*workspaceName,*compiler,*scratch;
    int projectDirectory,workspaceMode,workspaceDirectory; /* none=0, new=1, current=2 */
} xide_template_request;
/* Deep-copies inputs; all generation and writes happen off the UI thread. */
xide_template_job* xideTemplateJobStart(const xide_template_request*,xide_model_error*);
int xideTemplateJobDone(xide_template_job*);
void xideTemplateJobCancel(xide_template_job*);
/* Valid after Done. Owned by job. Project may exist when workspace save fails;
 * projectPath remains available so the UI can recover without regenerating. */
const char* xideTemplateJobProject(const xide_template_job*);
const char* xideTemplateJobWorkspace(const xide_template_job*);
const char* xideTemplateJobText(const xide_template_job*);
const char* xideTemplateJobError(const xide_template_job*);
const xvalue* xideTemplateJobFieldErrors(const xide_template_job*);
int xideTemplateJobSucceeded(const xide_template_job*);
void xideTemplateJobDestroy(xide_template_job*);
int xideTemplateNameValid(const char*);
#endif
