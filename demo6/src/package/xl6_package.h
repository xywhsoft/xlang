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
    char* target;
    char* manifestPath;
    char* packageDir;
    char* packageName;
    char* packageVersion;
    char* abiVersion;
    char* packageId;
} XL6_PackageResolveResult;

typedef struct XL6_PackageStringList {
    char** items;
    size_t count;
} XL6_PackageStringList;

typedef struct XL6_PackageDependency {
    char* name;
    char* version;
    char* abiVersion;
    char* packageId;
} XL6_PackageDependency;

typedef struct XL6_PackageNativeInfo {
    int singleton;
    char* loadName;
    char* version;
} XL6_PackageNativeInfo;

typedef struct XL6_PackageManifest {
    char* format;
    char* name;
    char* version;
    char* abiVersion;
    char* packageId;
    char* dir;
    XL6_PackageStringList cSources;
    XL6_PackageStringList includeDirs;
    XL6_PackageStringList xlSources;
    XL6_PackageStringList xrtModules;
    int xrtModulesDeclared;
    XL6_PackageStringList links;
    XL6_PackageStringList loadlibs;
    XL6_PackageDependency* dependencies;
    size_t dependencyCount;
    XL6_PackageNativeInfo nativeInfo;
} XL6_PackageManifest;

void xl6PackageStringListInit(XL6_PackageStringList* list);
void xl6PackageStringListFree(XL6_PackageStringList* list);
int xl6PackageStringListAppend(XL6_PackageStringList* list, const char* text);

void xl6PackageManifestInit(XL6_PackageManifest* manifest);
void xl6PackageManifestFree(XL6_PackageManifest* manifest);
int xl6PackageLoadManifest(const char* path, XL6_PackageManifest* manifest);

void xl6PackageResolveResultInit(XL6_PackageResolveResult* result);
void xl6PackageResolveResultFree(XL6_PackageResolveResult* result);
const char* xl6PackageResolveKindName(XL6_PackageResolveKind kind);
int xl6PackageResolveImport(const char* importerPath, const char* target, XL6_PackageResolveResult* result);
int xl6PackageJoinPath(char* out, size_t outSize, const char* a, const char* b);

#endif
