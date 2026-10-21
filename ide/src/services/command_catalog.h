#ifndef XLANG_IDE_COMMAND_CATALOG_H
#define XLANG_IDE_COMMAND_CATALOG_H
#include "model/workspace.h"
typedef struct xide_command_parameter { char *name,*type,*defaultValue,*description; int optional; } xide_command_parameter;
typedef struct xide_help_document { char *id,*title,*markdown,*source; int sourceLine; } xide_help_document;
typedef struct xide_command {
    char *id,*name,*language,*category,*module,*kind,*signature,*description,*returns,*example,*insert,*version,*requiredImport,*source;
    int sourceLine; xide_command_parameter* parameters; size_t parameterCount;
    const xide_help_document** documents; size_t documentCount;
} xide_command;
typedef struct xide_command_catalog { char* path; xide_command* entries; size_t count; xvalue* source; xide_help_document* documents; size_t documentCount; char sha256[65]; } xide_command_catalog;
const xide_help_document* xideCatalogDocument(const xide_command_catalog* catalog,const char* id);
void xideCatalogClear(xide_command_catalog* catalog);
int xideCatalogLoad(xide_command_catalog* catalog,const char* path,xide_model_error* error);
const xide_command* xideCatalogFind(const xide_command_catalog* catalog,const char* id);
int xideCatalogMatches(const xide_command* command,const char* query);
int xideCatalogCanComplete(const xide_command* command,const char* language);
#endif
