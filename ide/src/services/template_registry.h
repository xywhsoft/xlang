#ifndef XIDE_TEMPLATE_REGISTRY_H
#define XIDE_TEMPLATE_REGISTRY_H
#include "services/project_templates.h"

typedef struct xide_template_definition {
    char *id,*title,*description,*author,*version,*website,*icon,*category,*root;
    char *module,*validate,*create,*language;
    int project,builtin;
    xvalue *fields,*files;
} xide_template_definition;
typedef struct xide_template_registry {
    xide_template_definition* items;
    size_t count;
    char issues[1024];
} xide_template_registry;
int xideTemplateRegistryLoad(xide_template_registry*,const char* userRoot,xide_model_error*);
void xideTemplateRegistryClear(xide_template_registry*);
/* A detached definition can outlive its catalog and is safe to move to a worker. */
int xideTemplateDefinitionCopy(xide_template_definition*,const xide_template_definition*);
void xideTemplateDefinitionClear(xide_template_definition*);
const char* xideTemplateString(const xvalue*,const char*,const char*);
/* Field descriptions are immutable data. Hidden/inactive values are excluded
 * from the validated owned result. Conditions reference stable field IDs. */
int xideTemplateFieldsValidate(const xvalue* descriptions,const xvalue* values,xvalue** result,xide_model_error*);
int xideTemplateFieldActive(const xvalue* description,const char* condition,const xvalue* values);
#endif
