#ifndef XL6_PACKAGE_H
#define XL6_PACKAGE_H

#include <stddef.h>

struct XL6_SourceReport;

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
    struct XL6_SourceReport* sourceReport; /* Borrowed during resolution. */
} XL6_PackageResolveResult;

typedef struct XL6_PackageStringList {
    char** items;
    size_t count;
} XL6_PackageStringList;

/* Enable-only requirements, combined across imported packages. */
enum {
    XL6_NATIVE_LINK_FUNCTION_SECTIONS = 1u << 0,
    XL6_NATIVE_LINK_DATA_SECTIONS = 1u << 1,
    XL6_NATIVE_LINK_GC_SECTIONS = 1u << 2,
    XL6_NATIVE_LINK_PRINT_GC_SECTIONS = 1u << 3
};

/* Cold, opt-in source dependency. Legacy cSources remain unconditional. */
typedef struct XL6_PackageCSourceGroup {
    char* source;
    XL6_PackageStringList providedSymbols;
    XL6_PackageStringList requiredSymbols;
    int xrtModulesDeclared;
    int selected; /* Set only by the verified compile-unit selection. */
} XL6_PackageCSourceGroup;
void xl6PackageCSourceGroupFree(XL6_PackageCSourceGroup* group);

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
    int linkage; /* 0 unspecified, 1 static, 2 dynamic. */
    char* staticManifest;
    char* dynamicManifest;
} XL6_PackageNativeInfo;

enum {
    XL6_NATIVE_SELECT_AUTO_JIT = 0,
    XL6_NATIVE_SELECT_STATIC = 1,
    XL6_NATIVE_SELECT_DYNAMIC = 2,
    XL6_NATIVE_SELECT_AUTO_AOT = 3
};

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
    XL6_PackageStringList runtimeSymbols;
    unsigned int nativeLinkFlags;
    XL6_PackageCSourceGroup* cSourceGroups;
    size_t cSourceGroupCount;
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
/* Speculative resolution reports hit/miss through the return value/result,
 * never through the caller's runtime error slot. Both hit and miss preserve
 * its exact prior error. Explicit LoadManifest still publishes parse errors. */
int xl6PackageResolveImport(const char* importerPath, const char* target, XL6_PackageResolveResult* result);
int xl6PackageLoadManifestObserved(const char* path, XL6_PackageManifest* manifest,
                                   struct XL6_SourceReport* report);
/* Native-only alternative with the same package/language/dependency identity.
 * 1 selected, 0 keep base, -1 invalid/unavailable. Never follows variant chains. */
int xl6PackageSelectNativeVariant(const XL6_PackageManifest* base, int selection,
                                 XL6_PackageManifest* selected,
                                 struct XL6_SourceReport* report,
                                 char* error, size_t errorSize);
int xl6PackageResolveImportObserved(const char* importerPath, const char* target,
                                    XL6_PackageResolveResult* result,
                                    struct XL6_SourceReport* report);
int xl6PackageJoinPath(char* out, size_t outSize, const char* a, const char* b);

#endif
