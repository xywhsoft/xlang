#ifndef XL6_PACKAGE_H
#define XL6_PACKAGE_H

#include <stddef.h>

typedef enum XL6_PackageResolveKind {
    XL6_PACKAGE_RESOLVE_UNRESOLVED,
    XL6_PACKAGE_RESOLVE_FILE,
    XL6_PACKAGE_RESOLVE_PACKAGE
} XL6_PackageResolveKind;

typedef struct XL6_PackageResolveResult {
    XL6_PackageResolveKind kind;
    int resolved;
    char target[256];
    char manifestPath[512];
} XL6_PackageResolveResult;

void xl6PackageResolveResultInit(XL6_PackageResolveResult* result);
const char* xl6PackageResolveKindName(XL6_PackageResolveKind kind);
int xl6PackageResolveImport(const char* importerPath, const char* target, XL6_PackageResolveResult* result);
int xl6PackageResolveInterfacePath(const XL6_PackageResolveResult* result, char* out, size_t outSize);

#endif
