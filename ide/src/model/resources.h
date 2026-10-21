#ifndef XLANG_IDE_RESOURCE_MODEL_H
#define XLANG_IDE_RESOURCE_MODEL_H
#include "model/workspace.h"
#include "model/resource_text.h"
/* root retains the SDK DOM where supported, otherwise editable fields only.
   text owns the exact extension spelling for all formats; do not mutate it. */
typedef struct xide_resource_config { xvalue* root; int xson; xide_resource_text text; } xide_resource_config;
enum { XIDE_RESOURCE_NONE, XIDE_RESOURCE_PATH, XIDE_RESOURCE_BASE64, XIDE_RESOURCE_TEXT };
enum { XIDE_RESOURCE_VERSION_FIELDS=9 };
void xideResourceClear(xide_resource_config* config);
int xideResourceParse(xide_resource_config* config,const char* text,xide_model_error* error);
int xideResourceValidate(const xvalue* root,xide_model_error* error);
const char* xideResourceVersion(const xvalue* root,int index);
unsigned int xideResourceNumber(const xvalue* root,const char* key);
int xideResourceSource(const xvalue* root,int manifest,const char** payload);
int xideResourceSetVersion(xvalue* root,int enabled,const char* const fields[9],xide_model_error* error);
int xideResourceSetNumber(xvalue* root,const char* key,const char* decimal,xide_model_error* error);
int xideResourceSetSource(xvalue* root,int manifest,int mode,const char* payload,xide_model_error* error);
/* Preserve extension spans, reparse editable projection; xrtFree result.
   Hand-built configs without original text use the legacy strict DOM roundtrip. */
char* xideResourceSerialize(const xide_resource_config* config,const xvalue* draft,xide_model_error* error);
/* Read-only asset availability/ICO directory check; does not claim XML validity. */
int xideResourceCheckAssets(const xvalue* root,const char* configPath,xide_model_error* error);
#endif
