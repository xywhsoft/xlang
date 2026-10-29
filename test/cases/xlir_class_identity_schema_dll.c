#include "src/runtime/xl6_code_lease.h"
#ifndef CLASS_SCHEMA_TAG
#define CLASS_SCHEMA_TAG 17
#endif
#if defined(_WIN32)
#define SCHEMA_EXPORT __declspec(dllexport)
#else
#define SCHEMA_EXPORT __attribute__((visibility("default")))
#endif
static const char binary_key[]={'b',0,'s'};
static const char storage_key[]={'s',0,'t'},handle_key[]={'h',0,'d'};
static const XL6_ClassIdentityFieldV3 fields[]={
    {binary_key,sizeof(binary_key),UINT64_C(0x21)},
    {"own",3,UINT64_C(0x22)}
};
static const XL6_ClassIdentitySchemaV3 schema={sizeof(schema),UINT64_C(0x434c415353),CLASS_SCHEMA_TAG,
    XL6_CLASS_IDENTITY_DERIVED_V3,XL6_CLASS_IDENTITY_DERIVED_V3,fields,2,
    {storage_key,sizeof(storage_key),handle_key,sizeof(handle_key)}};
SCHEMA_EXPORT const XL6_ClassIdentitySchemaV3* class_identity_schema_get(void) { return &schema; }
