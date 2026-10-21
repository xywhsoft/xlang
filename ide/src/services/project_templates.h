#ifndef XLANG_IDE_PROJECT_TEMPLATES_H
#define XLANG_IDE_PROJECT_TEMPLATES_H
#include "model/workspace.h"
typedef struct xide_template_file { const char *path,*kind,*content; } xide_template_file;
typedef struct xide_project_template { const char *id,*title,*description,*kind; const xide_template_file* files; size_t fileCount; } xide_project_template;
typedef struct xide_template_plan xide_template_plan;
size_t xideProjectTemplateCount(void);
const xide_project_template* xideProjectTemplateAt(size_t index);
const xide_project_template* xideProjectTemplateFind(const char* id);
/* Prepare performs no writes. All preview text belongs to the plan. */
xide_template_plan* xideTemplatePlanPrepare(const char* parent,const char* name,const char* templateId,xide_model_error* error);
void xideTemplatePlanDestroy(xide_template_plan* plan);
const char* xideTemplatePlanDirectory(const xide_template_plan* plan);
const char* xideTemplatePlanManifest(const xide_template_plan* plan);
const xide_project* xideTemplatePlanProject(const xide_template_plan* plan);
const xide_project_template* xideTemplatePlanDefinition(const xide_template_plan* plan);
/* Atomically publishes a new directory, then transfers the prepared project to
   the workspace. Existing directories are never merged or overwritten. The
   optional returned project ID uses malloc/free. A committed plan cannot run twice. */
int xideTemplatePlanCommit(xide_workspace* workspace,xide_template_plan* plan,int startup,char** createdId,xide_model_error* error);
#endif
