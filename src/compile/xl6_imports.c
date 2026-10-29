#include "src/compile/xl6_build_inputs.h"
#include "src/compile/xl6_imports.h"

#include "src/common/xl6_common.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_type_registry.h"
#include "src/package/xl6_compiled_interface.inc"

typedef struct XL6_ImportTextBuilder {
    char* data;
    size_t len;
    size_t capacity;
    int ok;
} XL6_ImportTextBuilder;

typedef struct XL6_ImportSymbol {
    char* ns;
    char* name;
    char* internalName;
    XL6_AstKind kind;
    int supported;
} XL6_ImportSymbol;

typedef struct XL6_ImportNamespace {
    char* ns;
    int isPrivate;
    int loaded;
} XL6_ImportNamespace;

typedef struct XL6_ImportSymbolTable {
    XL6_ImportSymbol* items;
    size_t count;
    XL6_ImportNamespace* namespaces;
    size_t namespaceCount;
} XL6_ImportSymbolTable;

typedef struct XL6_ImportNameMapItem {
    char* name;
    char* internalName;
} XL6_ImportNameMapItem;

typedef struct XL6_ImportNameMap {
    XL6_ImportNameMapItem* items;
    size_t count;
} XL6_ImportNameMap;

static int xl6ImportsTextEquals(const char* a, const char* b)
{
    return a != NULL && b != NULL && strcmp(a, b) == 0;
}

static char* xl6ImportsDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtStrDupN(src, strlen(src));
}

static void xl6ImportsBuilderInit(XL6_ImportTextBuilder* builder)
{
    memset(builder, 0, sizeof(*builder));
    builder->ok = 1;
}

static void xl6ImportsBuilderFree(XL6_ImportTextBuilder* builder)
{
    if ( builder == NULL ) {
        return;
    }
    xrtFree(builder->data);
    memset(builder, 0, sizeof(*builder));
}

static int xl6ImportsBuilderGrow(XL6_ImportTextBuilder* builder, size_t addLen)
{
    char* newData;
    size_t required;
    size_t newCapacity;

    if ( builder == NULL || !builder->ok ) {
        return 0;
    }
    required = builder->len + addLen + 1;
    if ( required <= builder->capacity ) {
        return 1;
    }
    newCapacity = builder->capacity == 0 ? 256 : builder->capacity;
    while ( newCapacity < required ) {
        newCapacity *= 2;
    }
    newData = (char*)xrtRealloc(builder->data, newCapacity);
    if ( newData == NULL ) {
        builder->ok = 0;
        return 0;
    }
    builder->data = newData;
    builder->capacity = newCapacity;
    return 1;
}

static void xl6ImportsBuilderAppendRange(XL6_ImportTextBuilder* builder, const char* text, size_t len)
{
    if ( builder == NULL || text == NULL || !xl6ImportsBuilderGrow(builder, len) ) {
        return;
    }
    if ( len > 0 ) {
        memcpy(builder->data + builder->len, text, len);
    }
    builder->len += len;
    builder->data[builder->len] = '\0';
}

static void xl6ImportsBuilderAppend(XL6_ImportTextBuilder* builder, const char* text)
{
    if ( text != NULL ) {
        xl6ImportsBuilderAppendRange(builder, text, strlen(text));
    }
}

static void xl6ImportsBuilderAppendQuoted(XL6_ImportTextBuilder* builder, const char* text)
{
    const char* p;

    xl6ImportsBuilderAppend(builder, "\"");
    for ( p = text != NULL ? text : ""; *p != '\0'; ++p ) {
        if ( *p == '"' || *p == '\\' ) {
            xl6ImportsBuilderAppend(builder, "\\");
        }
        xl6ImportsBuilderAppendRange(builder, p, 1);
    }
    xl6ImportsBuilderAppend(builder, "\"");
}

static char* xl6ImportsBuilderTake(XL6_ImportTextBuilder* builder)
{
    char* out;

    if ( builder == NULL || !builder->ok ) {
        return NULL;
    }
    if ( builder->data == NULL ) {
        builder->data = xrtStrDup("");
    }
    out = builder->data;
    builder->data = NULL;
    builder->len = 0;
    builder->capacity = 0;
    return out;
}

static char* xl6ImportsDupImportTarget(const char* text)
{
    size_t len;

    if ( text == NULL ) {
        return xrtStrDup("");
    }
    len = strlen(text);
    if ( len >= 2 && ((text[0] == '"' && text[len - 1] == '"') ||
                      (text[0] == '\'' && text[len - 1] == '\'')) ) {
        return xrtStrDupN(text + 1, len - 2);
    }
    return xrtStrDupN(text, len);
}

static int xl6ImportsTryBuiltinFeatureImport(const char* target, XL6_ImportDeps* deps)
{
    if ( target == NULL || deps == NULL ) {
        return 0;
    }

    /*
     * 内置脚本模块不是 package，也不读取外部接口文件。
     * 它们只声明编译单元需要的运行/编译能力，后续由 cgen/API/TCC 链路统一消费。
     */
    if ( xl6ImportsTextEquals(target, "tcc") ) {
        deps->features |= XL6_IMPORT_FEATURE_TCC_RUNTIME;
        return 1;
    }
    if ( xl6ImportsTextEquals(target, "xlang") ) {
        deps->features |= XL6_IMPORT_FEATURE_XLANG_RUNTIME;
        return 1;
    }
    if ( xl6ImportsTextEquals(target, "xlang.compiler") ) {
        deps->features |= XL6_IMPORT_FEATURE_XLANG_RUNTIME | XL6_IMPORT_FEATURE_XLANG_COMPILER;
        return 1;
    }
    /* Resident advanced declarations are selected by explicit import; actual
     * native source dependencies are derived from verified IR, not this name. */
    if ( xl6ImportsTextEquals(target, "concurrent") || xl6ImportsTextEquals(target, "memory") ) return 1;
    return 0;
}

void xl6ImportDepsInit(XL6_ImportDeps* deps)
{
    if ( deps == NULL ) {
        return;
    }
    memset(deps, 0, sizeof(*deps));
    xl6PackageStringListInit(&deps->cSources);
    xl6PackageStringListInit(&deps->includeDirs);
    xl6PackageStringListInit(&deps->xrtModules);
    xl6PackageStringListInit(&deps->links);
    xl6PackageStringListInit(&deps->loadlibs);
    xl6PackageStringListInit(&deps->activeSources);
}

void xl6ImportDepsFree(XL6_ImportDeps* deps)
{
    for (size_t i = 0; deps && i < deps->packageInfoCount; ++i) {
        XL6_ImportPackageInfo* info = &deps->packageInfos[i];
        xrtFree(info->name); xrtFree(info->version); xrtFree(info->abiVersion); xrtFree(info->dir);
        xl6PackageStringListFree(&info->dependencies);
    }
    if (deps) xrtFree(deps->packageInfos);
    size_t i;

    if ( deps == NULL ) {
        return;
    }
    xl6PackageStringListFree(&deps->cSources);
    xl6PackageStringListFree(&deps->includeDirs);
    xl6PackageStringListFree(&deps->xrtModules);
    xl6PackageStringListFree(&deps->links);
    xl6PackageStringListFree(&deps->loadlibs);
    xl6PackageStringListFree(&deps->activeSources);
    for ( i = 0; i < deps->packageCount; ++i ) {
        xrtFree(deps->packageIds[i]);
    }
    xrtFree(deps->packageIds);
    for ( i = 0; i < deps->nativeCount; ++i ) {
        xrtFree(deps->natives[i].packageId);
        xrtFree(deps->natives[i].loadName);
        xrtFree(deps->natives[i].version);
    }
    xrtFree(deps->natives);
    memset(deps, 0, sizeof(*deps));
}

static int xl6ImportsStringListContains(const XL6_PackageStringList* list, const char* text)
{
    size_t i;

    if ( list == NULL || text == NULL ) {
        return 0;
    }
    for ( i = 0; i < list->count; ++i ) {
        if ( xl6ImportsTextEquals(list->items[i], text) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6ImportsStringListAppendUnique(XL6_PackageStringList* list, const char* text)
{
    if ( text == NULL || text[0] == '\0' || xl6ImportsStringListContains(list, text) ) {
        return 1;
    }
    return xl6PackageStringListAppend(list, text);
}

static int xl6ImportsPathTextEquals(const char* a, const char* b)
{
    unsigned char ca;
    unsigned char cb;

    if ( a == NULL || b == NULL ) {
        return 0;
    }
    while ( *a != '\0' && *b != '\0' ) {
        ca = (unsigned char)*a++;
        cb = (unsigned char)*b++;
        if ( ca == '\\' ) {
            ca = '/';
        }
        if ( cb == '\\' ) {
            cb = '/';
        }
#ifdef _WIN32
        /*
         * Windows 路径不区分 ASCII 大小写。UTF-8 非 ASCII 部分保持原样，
         * 避免依赖当前代码页，也足以覆盖盘符和常见目录大小写差异。
         */
        if ( ca >= 'A' && ca <= 'Z' ) {
            ca = (unsigned char)(ca - 'A' + 'a');
        }
        if ( cb >= 'A' && cb <= 'Z' ) {
            cb = (unsigned char)(cb - 'A' + 'a');
        }
#endif
        if ( ca != cb ) {
            return 0;
        }
    }
    return *a == '\0' && *b == '\0';
}

static int xl6ImportsPathListContains(const XL6_PackageStringList* list, const char* path)
{
    size_t i;

    if ( list == NULL || path == NULL ) {
        return 0;
    }
    for ( i = 0; i < list->count; ++i ) {
        if ( xl6ImportsPathTextEquals(list->items[i], path) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6ImportsStringListPop(XL6_PackageStringList* list)
{
    if ( list == NULL || list->count == 0 ) {
        return;
    }
    --list->count;
    xrtFree(list->items[list->count]);
    list->items[list->count] = NULL;
}

static int xl6ImportsDepsHasPackageId(const XL6_ImportDeps* deps, const char* packageId)
{
    size_t i;

    if ( deps == NULL || packageId == NULL || packageId[0] == '\0' ) {
        return 0;
    }
    for ( i = 0; i < deps->packageCount; ++i ) {
        if ( xl6ImportsTextEquals(deps->packageIds[i], packageId) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6ImportsPathIsAbsolute(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( path[0] == '\\' || path[0] == '/' ) {
        return 1;
    }
    return path[1] == ':';
}

static int xl6ImportsLooksLikePath(const char* text)
{
    const char* p;

    if ( text == NULL ) {
        return 0;
    }
    for ( p = text; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' || *p == '.' ) {
            return 1;
        }
    }
    return 0;
}

static int xl6ImportsEndsWithNoCase(const char* text, const char* suffix)
{
    size_t textLen;
    size_t suffixLen;
    size_t i;

    if ( text == NULL || suffix == NULL ) {
        return 0;
    }
    textLen = strlen(text);
    suffixLen = strlen(suffix);
    if ( suffixLen > textLen ) {
        return 0;
    }
    text += textLen - suffixLen;
    for ( i = 0; i < suffixLen; ++i ) {
        unsigned char a = (unsigned char)text[i];
        unsigned char b = (unsigned char)suffix[i];
        if ( a >= 'A' && a <= 'Z' ) {
            a = (unsigned char)(a - 'A' + 'a');
        }
        if ( b >= 'A' && b <= 'Z' ) {
            b = (unsigned char)(b - 'A' + 'a');
        }
        if ( a != b ) {
            return 0;
        }
    }
    return 1;
}

static void xl6ImportsAddSourceCompanionC(const char* sourcePath, XL6_ImportDeps* deps)
{
    char cPath[512];
    size_t len;

    if ( sourcePath == NULL || deps == NULL || !xl6ImportsEndsWithNoCase(sourcePath, ".xl") ) {
        return;
    }
    len = strlen(sourcePath);
    if ( len + 1 >= sizeof(cPath) ) {
        return;
    }
    memcpy(cPath, sourcePath, len + 1);
    cPath[len - 2] = 'c';
    cPath[len - 1] = '\0';
    if ( xl6BuildInputExists(cPath) ) {
        (void)xl6ImportsStringListAppendUnique(&deps->cSources, cPath);
        deps->hasUndeclaredCSourceModules = 1;
    }
}

static int xl6ImportsAppendPackagePath(XL6_PackageStringList* list, const char* packageDir, const char* value, int alwaysPath)
{
    char path[512];

    if ( value == NULL || value[0] == '\0' ) {
        return 1;
    }
    if ( !alwaysPath && !xl6ImportsLooksLikePath(value) ) {
        return xl6ImportsStringListAppendUnique(list, value);
    }
    if ( xl6ImportsPathIsAbsolute(value) || packageDir == NULL || packageDir[0] == '\0' ) {
        return xl6ImportsStringListAppendUnique(list, value);
    }
    if ( !xl6PackageJoinPath(path, sizeof(path), packageDir, value) ) {
        return 0;
    }
    return xl6ImportsStringListAppendUnique(list, path);
}

static int xl6ImportsDepsAddPackageId(XL6_ImportDeps* deps, const char* packageId)
{
    char** newItems;

    if ( deps == NULL || packageId == NULL || packageId[0] == '\0' ) {
        return 1;
    }
    if ( xl6ImportsDepsHasPackageId(deps, packageId) ) {
        return 1;
    }
    newItems = (char**)xrtRealloc(deps->packageIds, (deps->packageCount + 1) * sizeof(char*));
    if ( newItems == NULL ) {
        return 0;
    }
    deps->packageIds = newItems;
    deps->packageIds[deps->packageCount] = xl6ImportsDupText(packageId);
    if ( deps->packageIds[deps->packageCount] == NULL ) {
        return 0;
    }
    deps->packageCount += 1;
    return 1;
}

static int xl6ImportsDepsAddNative(
    XL6_ImportDeps* deps,
    const char* packageId,
    int singleton,
    const char* loadName,
    const char* version,
    XL6_AstNode* importNode,
    XL6_DiagList* diags)
{
    XL6_ImportNativeItem* newItems;
    XL6_ImportNativeItem* item;
    const char* versionText;
    size_t i;

    if ( deps == NULL || !singleton || loadName == NULL || loadName[0] == '\0' ) {
        return 1;
    }
    versionText = version != NULL ? version : "";
    for ( i = 0; i < deps->nativeCount; ++i ) {
        XL6_ImportNativeItem* existing = &deps->natives[i];
        if ( existing->singleton && xl6ImportsTextEquals(existing->loadName, loadName) ) {
            if ( xl6ImportsTextEquals(existing->version, versionText) &&
                 xl6ImportsTextEquals(existing->packageId, packageId) ) {
                return 1;
            }
            if ( diags != NULL ) {
                xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                           importNode != NULL ? importNode->column : 1,
                           "native singleton package conflict");
            }
            return 0;
        }
    }
    newItems = (XL6_ImportNativeItem*)xrtRealloc(deps->natives, (deps->nativeCount + 1) * sizeof(XL6_ImportNativeItem));
    if ( newItems == NULL ) {
        return 0;
    }
    deps->natives = newItems;
    item = &deps->natives[deps->nativeCount];
    memset(item, 0, sizeof(*item));
    item->packageId = xl6ImportsDupText(packageId);
    item->singleton = singleton;
    item->loadName = xl6ImportsDupText(loadName);
    item->version = xl6ImportsDupText(versionText);
    if ( item->packageId == NULL || item->loadName == NULL || item->version == NULL ) {
        return 0;
    }
    deps->nativeCount += 1;
    return 1;
}

static int xl6ImportsCollectResolvedPackageDeps(
    XL6_ImportDeps* deps,
    const XL6_PackageResolveResult* resolved,
    XL6_AstNode* importNode,
    XL6_DiagList* diags,
    XL6_ImportSymbolTable* symbols);
static int xl6ImportsSymbolTableAddNamespace(XL6_ImportSymbolTable* table, const char* ns, int isPrivate);
static const XL6_ImportNamespace* xl6ImportsFindNamespace(const XL6_ImportSymbolTable* table, const char* ns);
static int xl6ImportsSymbolTableAdd(
    XL6_ImportSymbolTable* table,
    const char* ns,
    const char* name,
    const char* internalName,
    XL6_AstKind kind,
    int supported);
static void xl6ImportsNamespaceFromTarget(char* out, size_t outSize, const char* target);

static int xl6ImportsCollectManifestDeps(
    XL6_ImportDeps* deps,
    const XL6_PackageManifest* manifest,
    XL6_AstNode* importNode,
    XL6_DiagList* diags)
{
    size_t i;

    if ( deps == NULL || manifest == NULL ) {
        return 1;
    }
    int knownPackage = 0;
    for (size_t p = 0; p < deps->packageInfoCount; ++p)
        if (!strcmp(deps->packageInfos[p].name, manifest->name)) knownPackage = 1;
    if (!knownPackage) {
        XL6_ImportPackageInfo info = {0};
        info.name = xrtStrDup(manifest->name); info.version = xrtStrDup(manifest->version);
        info.abiVersion = xrtStrDup(manifest->abiVersion); info.dir = xrtPathAbs(manifest->dir);
        int copied = info.name && info.version && info.abiVersion && info.dir;
        for (size_t p = 0; copied && p < manifest->dependencyCount; ++p)
            copied = xl6PackageStringListAppend(&info.dependencies, manifest->dependencies[p].name);
        XL6_ImportPackageInfo* items = copied ? (XL6_ImportPackageInfo*)xrtRealloc(
            deps->packageInfos, (deps->packageInfoCount + 1) * sizeof(*items)) : NULL;
        if (!items) {
            xrtFree(info.name); xrtFree(info.version); xrtFree(info.abiVersion); xrtFree(info.dir);
            xl6PackageStringListFree(&info.dependencies); return 0;
        }
        deps->packageInfos = items; items[deps->packageInfoCount++] = info;
    }
    if ( !xl6ImportsDepsAddPackageId(deps, manifest->packageId) ) {
        return 0;
    }
    for ( i = 0; i < manifest->cSources.count; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->cSources, manifest->dir, manifest->cSources.items[i], 1) ) {
            return 0;
        }
    }
    for ( i = 0; i < manifest->includeDirs.count; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->includeDirs, manifest->dir, manifest->includeDirs.items[i], 1) ) {
            return 0;
        }
    }
    if ( manifest->cSources.count > 0 && !manifest->xrtModulesDeclared ) {
        deps->hasUndeclaredCSourceModules = 1;
    }
    for ( i = 0; i < manifest->xrtModules.count; ++i ) {
        if ( !xl6ImportsStringListAppendUnique(&deps->xrtModules, manifest->xrtModules.items[i]) ) {
            return 0;
        }
    }
    for ( i = 0; i < manifest->links.count; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->links, manifest->dir, manifest->links.items[i], 0) ) {
            return 0;
        }
    }
    for ( i = 0; i < manifest->loadlibs.count; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->loadlibs, manifest->dir, manifest->loadlibs.items[i], 0) ) {
            return 0;
        }
    }
    return xl6ImportsDepsAddNative(
        deps,
        manifest->packageId,
        manifest->nativeInfo.singleton,
        manifest->nativeInfo.loadName,
        manifest->nativeInfo.version,
        importNode,
        diags);
}

static int xl6ImportsDependencyMatches(
    const XL6_PackageDependency* dep,
    const XL6_PackageResolveResult* resolved,
    XL6_AstNode* importNode,
    XL6_DiagList* diags)
{
    char message[256];
    int versionOk;
    int abiOk;

    if ( dep == NULL || resolved == NULL ) {
        return 0;
    }
    versionOk = xl6ImportsTextEquals(dep->version, "*") ||
                xl6ImportsTextEquals(dep->version, resolved->packageVersion);
    abiOk = xl6ImportsTextEquals(dep->abiVersion, "*") ||
            xl6ImportsTextEquals(dep->abiVersion, resolved->abiVersion);
    if ( !versionOk || !abiOk ) {
        if ( diags != NULL ) {
            snprintf(message,
                     sizeof(message),
                     "package dependency version mismatch: %s",
                     dep->packageId != NULL ? dep->packageId : "");
            xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                       importNode != NULL ? importNode->column : 1,
                       message);
        }
        return 0;
    }
    return 1;
}

static int xl6ImportsResolveDependency(
    const XL6_PackageManifest* manifest,
    const XL6_PackageDependency* dep,
    XL6_PackageResolveResult* out,
    XL6_AstNode* importNode,
    XL6_DiagList* diags,
    XL6_SourceReport* report)
{
    XL6_PackageResolveResult candidate;
    char* versionedName;
    int sawMismatchedPackage;

    if ( manifest == NULL || dep == NULL || out == NULL ) {
        return 0;
    }
    xl6PackageResolveResultInit(out);
    sawMismatchedPackage = 0;
    xl6PackageResolveImportObserved(manifest->dir, dep->name, &candidate, report);
    if ( candidate.resolved ) {
        if ( xl6ImportsDependencyMatches(dep, &candidate, NULL, NULL) ) {
            *out = candidate;
            return 1;
        }
        sawMismatchedPackage = 1;
    }
    xl6PackageResolveResultFree(&candidate);

    if ( xl6ImportsTextEquals(dep->version, "*") ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                       importNode != NULL ? importNode->column : 1,
                       "package dependency is not resolved");
        }
        return 0;
    }

    versionedName = (char*)xrtFormat("%s@%s", dep->name, dep->version);
    if ( versionedName == NULL ) {
        return 0;
    }
    xl6PackageResolveImportObserved(manifest->dir, versionedName, &candidate, report);
    xrtFree(versionedName);
    if ( candidate.resolved ) {
        if ( xl6ImportsDependencyMatches(dep, &candidate, importNode, diags) ) {
            *out = candidate;
            return 1;
        }
        xl6PackageResolveResultFree(&candidate);
        return 0;
    }
    xl6PackageResolveResultFree(&candidate);

    if ( sawMismatchedPackage ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                       importNode != NULL ? importNode->column : 1,
                       "package dependency version mismatch");
        }
        return 0;
    }
    if ( diags != NULL ) {
        xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                   importNode != NULL ? importNode->column : 1,
                   "package dependency is not resolved");
    }
    return 0;
}

static int xl6ImportsCollectManifestDependencyGraph(
    XL6_ImportDeps* deps,
    const XL6_PackageManifest* manifest,
    XL6_AstNode* importNode,
    XL6_DiagList* diags,
    XL6_ImportSymbolTable* symbols)
{
    size_t i;

    if ( deps == NULL || manifest == NULL ) {
        return 1;
    }
    for ( i = 0; i < manifest->dependencyCount; ++i ) {
        const XL6_PackageDependency* dep = &manifest->dependencies[i];
        XL6_PackageResolveResult resolved;

        if ( xl6ImportsDepsHasPackageId(deps, dep->packageId) ) {
            continue;
        }
        if ( !xl6ImportsResolveDependency(manifest, dep, &resolved, importNode, diags,
                                         deps->sourceOverlay ? deps->sourceOverlay->report : NULL) ) {
            return 0;
        }
        if ( !xl6ImportsCollectResolvedPackageDeps(deps, &resolved, importNode, diags, symbols) ) {
            xl6PackageResolveResultFree(&resolved);
            return 0;
        }
        xl6PackageResolveResultFree(&resolved);
    }
    return 1;
}

static int xl6ImportsCollectResolvedPackageDeps(
    XL6_ImportDeps* deps,
    const XL6_PackageResolveResult* resolved,
    XL6_AstNode* importNode,
    XL6_DiagList* diags,
    XL6_ImportSymbolTable* symbols)
{
    XL6_PackageManifest manifest;
    int hasManifest;
    int ok;

    if ( deps == NULL || resolved == NULL || !resolved->resolved ) {
        return 1;
    }
    if ( resolved->packageId != NULL && resolved->packageId[0] != '\0' &&
         xl6ImportsDepsHasPackageId(deps, resolved->packageId) ) {
        return 1;
    }

    xl6PackageManifestInit(&manifest);
    hasManifest = resolved->manifestPath != NULL && resolved->manifestPath[0] != '\0' &&
                  xl6PackageLoadManifestObserved(resolved->manifestPath, &manifest,
                                                deps->sourceOverlay ? deps->sourceOverlay->report : NULL);
    if ( !hasManifest ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                       importNode != NULL ? importNode->column : 1,
                       "import dependency requires xlang.package.json source manifest");
        }
        xl6PackageManifestFree(&manifest);
        return 0;
    }
    ok = 1;
    if ( symbols != NULL && resolved->packageName != NULL && resolved->packageName[0] != '\0' ) {
        ok = xl6ImportsSymbolTableAddNamespace(symbols, resolved->packageName, 1);
    }
    if ( ok && hasManifest ) {
        ok = xl6ImportsCollectManifestDeps(deps, &manifest, importNode, diags);
    }
    if ( ok && hasManifest ) {
        ok = xl6ImportsCollectManifestDependencyGraph(deps, &manifest, importNode, diags, symbols);
    }

    xl6PackageManifestFree(&manifest);
    return ok;
}

static void xl6ImportsSymbolTableInit(XL6_ImportSymbolTable* table)
{
    if ( table != NULL ) {
        memset(table, 0, sizeof(*table));
    }
}

static void xl6ImportsSymbolTableFree(XL6_ImportSymbolTable* table)
{
    size_t i;

    if ( table == NULL ) {
        return;
    }
    for ( i = 0; i < table->count; ++i ) {
        xrtFree(table->items[i].ns);
        xrtFree(table->items[i].name);
        xrtFree(table->items[i].internalName);
    }
    xrtFree(table->items);
    for ( i = 0; i < table->namespaceCount; ++i ) {
        xrtFree(table->namespaces[i].ns);
    }
    xrtFree(table->namespaces);
    memset(table, 0, sizeof(*table));
}

static void xl6ImportsNameMapInit(XL6_ImportNameMap* map)
{
    if ( map != NULL ) {
        memset(map, 0, sizeof(*map));
    }
}

static void xl6ImportsNameMapFree(XL6_ImportNameMap* map)
{
    size_t i;

    if ( map == NULL ) {
        return;
    }
    for ( i = 0; i < map->count; ++i ) {
        xrtFree(map->items[i].name);
        xrtFree(map->items[i].internalName);
    }
    xrtFree(map->items);
    memset(map, 0, sizeof(*map));
}

static const char* xl6ImportsNameMapFind(const XL6_ImportNameMap* map, const char* name)
{
    size_t i;

    if ( map == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < map->count; ++i ) {
        if ( xl6ImportsTextEquals(map->items[i].name, name) ) {
            return map->items[i].internalName;
        }
    }
    return NULL;
}

static char* xl6ImportsNameMapRewriteDotted(const XL6_ImportNameMap* map, const char* name)
{
    size_t i;

    if ( map == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < map->count; ++i ) {
        size_t prefixLen = strlen(map->items[i].name);

        if ( strncmp(name, map->items[i].name, prefixLen) == 0 &&
             name[prefixLen] == '.' ) {
            return (char*)xrtFormat("%s%s", map->items[i].internalName, name + prefixLen);
        }
    }
    return NULL;
}

static int xl6ImportsNameMapAdd(XL6_ImportNameMap* map, const char* name, const char* internalName)
{
    XL6_ImportNameMapItem* newItems;
    XL6_ImportNameMapItem* item;

    if ( map == NULL || name == NULL || name[0] == '\0' || internalName == NULL || internalName[0] == '\0' ) {
        return 1;
    }
    if ( xl6ImportsNameMapFind(map, name) != NULL ) {
        return 1;
    }
    newItems = (XL6_ImportNameMapItem*)xrtRealloc(map->items, (map->count + 1) * sizeof(*map->items));
    if ( newItems == NULL ) {
        return 0;
    }
    map->items = newItems;
    item = &map->items[map->count];
    memset(item, 0, sizeof(*item));
    item->name = xl6ImportsDupText(name);
    item->internalName = xl6ImportsDupText(internalName);
    if ( item->name == NULL || item->internalName == NULL ) {
        xrtFree(item->name);
        xrtFree(item->internalName);
        memset(item, 0, sizeof(*item));
        return 0;
    }
    map->count += 1;
    return 1;
}

static XL6_ImportNamespace* xl6ImportsFindNamespaceMutable(XL6_ImportSymbolTable* table, const char* ns)
{
    size_t i;

    if ( table == NULL || ns == NULL ) {
        return NULL;
    }
    for ( i = 0; i < table->namespaceCount; ++i ) {
        if ( xl6ImportsTextEquals(table->namespaces[i].ns, ns) ) {
            return &table->namespaces[i];
        }
    }
    return NULL;
}

static const XL6_ImportNamespace* xl6ImportsFindNamespace(const XL6_ImportSymbolTable* table, const char* ns)
{
    return xl6ImportsFindNamespaceMutable((XL6_ImportSymbolTable*)table, ns);
}

static int xl6ImportsSymbolTableAddNamespace(XL6_ImportSymbolTable* table, const char* ns, int isPrivate)
{
    XL6_ImportNamespace* existing;
    XL6_ImportNamespace* newItems;
    XL6_ImportNamespace* item;

    if ( table == NULL || ns == NULL || ns[0] == '\0' ) {
        return 0;
    }
    existing = xl6ImportsFindNamespaceMutable(table, ns);
    if ( existing != NULL ) {
        if ( !isPrivate ) {
            existing->isPrivate = 0;
        }
        return 1;
    }
    newItems = (XL6_ImportNamespace*)xrtRealloc(
        table->namespaces,
        (table->namespaceCount + 1) * sizeof(XL6_ImportNamespace));
    if ( newItems == NULL ) {
        return 0;
    }
    table->namespaces = newItems;
    item = &table->namespaces[table->namespaceCount];
    memset(item, 0, sizeof(*item));
    item->ns = xl6ImportsDupText(ns);
    item->isPrivate = isPrivate ? 1 : 0;
    if ( item->ns == NULL ) {
        memset(item, 0, sizeof(*item));
        return 0;
    }
    table->namespaceCount += 1;
    return 1;
}

static int xl6ImportsNamespaceIsLoaded(const XL6_ImportSymbolTable* table, const char* ns)
{
    const XL6_ImportNamespace* item = xl6ImportsFindNamespace(table, ns);

    return item != NULL && item->loaded;
}

static int xl6ImportsNamespaceIsPublic(const XL6_ImportSymbolTable* table, const char* ns)
{
    const XL6_ImportNamespace* item = xl6ImportsFindNamespace(table, ns);

    return item != NULL && !item->isPrivate;
}

static void xl6ImportsNamespaceMarkLoaded(XL6_ImportSymbolTable* table, const char* ns)
{
    XL6_ImportNamespace* item = xl6ImportsFindNamespaceMutable(table, ns);

    if ( item != NULL ) {
        item->loaded = 1;
    }
}

static int xl6ImportsNamespaceExists(const XL6_ImportSymbolTable* table, const char* ns)
{
    return xl6ImportsFindNamespace(table, ns) != NULL;
}

static int xl6ImportsSymbolIsType(const XL6_ImportSymbol* symbol)
{
    if ( symbol == NULL ) {
        return 0;
    }
    return symbol->kind == XL6_AST_CLASS ||
           symbol->kind == XL6_AST_PROTOCOL ||
           symbol->kind == XL6_AST_ENUM ||
           symbol->kind == XL6_AST_UNION ||
           symbol->kind == XL6_AST_TYPE_ALIAS ||
           symbol->kind == XL6_AST_EXTERN_TYPE_ALIAS;
}

static const XL6_ImportSymbol* xl6ImportsFindSymbol(const XL6_ImportSymbolTable* table, const char* ns, const char* name)
{
    size_t i;

    if ( table == NULL || ns == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < table->count; ++i ) {
        if ( xl6ImportsTextEquals(table->items[i].ns, ns) &&
             xl6ImportsTextEquals(table->items[i].name, name) ) {
            return &table->items[i];
        }
    }
    return NULL;
}

static int xl6ImportsCollectMemberParts(
    const XL6_AstNode* node,
    const char** parts,
    size_t maxParts,
    size_t* count)
{
    if ( node == NULL || parts == NULL || count == NULL || *count >= maxParts ) {
        return 0;
    }
    if ( node->kind == XL6_AST_NAME ) {
        parts[*count] = node->text;
        *count += 1;
        return node->text != NULL && node->text[0] != '\0';
    }
    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL ) {
        if ( !xl6ImportsCollectMemberParts(node->children[0], parts, maxParts, count) ) {
            return 0;
        }
        if ( *count >= maxParts || node->text == NULL || node->text[0] == '\0' ) {
            return 0;
        }
        parts[*count] = node->text;
        *count += 1;
        return 1;
    }
    return 0;
}

static char* xl6ImportsJoinParts(const char** parts, size_t count)
{
    XL6_ImportTextBuilder builder;
    size_t i;
    char* out;

    if ( parts == NULL || count == 0 ) {
        return NULL;
    }
    xl6ImportsBuilderInit(&builder);
    for ( i = 0; i < count; ++i ) {
        if ( i > 0 ) {
            xl6ImportsBuilderAppend(&builder, ".");
        }
        xl6ImportsBuilderAppend(&builder, parts[i]);
    }
    out = xl6ImportsBuilderTake(&builder);
    xl6ImportsBuilderFree(&builder);
    return out;
}

static int xl6ImportsIsIdentChar(int ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_';
}

static char* xl6ImportsMakeInternalName(const char* ns, const char* name)
{
    XL6_ImportTextBuilder builder;
    const char* p;
    char* out;

    xl6ImportsBuilderInit(&builder);
    xl6ImportsBuilderAppend(&builder, "__xl6_import_");
    for ( p = ns != NULL ? ns : "pkg"; *p != '\0'; ++p ) {
        char ch = xl6ImportsIsIdentChar((unsigned char)*p) ? *p : '_';
        xl6ImportsBuilderAppendRange(&builder, &ch, 1);
    }
    xl6ImportsBuilderAppend(&builder, "_");
    for ( p = name != NULL ? name : "sym"; *p != '\0'; ++p ) {
        char ch = xl6ImportsIsIdentChar((unsigned char)*p) ? *p : '_';
        xl6ImportsBuilderAppendRange(&builder, &ch, 1);
    }
    out = xl6ImportsBuilderTake(&builder);
    xl6ImportsBuilderFree(&builder);
    return out;
}

static char* xl6ImportsMakeLocalInternalName(const char* ns, const char* name)
{
    XL6_ImportTextBuilder builder;
    const char* p;
    char* out;

    xl6ImportsBuilderInit(&builder);
    xl6ImportsBuilderAppend(&builder, "__xl6_import_");
    for ( p = ns != NULL ? ns : "pkg"; *p != '\0'; ++p ) {
        char ch = xl6ImportsIsIdentChar((unsigned char)*p) ? *p : '_';
        xl6ImportsBuilderAppendRange(&builder, &ch, 1);
    }
    xl6ImportsBuilderAppend(&builder, "__local_");
    for ( p = name != NULL ? name : "sym"; *p != '\0'; ++p ) {
        char ch = xl6ImportsIsIdentChar((unsigned char)*p) ? *p : '_';
        xl6ImportsBuilderAppendRange(&builder, &ch, 1);
    }
    out = xl6ImportsBuilderTake(&builder);
    xl6ImportsBuilderFree(&builder);
    return out;
}

static int xl6ImportsSanitizedNs(char* out, size_t outSize, const char* ns)
{
    size_t len;
    const char* p;

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    len = 0;
    for ( p = ns != NULL ? ns : "module"; *p != '\0'; ++p ) {
        char ch = xl6ImportsIsIdentChar((unsigned char)*p) ? *p : '_';
        if ( len + 1 >= outSize ) {
            out[0] = '\0';
            return 0;
        }
        out[len++] = ch;
    }
    out[len] = '\0';
    return len > 0;
}

static char* xl6ImportsPublicNameFromInternal(const char* ns, const char* internalName)
{
    char sanitized[256];
    char prefix[320];
    const char* publicName;
    size_t len;

    if ( internalName == NULL || !xl6ImportsSanitizedNs(sanitized, sizeof(sanitized), ns) ) {
        return NULL;
    }
    if ( snprintf(prefix, sizeof(prefix), "__xl6_import_%s_", sanitized) <= 0 ) {
        return NULL;
    }
    len = strlen(prefix);
    if ( strncmp(internalName, prefix, len) != 0 || internalName[len] == '\0' ) {
        return NULL;
    }
    publicName = internalName + len;
    len = strlen(publicName);
    if ( len > 7 && strcmp(publicName + len - 7, "_export") == 0 ) {
        len -= 7;
    }
    if ( len == 0 ) {
        return NULL;
    }
    return xrtStrDupN(publicName, len);
}

static XL6_AstNode* xl6ImportsDeclNameNode(XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    switch ( node->kind ) {
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_EXPORT_VAR:
    case XL6_AST_EXPORT_CONST:
    case XL6_AST_EXTERN_VAR:
        return node->childCount > 0 ? node->children[0] : NULL;
    default:
        return NULL;
    }
}

static const char* xl6ImportsDeclName(const XL6_AstNode* node)
{
    XL6_AstNode* nameNode;

    if ( node == NULL ) {
        return NULL;
    }
    switch ( node->kind ) {
    case XL6_AST_CLASS:
    case XL6_AST_PROTOCOL:
    case XL6_AST_ENUM:
    case XL6_AST_UNION:
    case XL6_AST_TYPE_ALIAS:
    case XL6_AST_EXTERN_TYPE_ALIAS:
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
        return node->text;
    default:
        break;
    }
    nameNode = xl6ImportsDeclNameNode((XL6_AstNode*)node);
    return nameNode != NULL ? nameNode->text : NULL;
}

static int xl6ImportsDeclCanBeRenamed(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return 0;
    }
    switch ( node->kind ) {
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_CLASS:
    case XL6_AST_PROTOCOL:
    case XL6_AST_ENUM:
    case XL6_AST_UNION:
    case XL6_AST_TYPE_ALIAS:
    case XL6_AST_EXTERN_TYPE_ALIAS:
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
    case XL6_AST_EXPORT_VAR:
    case XL6_AST_EXPORT_CONST:
    case XL6_AST_EXTERN_VAR:
        return 1;
    default:
        return 0;
    }
}

static void xl6ImportsSetDeclName(XL6_AstNode* node, const char* name)
{
    XL6_AstNode* nameNode;

    if ( node == NULL || name == NULL ) {
        return;
    }
    nameNode = xl6ImportsDeclNameNode(node);
    if ( nameNode != NULL ) {
        xrtFree(nameNode->text);
        nameNode->text = xl6ImportsDupText(name);
        return;
    }
    switch ( node->kind ) {
    case XL6_AST_CLASS:
    case XL6_AST_PROTOCOL:
    case XL6_AST_ENUM:
    case XL6_AST_UNION:
    case XL6_AST_TYPE_ALIAS:
    case XL6_AST_EXTERN_TYPE_ALIAS:
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
        xrtFree(node->text);
        node->text = xl6ImportsDupText(name);
        break;
    default:
        break;
    }
}

static int xl6ImportsIsModulePublicDecl(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return 0;
    }
    return node->kind == XL6_AST_EXPORT_FUNC ||
           node->kind == XL6_AST_EXPORT_VAR ||
           node->kind == XL6_AST_EXPORT_CONST ||
           (node->kind == XL6_AST_EXTERN_FUNC && (node->flags & XL6_AST_FLAG_PUBLIC) != 0) ||
           ((node->kind == XL6_AST_CLASS ||
             node->kind == XL6_AST_PROTOCOL ||
             node->kind == XL6_AST_ENUM ||
             node->kind == XL6_AST_UNION ||
             node->kind == XL6_AST_TYPE_ALIAS ||
             node->kind == XL6_AST_EXTERN_TYPE_ALIAS) &&
            (node->flags & XL6_AST_FLAG_PUBLIC) != 0) ||
           (node->kind == XL6_AST_EXTERN_VAR && (node->flags & XL6_AST_FLAG_PUBLIC) != 0);
}

static int xl6ImportsModuleHasDeclName(const XL6_AstNode* moduleRoot, const char* name)
{
    size_t i;

    if ( moduleRoot == NULL || name == NULL || name[0] == '\0' ) {
        return 0;
    }
    for ( i = 0; i < moduleRoot->childCount; ++i ) {
        const char* declName = xl6ImportsDeclName(moduleRoot->children[i]);
        if ( xl6ImportsTextEquals(declName, name) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6ImportsIsInternalImportName(const char* name);

static void xl6ImportsDropDuplicateImportedDecls(XL6_AstNode* dstRoot, XL6_AstNode* srcRoot)
{
    size_t i;

    if ( dstRoot == NULL || srcRoot == NULL ) {
        return;
    }
    i = 0;
    while ( i < srcRoot->childCount ) {
        XL6_AstNode* child = srcRoot->children[i];
        const char* declName = xl6ImportsDeclName(child);

        if ( xl6ImportsIsInternalImportName(declName) &&
             xl6ImportsModuleHasDeclName(dstRoot, declName) ) {
            xl6AstFree(child);
            if ( i + 1 < srcRoot->childCount ) {
                memmove(srcRoot->children + i,
                        srcRoot->children + i + 1,
                        (srcRoot->childCount - i - 1) * sizeof(*srcRoot->children));
            }
            srcRoot->childCount -= 1;
            continue;
        }
        i += 1;
    }
}

static int xl6ImportsIsInternalImportName(const char* name)
{
    return name != NULL && strncmp(name, "__xl6_import_", 13) == 0;
}

static int xl6ImportsModuleHasNaturalDeclName(const XL6_AstNode* moduleRoot, const char* name)
{
    size_t i;

    if ( moduleRoot == NULL || name == NULL || name[0] == '\0' ) {
        return 0;
    }
    for ( i = 0; i < moduleRoot->childCount; ++i ) {
        const char* declName = xl6ImportsDeclName(moduleRoot->children[i]);
        if ( declName != NULL &&
             !xl6ImportsIsInternalImportName(declName) &&
             xl6ImportsTextEquals(declName, name) ) {
            return 1;
        }
    }
    return 0;
}

static char* xl6ImportsImportedPublicNameForInternal(const XL6_AstNode* moduleRoot, const char* internalName)
{
    size_t i;

    if ( moduleRoot == NULL || internalName == NULL ) {
        return NULL;
    }
    for ( i = 0; i < moduleRoot->childCount; ++i ) {
        XL6_AstNode* child = moduleRoot->children[i];
        char* target;
        char ns[256];
        char* publicName;

        if ( child == NULL || child->kind != XL6_AST_IMPORT ) {
            continue;
        }

        target = xl6ImportsDupImportTarget(child->text);
        if ( target == NULL ) {
            return NULL;
        }
        xl6ImportsNamespaceFromTarget(ns, sizeof(ns), target);
        publicName = xl6ImportsPublicNameFromInternal(ns, internalName);
        xrtFree(target);
        if ( publicName != NULL ) {
            if ( strncmp(publicName, "_local_", 7) == 0 ) {
                xrtFree(publicName);
                continue;
            }
            return publicName;
        }
    }
    return NULL;
}

static int xl6ImportsScopeContains(const XL6_PackageStringList* scope, const char* name)
{
    size_t i;

    if ( scope == NULL || name == NULL ) {
        return 0;
    }
    for ( i = 0; i < scope->count; ++i ) {
        if ( xl6ImportsTextEquals(scope->items[i], name) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6ImportsScopeAdd(XL6_PackageStringList* scope, const char* name)
{
    if ( scope == NULL || name == NULL || name[0] == '\0' || xl6ImportsScopeContains(scope, name) ) {
        return 1;
    }
    return xl6PackageStringListAppend(scope, name);
}

static int xl6ImportsScopeCopy(XL6_PackageStringList* dst, const XL6_PackageStringList* src)
{
    size_t i;

    xl6PackageStringListInit(dst);
    if ( src == NULL ) {
        return 1;
    }
    for ( i = 0; i < src->count; ++i ) {
        if ( !xl6PackageStringListAppend(dst, src->items[i]) ) {
            xl6PackageStringListFree(dst);
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsScopeAddParams(XL6_PackageStringList* scope, const XL6_AstNode* params)
{
    size_t i;

    if ( scope == NULL || params == NULL ) {
        return 1;
    }
    for ( i = 0; i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        if ( param != NULL && param->kind == XL6_AST_PARAM ) {
            if ( !xl6ImportsScopeAdd(scope, param->text) ) {
                return 0;
            }
        }
    }
    return 1;
}

static int xl6ImportsScopeAddForTargets(XL6_PackageStringList* scope, const XL6_AstNode* targets)
{
    size_t i;

    if ( scope == NULL || targets == NULL ) {
        return 1;
    }
    for ( i = 0; i < targets->childCount; ++i ) {
        const XL6_AstNode* child = targets->children[i];
        if ( child != NULL && child->kind == XL6_AST_NAME ) {
            if ( !xl6ImportsScopeAdd(scope, child->text) ) {
                return 0;
            }
        }
    }
    return 1;
}

static int xl6ImportsRewriteNamesScoped(
    XL6_AstNode* node,
    const XL6_ImportNameMap* map,
    XL6_PackageStringList* scope);

static int xl6ImportsRewriteChildScoped(
    XL6_AstNode* child,
    const XL6_ImportNameMap* map,
    XL6_PackageStringList* scope)
{
    int ok;
    XL6_PackageStringList childScope;

    if ( child == NULL ) {
        return 1;
    }
    if ( child->kind != XL6_AST_BLOCK ) {
        return xl6ImportsRewriteNamesScoped(child, map, scope);
    }

    if ( !xl6ImportsScopeCopy(&childScope, scope) ) {
        return 0;
    }
    ok = xl6ImportsRewriteNamesScoped(child, map, &childScope);
    xl6PackageStringListFree(&childScope);
    return ok;
}

static int xl6ImportsRewriteFunctionLike(
    XL6_AstNode* node,
    const XL6_ImportNameMap* map,
    XL6_PackageStringList* scope,
    size_t paramsIndex,
    size_t returnsIndex,
    size_t bodyIndex)
{
    size_t i;
    int ok;
    XL6_PackageStringList functionScope;

    if ( node == NULL ) {
        return 1;
    }
    if ( !xl6ImportsScopeCopy(&functionScope, scope) ) {
        return 0;
    }
    if ( paramsIndex < node->childCount &&
         !xl6ImportsScopeAddParams(&functionScope, node->children[paramsIndex]) ) {
        xl6PackageStringListFree(&functionScope);
        return 0;
    }

    ok = 1;
    for ( i = 0; ok && i < node->childCount; ++i ) {
        if ( i == 0 && node->children[i] != NULL && node->children[i]->kind == XL6_AST_NAME ) {
            continue;
        }
        if ( i == bodyIndex ) {
            ok = xl6ImportsRewriteChildScoped(node->children[i], map, &functionScope);
        } else if ( i == paramsIndex || i == returnsIndex ) {
            ok = xl6ImportsRewriteNamesScoped(node->children[i], map, &functionScope);
        } else {
            ok = xl6ImportsRewriteChildScoped(node->children[i], map, scope);
        }
    }

    xl6PackageStringListFree(&functionScope);
    return ok;
}

static int xl6ImportsRewriteBlock(
    XL6_AstNode* node,
    const XL6_ImportNameMap* map,
    XL6_PackageStringList* scope)
{
    size_t i;

    if ( node == NULL ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        XL6_AstNode* child = node->children[i];
        if ( child == NULL ) {
            continue;
        }

        if ( child->kind == XL6_AST_VAR_DECL || child->kind == XL6_AST_CONST_DECL ) {
            if ( child->childCount > 0 &&
                 !xl6ImportsRewriteNamesScoped(child->children[0], map, scope) ) {
                return 0;
            }
            if ( child->childCount > 1 &&
                 !xl6ImportsRewriteNamesScoped(child->children[1], map, scope) ) {
                return 0;
            }
            if ( !xl6ImportsScopeAdd(scope, child->text) ) {
                return 0;
            }
            continue;
        }

        if ( child->kind == XL6_AST_MULTI_VAR_DECL ) {
            size_t j;
            if ( child->childCount > 1 &&
                 !xl6ImportsRewriteNamesScoped(child->children[1], map, scope) ) {
                return 0;
            }
            if ( child->childCount > 0 && child->children[0] != NULL ) {
                for ( j = 0; j < child->children[0]->childCount; ++j ) {
                    XL6_AstNode* name = child->children[0]->children[j];
                    if ( name != NULL && name->kind == XL6_AST_NAME &&
                         !xl6ImportsScopeAdd(scope, name->text) ) {
                        return 0;
                    }
                }
            }
            continue;
        }

        if ( child->kind == XL6_AST_FOR ) {
            int ok;
            XL6_PackageStringList loopScope;

            if ( child->childCount > 1 &&
                 !xl6ImportsRewriteNamesScoped(child->children[1], map, scope) ) {
                return 0;
            }
            if ( !xl6ImportsScopeCopy(&loopScope, scope) ) {
                return 0;
            }
            ok = child->childCount == 0 ||
                 xl6ImportsScopeAddForTargets(&loopScope, child->children[0]);
            if ( ok && child->childCount > 2 ) {
                ok = xl6ImportsRewriteNamesScoped(child->children[2], map, &loopScope);
            }
            xl6PackageStringListFree(&loopScope);
            if ( !ok ) {
                return 0;
            }
            continue;
        }

        if ( !xl6ImportsRewriteChildScoped(child, map, scope) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsRewriteNamesScoped(
    XL6_AstNode* node,
    const XL6_ImportNameMap* map,
    XL6_PackageStringList* scope)
{
    const char* internalName;
    size_t i;

    if ( node == NULL ) {
        return 1;
    }
    if ( node->kind == XL6_AST_IMPORT ) {
        return 1;
    }
    if ( node->kind == XL6_AST_NAME && node->text != NULL ) {
        char* dottedName;

        if ( xl6ImportsScopeContains(scope, node->text) ) {
            return 1;
        }
        internalName = xl6ImportsNameMapFind(map, node->text);
        if ( internalName != NULL ) {
            xrtFree(node->text);
            node->text = xl6ImportsDupText(internalName);
            return node->text != NULL;
        }
        dottedName = xl6ImportsNameMapRewriteDotted(map, node->text);
        if ( dottedName != NULL ) {
            xrtFree(node->text);
            node->text = dottedName;
        }
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && node->text != NULL ) {
        char* dottedName;

        internalName = xl6ImportsNameMapFind(map, node->text);
        if ( internalName != NULL ) {
            xrtFree(node->text);
            node->text = xl6ImportsDupText(internalName);
            return node->text != NULL;
        }
        dottedName = xl6ImportsNameMapRewriteDotted(map, node->text);
        if ( dottedName != NULL ) {
            xrtFree(node->text);
            node->text = dottedName;
        }
    }

    switch ( node->kind ) {
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXPORT_FUNC:
        return xl6ImportsRewriteFunctionLike(node, map, scope, 1, 2, 3);
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_NATIVE_EXTERN_FUNC:
        return xl6ImportsRewriteFunctionLike(node, map, scope, 1, 2, (size_t)-1);
    case XL6_AST_GET:
    case XL6_AST_LET:
        return xl6ImportsRewriteFunctionLike(node, map, scope, 1, 2, 3);
    case XL6_AST_CREATE:
        return xl6ImportsRewriteFunctionLike(node, map, scope, 0, (size_t)-1, 1);
    case XL6_AST_NATIVE_EXTERN_SPECIAL:
        return xl6ImportsRewriteFunctionLike(node, map, scope, 1, (size_t)-1, (size_t)-1);
    case XL6_AST_ANON_FUNC:
        return xl6ImportsRewriteFunctionLike(node, map, scope, 0, 1, 2);
    case XL6_AST_BLOCK:
        return xl6ImportsRewriteBlock(node, map, scope);
    default:
        break;
    }

    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6ImportsRewriteChildScoped(node->children[i], map, scope) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsRewriteNamesWithMap(XL6_AstNode* node, const XL6_ImportNameMap* map)
{
    int ok;
    XL6_PackageStringList scope;

    xl6PackageStringListInit(&scope);
    ok = xl6ImportsRewriteNamesScoped(node, map, &scope);
    xl6PackageStringListFree(&scope);
    return ok;
}

static int xl6ImportsPrepareSourceModule(
    XL6_AstNode* moduleRoot,
    const char* ns,
    XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags)
{
    XL6_ImportNameMap map;
    size_t i;
    int ok;

    if ( moduleRoot == NULL || symbols == NULL || ns == NULL || ns[0] == '\0' ) {
        return 1;
    }
    xl6ImportsNameMapInit(&map);
    ok = 1;

    for ( i = 0; ok && i < moduleRoot->childCount; ++i ) {
        XL6_AstNode* child = moduleRoot->children[i];
        const char* oldName;
        char* oldNameCopy;
        char* publicName;
        char* internalName;

        if ( !xl6ImportsDeclCanBeRenamed(child) ) {
            continue;
        }
        oldName = xl6ImportsDeclName(child);
        if ( oldName == NULL || oldName[0] == '\0' ) {
            continue;
        }

        oldNameCopy = xl6ImportsDupText(oldName);
        if ( oldNameCopy == NULL ) {
            ok = 0;
            break;
        }
        publicName = xl6ImportsPublicNameFromInternal(ns, oldNameCopy);
        if ( publicName == NULL && xl6ImportsIsInternalImportName(oldNameCopy) ) {
            /*
             * import 展开会把被导入包的声明追加到当前源码 AST 中。
             * 这些声明已经在各自命名空间下完成内部命名，不能再按当前包重命名。
             * 但包实现源码可以显式 import 依赖包，并复用依赖声明短名。
             * 这些短名只在当前包源码重写阶段生效，不进入最终用户的全局环境。
             */
            {
                char* importedName = xl6ImportsImportedPublicNameForInternal(moduleRoot, oldNameCopy);
                if ( importedName != NULL ) {
                    if ( !xl6ImportsModuleHasNaturalDeclName(moduleRoot, importedName) ) {
                        ok = xl6ImportsNameMapAdd(&map, importedName, oldNameCopy);
                    }
                    xrtFree(importedName);
                }
            }
            xrtFree(oldNameCopy);
            continue;
        }
        internalName = publicName != NULL ? xl6ImportsDupText(oldNameCopy) : xl6ImportsMakeInternalName(ns, oldNameCopy);
        if ( publicName == NULL &&
             internalName != NULL &&
             !xl6ImportsTextEquals(oldNameCopy, internalName) &&
             xl6ImportsModuleHasDeclName(moduleRoot, internalName) &&
             !xl6ImportsTextEquals(xl6ImportsNameMapFind(&map, oldNameCopy), internalName) ) {
            xrtFree(internalName);
            internalName = xl6ImportsMakeLocalInternalName(ns, oldNameCopy);
        }
        if ( internalName == NULL ) {
            xrtFree(oldNameCopy);
            xrtFree(publicName);
            ok = 0;
            break;
        }
        ok = xl6ImportsNameMapAdd(&map, oldNameCopy, internalName);
        if ( ok && !xl6ImportsTextEquals(oldNameCopy, internalName) ) {
            xl6ImportsSetDeclName(child, internalName);
            if ( xl6ImportsDeclName(child) == NULL ) {
                ok = 0;
            }
        }
        if ( ok && xl6ImportsIsModulePublicDecl(child) ) {
            ok = xl6ImportsSymbolTableAdd(symbols, ns, oldNameCopy, internalName, child->kind, 1);
        }
        if ( ok && publicName != NULL ) {
            ok = xl6ImportsSymbolTableAdd(symbols, ns, publicName, internalName, child->kind, 1);
        }
        xrtFree(oldNameCopy);
        xrtFree(publicName);
        xrtFree(internalName);
    }

    if ( ok ) {
        ok = xl6ImportsRewriteNamesWithMap(moduleRoot, &map);
    }
    if ( !ok && diags != NULL ) {
        xl6DiagAdd(diags, 1, 1, "source import symbol rewrite failed");
    }
    xl6ImportsNameMapFree(&map);
    return ok;
}

static int xl6ImportsSymbolTableAdd(
    XL6_ImportSymbolTable* table,
    const char* ns,
    const char* name,
    const char* internalName,
    XL6_AstKind kind,
    int supported)
{
    XL6_ImportSymbol* newItems;
    XL6_ImportSymbol* item;

    if ( table == NULL || ns == NULL || name == NULL ) {
        return 0;
    }
    if ( xl6ImportsFindSymbol(table, ns, name) != NULL ) {
        return 1;
    }
    newItems = (XL6_ImportSymbol*)xrtRealloc(table->items, (table->count + 1) * sizeof(XL6_ImportSymbol));
    if ( newItems == NULL ) {
        return 0;
    }
    table->items = newItems;
    item = &table->items[table->count];
    memset(item, 0, sizeof(*item));
    item->ns = xl6ImportsDupText(ns);
    item->name = xl6ImportsDupText(name);
    item->internalName = xl6ImportsDupText(internalName != NULL ? internalName : "");
    item->kind = kind;
    item->supported = supported;
    if ( item->ns == NULL || item->name == NULL || item->internalName == NULL ) {
        xrtFree(item->ns);
        xrtFree(item->name);
        xrtFree(item->internalName);
        memset(item, 0, sizeof(*item));
        return 0;
    }
    table->count += 1;
    return 1;
}

static int xl6ImportsPublishNamespaceAlias(
    XL6_ImportSymbolTable* table,
    const char* sourceNs,
    const char* alias)
{
    size_t i;
    size_t sourceCount;

    if ( table == NULL || sourceNs == NULL || alias == NULL || alias[0] == '\0' ) {
        return 0;
    }
    if ( xl6ImportsTextEquals(sourceNs, alias) ) {
        return xl6ImportsSymbolTableAddNamespace(table, sourceNs, 0);
    }
    if ( xl6ImportsNamespaceExists(table, alias) ) {
        return 0;
    }
    if ( !xl6ImportsSymbolTableAddNamespace(table, alias, 0) ) {
        return 0;
    }
    sourceCount = table->count;
    for ( i = 0; i < sourceCount; ++i ) {
        XL6_ImportSymbol* symbol = &table->items[i];

        if ( xl6ImportsTextEquals(symbol->ns, sourceNs) &&
             !xl6ImportsSymbolTableAdd(
                 table,
                 alias,
                 symbol->name,
                 symbol->internalName,
                 symbol->kind,
                 symbol->supported) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsBindSelectiveSymbols(
    XL6_AstNode* root,
    XL6_AstNode* importNode,
    const char* ns,
    const XL6_ImportSymbolTable* symbols,
    XL6_ImportNameMap* map,
    XL6_DiagList* diags)
{
    size_t i;

    if ( importNode == NULL || ns == NULL || symbols == NULL || map == NULL ) {
        return 0;
    }
    for ( i = 0; i < importNode->childCount; ++i ) {
        XL6_AstNode* selected = importNode->children[i];
        XL6_AstNode* aliasNode;
        const XL6_ImportSymbol* symbol;
        const char* localName;
        char message[512];

        if ( selected == NULL || selected->kind != XL6_AST_NAME ||
             selected->text == NULL || selected->text[0] == '\0' ) {
            return 0;
        }
        aliasNode = selected->childCount > 0 ? selected->children[0] : NULL;
        localName = aliasNode != NULL && aliasNode->kind == XL6_AST_NAME ?
                    aliasNode->text : selected->text;
        symbol = xl6ImportsFindSymbol(symbols, ns, selected->text);
        if ( symbol == NULL ) {
            snprintf(message,
                     sizeof(message),
                     "cannot import private or missing symbol: %s.%s",
                     ns,
                     selected->text);
            xl6DiagAddFile(diags, xl6AstSourcePath(selected), selected->line, selected->column, message);
            return 0;
        }
        if ( !symbol->supported ) {
            snprintf(message,
                     sizeof(message),
                     "import symbol cannot enter C ABI bridge yet: %s.%s",
                     ns,
                     selected->text);
            xl6DiagAddFile(diags, xl6AstSourcePath(selected), selected->line, selected->column, message);
            return 0;
        }
        if ( xl6ImportsNameMapFind(map, localName) != NULL ||
             xl6ImportsNamespaceIsPublic(symbols, localName) ||
             xl6TypeRegistryIsBuiltinName(localName) ||
             xl6ImportsModuleHasNaturalDeclName(root, localName) ) {
            snprintf(message,
                     sizeof(message),
                     "selective import name conflicts with an existing declaration: %s",
                     localName);
            xl6DiagAddFile(diags, xl6AstSourcePath(selected), selected->line, selected->column, message);
            return 0;
        }
        if ( !xl6ImportsNameMapAdd(map, localName, symbol->internalName) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsRewriteInitialChildren(
    XL6_AstNode* root,
    size_t initialCount,
    const XL6_ImportNameMap* map)
{
    size_t i;

    if ( root == NULL || map == NULL ) {
        return 0;
    }
    for ( i = 0; i < initialCount && i < root->childCount; ++i ) {
        if ( !xl6ImportsRewriteNamesWithMap(root->children[i], map) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsMoveAppendedChildrenBeforeBody(XL6_AstNode* root, size_t initialCount, XL6_DiagList* diags)
{
    size_t insertAt;
    size_t bodyCount;
    size_t appendedCount;
    XL6_AstNode** appended;

    if ( root == NULL || root->childCount <= initialCount ) {
        return 1;
    }
    insertAt = 0;
    while ( insertAt < initialCount &&
            root->children[insertAt] != NULL &&
            root->children[insertAt]->kind == XL6_AST_IMPORT ) {
        insertAt += 1;
    }
    if ( insertAt >= initialCount ) {
        return 1;
    }

    bodyCount = initialCount - insertAt;
    appendedCount = root->childCount - initialCount;
    appended = (XL6_AstNode**)xrtMalloc(appendedCount * sizeof(*appended));
    if ( appended == NULL ) {
        if ( diags != NULL ) {
            xl6DiagAdd(diags, 1, 1, "import expansion reorder failed: out of memory");
        }
        return 0;
    }
    memcpy(appended, root->children + initialCount, appendedCount * sizeof(*appended));
    memmove(root->children + insertAt + appendedCount,
            root->children + insertAt,
            bodyCount * sizeof(*root->children));
    memcpy(root->children + insertAt, appended, appendedCount * sizeof(*root->children));
    xrtFree(appended);
    return 1;
}

static int xl6ImportsMergePackageSources(
    XL6_AstNode* root,
    const XL6_PackageManifest* manifest,
    XL6_DiagList* diags,
    XL6_ImportDeps* deps)
{
    size_t i;

    if ( root == NULL || manifest == NULL ) {
        return 1;
    }
    for ( i = 0; i < manifest->xlSources.count; ++i ) {
        char sourcePath[512];
        XL6_ParseResult result;
        int ok;

        if ( !xl6PackageJoinPath(sourcePath, sizeof(sourcePath), manifest->dir, manifest->xlSources.items[i]) ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags, 1, 1, "package xlSources path is too long");
            }
            return 0;
        }
        int exists = xl6SourceOverlayExists(deps ? deps->sourceOverlay : NULL, sourcePath);
        if ( exists <= 0 ) {
            if ( diags != NULL ) {
                char message[640];
                snprintf(message, sizeof(message), exists < 0 ?
                    "cannot resolve package xlSources snapshot: %s" :
                    "package xlSources file is missing: %s", sourcePath);
                xl6DiagAdd(diags, 1, 1, message);
            }
            return 0;
        }

        xl6ParseResultInit(&result);
        ok = xl6SourceOverlayParse(deps ? deps->sourceOverlay : NULL, sourcePath, &result);
        if ( ok ) {
            ok = xl6ImportsExpandModule(result.root, sourcePath, diags, deps);
        }
        if ( ok && result.root != NULL ) {
            xl6AstMoveChildren(root, result.root);
        } else if ( diags != NULL && result.diags.count == 0 ) {
            xl6DiagAdd(diags, 1, 1, "package xlSources import failed");
        }
        if ( result.diags.count > 0 && diags != NULL ) {
            size_t diagIndex;
            for ( diagIndex = 0; diagIndex < result.diags.count; ++diagIndex ) {
                xl6DiagAddFile(diags, result.diags.items[diagIndex].file,
                           result.diags.items[diagIndex].line,
                           result.diags.items[diagIndex].column,
                           result.diags.items[diagIndex].message);
            }
        }
        xl6ParseResultFree(&result);
        if ( !ok ) {
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsCopySourceDiagnostics(XL6_DiagList* dst, const XL6_DiagList* src)
{
    size_t i;

    if ( dst == NULL || src == NULL ) {
        return 1;
    }
    for ( i = 0; i < src->count; ++i ) {
        xl6DiagAddFile(dst, src->items[i].file, src->items[i].line, src->items[i].column, src->items[i].message);
    }
    return 1;
}

static void xl6ImportsNormalizeNamespaceText(char* text)
{
    char* p;
    char* dot;

    if ( text == NULL ) {
        return;
    }
    for ( p = text; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' ) {
            *p = '.';
        }
    }
    dot = strrchr(text, '.');
    if ( dot != NULL && xl6ImportsTextEquals(dot, ".xl") ) {
        *dot = '\0';
    }
}

static void xl6ImportsNamespaceFromTarget(char* out, size_t outSize, const char* target)
{
    const char* start;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    out[0] = '\0';
    start = target != NULL && target[0] != '\0' ? target : "module";
    while ( start[0] == '.' || start[0] == '\\' || start[0] == '/' ) {
        start += 1;
    }
    len = strlen(start);
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, start, len);
    out[len] = '\0';
    xl6ImportsNormalizeNamespaceText(out);
}

static int xl6ImportsTrySourcePath(const XL6_ImportDeps* deps, char* out, size_t outSize, const char* path)
{
    int exists;
    if ( out == NULL || outSize == 0 || path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( xl6ImportsEndsWithNoCase(path, ".xl") ) {
        exists = xl6SourceOverlayExists(deps ? deps->sourceOverlay : NULL, path);
        if (exists < 0) return -1;
        if ( exists ) {
            if (strlen(path) >= outSize) return 0;
            snprintf(out, outSize, "%s", path);
            return 1;
        }
        return 0;
    }
    if (strlen(path) + 3 >= outSize) return 0;
    if ( snprintf(out, outSize, "%s.xl", path) <= 0 ) {
        return 0;
    }
    return xl6SourceOverlayExists(deps ? deps->sourceOverlay : NULL, out);
}

static int xl6ImportsResolveSourcePath(const XL6_ImportDeps* deps, const char* sourcePath, const char* target, char* out, size_t outSize)
{
    char importerDir[512];
    char path[512];
    char dotted[512];
    size_t i;
    int status;

    if ( out == NULL || outSize == 0 || target == NULL || target[0] == '\0' ) {
        return 0;
    }
    out[0] = '\0';
    if (strlen(target) >= sizeof(dotted)) return 0;
    if ( xl6ImportsPathIsAbsolute(target) ) {
        return xl6ImportsTrySourcePath(deps, out, outSize, target);
    }

    /*
     * 源码 import 只从当前源文件所在目录出发解析。
     * 不向上查找 .xlp，避免隐藏工程边界。
     */
    snprintf(importerDir, sizeof(importerDir), ".");
    if ( sourcePath != NULL ) {
        const char* p;
        size_t len = 0;
        for ( p = sourcePath; *p != '\0'; ++p ) {
            if ( *p == '\\' || *p == '/' ) {
                len = (size_t)(p - sourcePath + 1);
            }
        }
        if (len >= sizeof(importerDir)) return 0;
        if ( len > 0 && len < sizeof(importerDir) ) {
            memcpy(importerDir, sourcePath, len);
            importerDir[len] = '\0';
        }
    }
    if ( xl6PackageJoinPath(path, sizeof(path), importerDir, target) ) {
        status = xl6ImportsTrySourcePath(deps, out, outSize, path);
        if (status != 0) return status;
    }
    snprintf(dotted, sizeof(dotted), "%s", target);
    for ( i = 0; dotted[i] != '\0'; ++i ) {
        if ( dotted[i] == '.' ) {
            dotted[i] = '\\';
        }
    }
    if ( xl6PackageJoinPath(path, sizeof(path), importerDir, dotted) ) {
        status = xl6ImportsTrySourcePath(deps, out, outSize, path);
        if (status != 0) return status;
    }
    return xl6ImportsTrySourcePath(deps, out, outSize, target);
}

static int xl6ImportsMergeSourceFile(
    XL6_AstNode* root,
    const char* sourcePath,
    const char* ns,
    XL6_DiagList* diags,
    XL6_ImportDeps* deps,
    XL6_ImportSymbolTable* symbols)
{
    XL6_ParseResult result;
    size_t initialDiagCount;
    int ok;

    if ( root == NULL || sourcePath == NULL || ns == NULL || symbols == NULL ) {
        return 1;
    }
    initialDiagCount = diags != NULL ? diags->count : 0;
    xl6ParseResultInit(&result);
    ok = xl6SourceOverlayParse(deps ? deps->sourceOverlay : NULL, sourcePath, &result);
    if ( ok ) {
        ok = xl6ImportsExpandModule(result.root, sourcePath, diags, deps);
    }
    if ( ok ) {
        ok = xl6ImportsPrepareSourceModule(result.root, ns, symbols, diags);
    }
    if ( ok && result.root != NULL ) {
        xl6ImportsDropDuplicateImportedDecls(root, result.root);
        xl6AstMoveChildren(root, result.root);
    } else if ( diags != NULL &&
                result.diags.count == 0 &&
                diags->count == initialDiagCount ) {
        xl6DiagAdd(diags, 1, 1, "source import failed");
    }
    xl6ImportsCopySourceDiagnostics(diags, &result.diags);
    xl6ParseResultFree(&result);
    return ok;
}

static int xl6ImportsMergeManifestSourcesAsModule(
    XL6_AstNode* root,
    const XL6_PackageManifest* manifest,
    const char* ns,
    XL6_DiagList* diags,
    XL6_ImportDeps* deps,
    XL6_ImportSymbolTable* symbols)
{
    size_t i;
    int ok;
    XL6_AstNode* moduleRoot;

    if ( root == NULL || manifest == NULL || symbols == NULL ) {
        return 1;
    }
    moduleRoot = xl6AstNew(XL6_AST_MODULE, NULL);
    if ( moduleRoot == NULL ) {
        return 0;
    }
    ok = 1;
    for ( i = 0; ok && i < manifest->xlSources.count; ++i ) {
        char path[512];
        XL6_ParseResult result;
        size_t initialDiagCount;

        if ( !xl6PackageJoinPath(path, sizeof(path), manifest->dir, manifest->xlSources.items[i]) ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags, 1, 1, "package xlSources path is too long");
            }
            ok = 0;
            break;
        }
        initialDiagCount = diags != NULL ? diags->count : 0;
        xl6ParseResultInit(&result);
        ok = xl6SourceOverlayParse(deps ? deps->sourceOverlay : NULL, path, &result);
        if ( ok ) {
            ok = xl6ImportsExpandModule(result.root, path, diags, deps);
        }
        if ( ok && result.root != NULL ) {
            /*
             * 一个 manifest 的所有 xlSources 共同组成一个模块。
             * 先合并原始声明，最后统一建立私有名称和导出映射。
             */
            xl6ImportsDropDuplicateImportedDecls(moduleRoot, result.root);
            xl6AstMoveChildren(moduleRoot, result.root);
        } else if ( diags != NULL &&
                    result.diags.count == 0 &&
                    diags->count == initialDiagCount ) {
            xl6DiagAdd(diags, 1, 1, "package xlSources import failed");
        }
        xl6ImportsCopySourceDiagnostics(diags, &result.diags);
        xl6ParseResultFree(&result);
    }
    if ( ok ) {
        ok = xl6ImportsPrepareSourceModule(moduleRoot, ns, symbols, diags);
    }
    if ( ok ) {
        xl6ImportsDropDuplicateImportedDecls(root, moduleRoot);
        xl6AstMoveChildren(root, moduleRoot);
    }
    xl6AstFree(moduleRoot);
    return ok;
}

static int xl6ImportsLoadOne(
    XL6_AstNode* root,
    const char* sourcePath,
    XL6_AstNode* importNode,
    XL6_DiagList* diags,
    XL6_ImportDeps* deps,
    XL6_ImportSymbolTable* symbols,
    int exposeNamespace,
    char** outNamespace,
    int* outBuiltin)
{
    XL6_PackageResolveResult resolved;
    XL6_PackageManifest manifest;
    char xlSourcePath[512];
    char nsBuffer[256];
    char* target;
    const char* ns;
    int hasManifest;
    int wasLoaded;
    int ok;

    if ( outNamespace != NULL ) {
        *outNamespace = NULL;
    }
    if ( outBuiltin != NULL ) {
        *outBuiltin = 0;
    }
    target = xl6ImportsDupImportTarget(importNode != NULL ? importNode->text : NULL);
    if ( target == NULL ) {
        return 0;
    }

    if ( xl6ImportsTryBuiltinFeatureImport(target, deps) ) {
        if ( outNamespace != NULL ) {
            *outNamespace = xl6ImportsDupText(target);
            if ( *outNamespace == NULL ) {
                xrtFree(target);
                return 0;
            }
        }
        if ( outBuiltin != NULL ) {
            *outBuiltin = 1;
        }
        xrtFree(target);
        return 1;
    }

    const XL_CompiledPackage* boundPackage = NULL;
    for (size_t p = 0; deps && deps->compiledPackages && p < deps->compiledPackages->count; ++p)
        if (!strcmp(target, deps->compiledPackages->items[p].name)) {
            boundPackage = &deps->compiledPackages->items[p]; break;
        }
    if (boundPackage) {
        XL6_AstNode* interfaceRoot;
        deps->features |= XL6_IMPORT_FEATURE_COMPILED_PACKAGE_CONSUMER;
        if (outNamespace && !(*outNamespace = xl6ImportsDupText(target))) { xrtFree(target); return 0; }
        wasLoaded = xl6ImportsNamespaceIsLoaded(symbols, target);
        ok = xl6ImportsSymbolTableAddNamespace(symbols, target, exposeNamespace ? 0 : 1);
        if (ok && !wasLoaded) {
            interfaceRoot = xl6CompiledPackageDecode(boundPackage->interfaceData, boundPackage->interfaceSize);
            if (!interfaceRoot) ok = 0;
            else {
                char virtualPath[512];
                snprintf(virtualPath, sizeof(virtualPath), "__xl_compiled__/%s.xl", target);
                if(xl6ImportsPathListContains(&deps->activeSources,virtualPath)) {
                    xl6DiagAdd(diags,1,1,"compiled package interface import cycle");ok=0;
                } else if(!xl6PackageStringListAppend(&deps->activeSources,virtualPath))ok=0;
                else {
                    unsigned nested=deps->features & XL6_IMPORT_FEATURE_COMPILED_INTERFACE_EXPANSION;
                    deps->features |= XL6_IMPORT_FEATURE_COMPILED_INTERFACE_EXPANSION;
                    /* No source path for expansion: virtual interfaces have
                     * no filesystem companions or relative source imports. */
                    ok = xl6AstSetSourceTree(interfaceRoot, virtualPath) &&
                        xl6ImportsExpandModule(interfaceRoot, NULL, diags, deps);
                    deps->features=(deps->features & ~XL6_IMPORT_FEATURE_COMPILED_INTERFACE_EXPANSION)|nested;
                    xl6ImportsStringListPop(&deps->activeSources);
                }
                /* Names inside this interface are already canonical. Publish
                 * only its public surface; private recipe dependencies must
                 * not become package members when the interface is imported. */
                for (size_t n = 0; ok && n < interfaceRoot->childCount; ++n) {
                    XL6_AstNode* declaration = interfaceRoot->children[n];
                    if (!declaration || !(declaration->flags & XL6_AST_FLAG_PUBLIC)) continue;
                    const char* internalName = xl6ImportsDeclName(declaration);
                    char* publicName = xl6ImportsPublicNameFromInternal(target, internalName);
                    if (!publicName) continue;
                    ok = xl6ImportsSymbolTableAdd(symbols, target, publicName, internalName, declaration->kind, 1);
                    xrtFree(publicName);
                }
                if (ok) {
                    xl6ImportsDropDuplicateImportedDecls(root, interfaceRoot);
                    size_t total=root->childCount+interfaceRoot->childCount;
                    if(total<root->childCount || total>SIZE_MAX/sizeof(*root->children))ok=0;
                    else {
                        XL6_AstNode** children=root->children;
                        if(total>root->childCapacity)children=(XL6_AstNode**)xrtRealloc(children,total*sizeof(*children));
                        if(total && !children)ok=0;
                        else {
                            root->children=children;
                            if(total>root->childCapacity)root->childCapacity=total;
                            if(interfaceRoot->childCount)memcpy(children+root->childCount,
                                interfaceRoot->children,interfaceRoot->childCount*sizeof(*children));
                            root->childCount=total;interfaceRoot->childCount=0;
                        }
                    }
                }
                xl6AstFree(interfaceRoot);
            }
        }
        if (ok) xl6ImportsNamespaceMarkLoaded(symbols, target);
        else xl6DiagAddFile(diags, sourcePath, importNode->line, importNode->column,
            "cannot import registered compiled package interface");
        xrtFree(target);
        return ok;
    }

    if(deps && (deps->features & XL6_IMPORT_FEATURE_COMPILED_INTERFACE_EXPANSION)) {
        xl6DiagAddFile(diags,xl6AstSourcePath(importNode),importNode->line,importNode->column,
            "compiled package dependency is not registered; filesystem fallback is forbidden");
        xrtFree(target);return 0;
    }

    xl6PackageResolveImportObserved(sourcePath, target, &resolved,
                                    deps && deps->sourceOverlay ? deps->sourceOverlay->report : NULL);
    if ( resolved.resolved ) {
        xl6PackageManifestInit(&manifest);
        hasManifest = resolved.manifestPath != NULL && resolved.manifestPath[0] != '\0' &&
                      xl6PackageLoadManifestObserved(resolved.manifestPath, &manifest,
                                                    deps && deps->sourceOverlay ? deps->sourceOverlay->report : NULL);
        if ( !hasManifest ) {
            if ( diags != NULL ) {
                xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                           importNode != NULL ? importNode->column : 1,
                           "import package requires xlang.package.json with xlSources");
            }
            xl6PackageManifestFree(&manifest);
            xl6PackageResolveResultFree(&resolved);
            xrtFree(target);
            return 0;
        }
        ns = resolved.packageName != NULL && resolved.packageName[0] != '\0' ?
             resolved.packageName : target;
        wasLoaded = xl6ImportsNamespaceIsLoaded(symbols, ns);
        if ( outNamespace != NULL ) {
            *outNamespace = xl6ImportsDupText(ns);
        }
        if ( outNamespace != NULL && *outNamespace == NULL ) {
            ok = 0;
        } else if ( !xl6ImportsSymbolTableAddNamespace(symbols, ns, exposeNamespace ? 0 : 1) ) {
            ok = 0;
        } else if ( manifest.xlSources.count == 0 ) {
            if ( diags != NULL ) {
                xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                           importNode != NULL ? importNode->column : 1,
                           "import package has no xlSources");
            }
            ok = 0;
        } else {
            ok = 1;
        }
        if ( ok && !wasLoaded ) {
            ok = xl6ImportsCollectManifestDeps(deps, &manifest, importNode, diags);
        }
        if ( ok && !wasLoaded ) {
            ok = xl6ImportsCollectManifestDependencyGraph(deps, &manifest, importNode, diags, symbols);
        }
        if ( ok && !wasLoaded ) {
            ok = xl6ImportsMergeManifestSourcesAsModule(root, &manifest, ns, diags, deps, symbols);
        }
        if ( ok ) {
            xl6ImportsNamespaceMarkLoaded(symbols, ns);
        }
        xl6PackageManifestFree(&manifest);
        xl6PackageResolveResultFree(&resolved);
        xrtFree(target);
        return ok;
    }
    xl6PackageResolveResultFree(&resolved);

    int sourceStatus = xl6ImportsResolveSourcePath(deps, sourcePath, target, xlSourcePath, sizeof(xlSourcePath));
    if (sourceStatus < 0) {
        if (diags) xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
            importNode ? importNode->line : 1, importNode ? importNode->column : 1,
            "cannot resolve source import snapshot identity");
        xrtFree(target);
        return 0;
    }
    if ( sourceStatus > 0 ) {
        xl6ImportsNamespaceFromTarget(nsBuffer, sizeof(nsBuffer), target);
        ns = nsBuffer[0] != '\0' ? nsBuffer : target;
        wasLoaded = xl6ImportsNamespaceIsLoaded(symbols, ns);
        ok = 1;
        if ( outNamespace != NULL ) {
            *outNamespace = xl6ImportsDupText(ns);
        }
        if ( outNamespace != NULL && *outNamespace == NULL ) {
            ok = 0;
        } else if ( !xl6ImportsSymbolTableAddNamespace(symbols, ns, exposeNamespace ? 0 : 1) ) {
            ok = 0;
        }
        if ( ok && !wasLoaded ) {
            ok = xl6ImportsMergeSourceFile(root, xlSourcePath, ns, diags, deps, symbols);
        }
        if ( ok ) {
            xl6ImportsNamespaceMarkLoaded(symbols, ns);
        }
        xrtFree(target);
        return ok;
    }

    if ( diags != NULL ) {
        char message[1024];

        /*
            import 失败必须同时给出导入者与目标。嵌入式宿主通常会在独立
            工作目录中编译脚本，只有这两个信息才能可靠地区分路径、包和
            VFS 边界问题。
        */
        snprintf(message,
                 sizeof(message),
                 "cannot resolve import '%s' from '%s'",
                 target,
                 sourcePath != NULL && sourcePath[0] != '\0' ? sourcePath : "<source>");
        xl6DiagAddFile(diags, xl6AstSourcePath(importNode),
                           importNode != NULL ? importNode->line : 1,
                   importNode != NULL ? importNode->column : 1,
                   message);
    }
    xrtFree(target);
    return 0;
}

static void xl6ImportsRewriteMemberToName(XL6_AstNode* node, const char* name)
{
    size_t i;

    if ( node == NULL || name == NULL ) {
        return;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6AstFree(node->children[i]);
    }
    xrtFree(node->children);
    xrtFree(node->text);
    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;
    node->kind = XL6_AST_NAME;
    node->text = xl6ImportsDupText(name);
}

static int xl6ImportsTryRewriteQualifiedNamespaceRef(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    const XL6_PackageStringList* scope,
    XL6_DiagList* diags,
    int* outHandled)
{
    const char* parts[64];
    size_t count;
    char* ns;
    const XL6_ImportNamespace* nsInfo;
    const XL6_ImportSymbol* symbol;

    if ( outHandled != NULL ) {
        *outHandled = 0;
    }
    if ( node == NULL || node->kind != XL6_AST_MEMBER ) {
        return 1;
    }
    count = 0;
    if ( !xl6ImportsCollectMemberParts(node, parts, XL6_ARRAY_COUNT(parts), &count) || count < 2 ) {
        return 1;
    }
    /* 局部值名称优先于同名导入命名空间。 */
    if ( xl6ImportsScopeContains(scope, parts[0]) ) {
        return 1;
    }
    ns = xl6ImportsJoinParts(parts, count - 1);
    if ( ns == NULL ) {
        return 0;
    }
    nsInfo = xl6ImportsFindNamespace(symbols, ns);
    if ( nsInfo == NULL ) {
        xrtFree(ns);
        return 1;
    }
    if ( outHandled != NULL ) {
        *outHandled = 1;
    }
    symbol = xl6ImportsFindSymbol(symbols, ns, parts[count - 1]);
    if ( symbol == NULL ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column,
                       nsInfo->isPrivate ?
                           "private package dependency is not exported" :
                           "import package has no such exported symbol");
        }
        xrtFree(ns);
        return 0;
    }
    if ( !symbol->supported ) {
        if ( diags != NULL ) {
            char message[512];

            snprintf(message,
                     sizeof(message),
                     "import symbol cannot enter C ABI bridge yet: %s.%s",
                     symbol->ns != NULL ? symbol->ns : "",
                     symbol->name != NULL ? symbol->name : "");
            xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column, message);
        }
        xrtFree(ns);
        return 0;
    }
    xl6ImportsRewriteMemberToName(node, symbol->internalName);
    xrtFree(ns);
    return node->text != NULL;
}

static int xl6ImportsTryRewriteQualifiedTypeRef(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    int* outHandled)
{
    const char* dot;
    char* ns;
    const XL6_ImportNamespace* nsInfo;
    const XL6_ImportSymbol* symbol;

    if ( outHandled != NULL ) {
        *outHandled = 0;
    }
    if ( node == NULL ||
         (node->kind != XL6_AST_TYPE && node->kind != XL6_AST_NAME) ||
         node->text == NULL ) {
        return 1;
    }
    dot = strrchr(node->text, '.');
    if ( dot == NULL || dot == node->text || dot[1] == '\0' ) {
        return 1;
    }
    ns = xrtStrDupN(node->text, (size_t)(dot - node->text));
    if ( ns == NULL ) {
        return 0;
    }
    nsInfo = xl6ImportsFindNamespace(symbols, ns);
    if ( nsInfo == NULL ) {
        xrtFree(ns);
        return 1;
    }
    if ( outHandled != NULL ) {
        *outHandled = 1;
    }
    if ( nsInfo->isPrivate ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column, "private package dependency is not exported");
        }
        xrtFree(ns);
        return 0;
    }
    symbol = xl6ImportsFindSymbol(symbols, ns, dot + 1);
    if ( symbol == NULL ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column, "import package has no such exported type");
        }
        xrtFree(ns);
        return 0;
    }
    if ( !xl6ImportsSymbolIsType(symbol) ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column, "import symbol is not a type");
        }
        xrtFree(ns);
        return 0;
    }
    if ( !symbol->supported ) {
        if ( diags != NULL ) {
            xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column, "import type cannot enter C ABI bridge yet");
        }
        xrtFree(ns);
        return 0;
    }
    xrtFree(node->text);
    node->text = xl6ImportsDupText(symbol->internalName);
    xrtFree(ns);
    return node->text != NULL;
}

static int xl6ImportsRewriteNamespaceRefsScoped(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope);

static int xl6ImportsRewriteNamespaceChildScoped(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope);

static int xl6ImportsRewriteNamespaceFunctionLike(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope,
    size_t paramsIndex);

static int xl6ImportsRewriteNamespaceBlock(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope);

static int xl6ImportsRewriteNamespaceRefsScoped(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope)
{
    size_t i;
    int ok;
    int handled;

    if ( node == NULL ) {
        return 1;
    }
    if ( node->kind == XL6_AST_IMPORT ) {
        return 1;
    }
    handled = 0;
    if ( !xl6ImportsTryRewriteQualifiedTypeRef(node, symbols, diags, &handled) ) {
        return 0;
    }
    if ( handled ) {
        for ( i = 0; i < node->childCount; ++i ) {
            if ( !xl6ImportsRewriteNamespaceRefsScoped(node->children[i], symbols, diags, scope) ) {
                return 0;
            }
        }
        return 1;
    }
    handled = 0;
    if ( !xl6ImportsTryRewriteQualifiedNamespaceRef(node, symbols, scope, diags, &handled) ) {
        return 0;
    }
    if ( handled ) {
        return 1;
    }

    switch ( node->kind ) {
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_GET:
    case XL6_AST_LET:
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_NATIVE_EXTERN_FUNC:
        return xl6ImportsRewriteNamespaceFunctionLike(node, symbols, diags, scope, 1);
    case XL6_AST_CREATE:
    case XL6_AST_ANON_FUNC:
        return xl6ImportsRewriteNamespaceFunctionLike(node, symbols, diags, scope, 0);
    case XL6_AST_NATIVE_EXTERN_SPECIAL:
        return xl6ImportsRewriteNamespaceFunctionLike(node, symbols, diags, scope, 1);
    case XL6_AST_BLOCK:
        return xl6ImportsRewriteNamespaceBlock(node, symbols, diags, scope);
    default:
        break;
    }

    if ( node->kind == XL6_AST_CLASS && node->childCount > 1 ) {
        XL6_AstNode* body = node->children[1];
        XL6_PackageStringList classScope;

        if ( !xl6ImportsScopeCopy(&classScope, scope) ) {
            return 0;
        }
        ok = 1;
        for ( i = 0; body != NULL && i < body->childCount; ++i ) {
            XL6_AstNode* member = body->children[i];
            if ( member != NULL && member->kind == XL6_AST_FIELD &&
                 !xl6ImportsScopeAdd(&classScope, member->text) ) {
                ok = 0;
            }
        }
        for ( i = 0; ok && i < node->childCount; ++i ) {
            if ( node->children[i] != body &&
                 !xl6ImportsRewriteNamespaceRefsScoped(
                     node->children[i], symbols, diags, &classScope) ) {
                ok = 0;
            }
        }
        for ( i = 0; ok && body != NULL && i < body->childCount; ++i ) {
            if ( !xl6ImportsRewriteNamespaceRefsScoped(
                     body->children[i], symbols, diags, &classScope) ) {
                ok = 0;
            }
        }
        xl6PackageStringListFree(&classScope);
        return ok;
    }

    if ( node->kind == XL6_AST_FOR && node->childCount > 2 ) {
        XL6_PackageStringList loopScope;

        if ( !xl6ImportsRewriteNamespaceRefsScoped(
                 node->children[1], symbols, diags, scope) ||
             !xl6ImportsScopeCopy(&loopScope, scope) ) {
            return 0;
        }
        ok = xl6ImportsScopeAddForTargets(&loopScope, node->children[0]) &&
             xl6ImportsRewriteNamespaceRefsScoped(
                 node->children[2], symbols, diags, &loopScope);
        xl6PackageStringListFree(&loopScope);
        return ok;
    }

    if ( node->kind == XL6_AST_CATCH && node->childCount > 1 ) {
        XL6_AstNode* catchName = node->childCount == 3 ? node->children[1] : node->children[0];
        XL6_AstNode* catchBlock = node->childCount == 3 ? node->children[2] : node->children[1];
        XL6_PackageStringList catchScope;

        if ( !xl6ImportsScopeCopy(&catchScope, scope) ) {
            return 0;
        }
        ok = xl6ImportsScopeAdd(&catchScope, catchName != NULL ? catchName->text : NULL) &&
             xl6ImportsRewriteNamespaceRefsScoped(catchBlock, symbols, diags, &catchScope);
        xl6PackageStringListFree(&catchScope);
        return ok;
    }

    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         !xl6ImportsScopeContains(scope, node->children[0]->text) ) {
        const char* ns = node->children[0]->text;
        const XL6_ImportSymbol* symbol = xl6ImportsFindSymbol(symbols, ns, node->text);
        if ( symbol != NULL ) {
            if ( !symbol->supported ) {
                if ( diags != NULL ) {
                    char message[512];

                    snprintf(message,
                             sizeof(message),
                             "导入符号当前不能进入 C ABI bridge: %s.%s",
                             symbol->ns != NULL ? symbol->ns : "",
                             symbol->name != NULL ? symbol->name : "");
                    xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column, message);
                }
                return 0;
            }
            xl6ImportsRewriteMemberToName(node, symbol->internalName);
            return node->text != NULL;
        }
        if ( xl6ImportsNamespaceExists(symbols, ns) ) {
            const XL6_ImportNamespace* nsInfo = xl6ImportsFindNamespace(symbols, ns);
            if ( diags != NULL ) {
                xl6DiagAddFile(diags, xl6AstSourcePath(node), node->line, node->column,
                           nsInfo != NULL && nsInfo->isPrivate ?
                               "private package dependency is not exported" :
                               "import package has no such exported symbol");
            }
            return 0;
        }
    }

    ok = 1;
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6ImportsRewriteNamespaceChildScoped(
                 node->children[i], symbols, diags, scope) ) {
            ok = 0;
        }
    }
    return ok;
}

static int xl6ImportsRewriteNamespaceChildScoped(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope)
{
    int ok;
    XL6_PackageStringList childScope;

    if ( node == NULL ) {
        return 1;
    }
    if ( node->kind != XL6_AST_BLOCK ) {
        return xl6ImportsRewriteNamespaceRefsScoped(node, symbols, diags, scope);
    }
    if ( !xl6ImportsScopeCopy(&childScope, scope) ) {
        return 0;
    }
    ok = xl6ImportsRewriteNamespaceRefsScoped(node, symbols, diags, &childScope);
    xl6PackageStringListFree(&childScope);
    return ok;
}

static int xl6ImportsRewriteNamespaceFunctionLike(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope,
    size_t paramsIndex)
{
    size_t i;
    int ok;
    XL6_PackageStringList functionScope;

    if ( !xl6ImportsScopeCopy(&functionScope, scope) ) {
        return 0;
    }
    if ( paramsIndex < node->childCount &&
         !xl6ImportsScopeAddParams(&functionScope, node->children[paramsIndex]) ) {
        xl6PackageStringListFree(&functionScope);
        return 0;
    }
    ok = 1;
    for ( i = 0; ok && i < node->childCount; ++i ) {
        ok = xl6ImportsRewriteNamespaceChildScoped(
            node->children[i], symbols, diags, &functionScope);
    }
    xl6PackageStringListFree(&functionScope);
    return ok;
}

static int xl6ImportsRewriteNamespaceBlock(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags,
    XL6_PackageStringList* scope)
{
    size_t i;

    for ( i = 0; node != NULL && i < node->childCount; ++i ) {
        XL6_AstNode* child = node->children[i];

        if ( child == NULL ) {
            continue;
        }
        if ( child->kind == XL6_AST_VAR_DECL || child->kind == XL6_AST_CONST_DECL ) {
            size_t j;

            /* 声明初始化式在名称进入当前作用域之前解析。 */
            for ( j = 0; j < child->childCount; ++j ) {
                if ( !xl6ImportsRewriteNamespaceRefsScoped(
                         child->children[j], symbols, diags, scope) ) {
                    return 0;
                }
            }
            if ( !xl6ImportsScopeAdd(scope, child->text) ) {
                return 0;
            }
            continue;
        }
        if ( child->kind == XL6_AST_MULTI_VAR_DECL ) {
            size_t j;
            XL6_AstNode* names = child->childCount > 0 ? child->children[0] : NULL;

            if ( child->childCount > 1 &&
                 !xl6ImportsRewriteNamespaceRefsScoped(
                     child->children[1], symbols, diags, scope) ) {
                return 0;
            }
            for ( j = 0; names != NULL && j < names->childCount; ++j ) {
                if ( names->children[j] != NULL &&
                     !xl6ImportsScopeAdd(scope, names->children[j]->text) ) {
                    return 0;
                }
            }
            continue;
        }
        if ( !xl6ImportsRewriteNamespaceChildScoped(child, symbols, diags, scope) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6ImportsRewriteNamespaceRefs(
    XL6_AstNode* node,
    const XL6_ImportSymbolTable* symbols,
    XL6_DiagList* diags)
{
    int ok;
    XL6_PackageStringList scope;

    xl6PackageStringListInit(&scope);
    ok = xl6ImportsRewriteNamespaceRefsScoped(node, symbols, diags, &scope);
    xl6PackageStringListFree(&scope);
    return ok;
}

int xl6ImportsExpandModule(XL6_AstNode* root, const char* sourcePath, XL6_DiagList* diags, XL6_ImportDeps* deps)
{
    XL6_ImportDeps localDeps;
    XL6_ImportSymbolTable symbols;
    XL6_ImportNameMap directImports;
    char* activeSource;
    size_t i;
    size_t initialCount;
    int ownsDeps;
    int activeSourcePushed;
    int ok;
    const char* previousFile;

    if ( root == NULL ) {
        return 1;
    }
    previousFile = diags ? diags->defaultFile : NULL;
    if (diags) diags->defaultFile = sourcePath;
    ownsDeps = 0;
    activeSourcePushed = 0;
    activeSource = NULL;
    if ( deps == NULL ) {
        xl6ImportDepsInit(&localDeps);
        deps = &localDeps;
        ownsDeps = 1;
    }
    if ( sourcePath != NULL && sourcePath[0] != '\0' ) {
        activeSource = xrtPathAbs(sourcePath);
        if ( activeSource == NULL ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags, 1, 1, "cannot normalize source import path");
            }
            if ( ownsDeps ) {
                xl6ImportDepsFree(deps);
            }
            if (diags) diags->defaultFile = previousFile;
            return 0;
        }
        if ( xl6ImportsPathListContains(&deps->activeSources, activeSource) ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags, 1, 1, "source import cycle detected");
            }
            xrtFree(activeSource);
            if ( ownsDeps ) {
                xl6ImportDepsFree(deps);
            }
            if (diags) diags->defaultFile = previousFile;
            return 0;
        }
        if ( !xl6PackageStringListAppend(&deps->activeSources, activeSource) ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags, 1, 1, "cannot track source import path");
            }
            xrtFree(activeSource);
            if ( ownsDeps ) {
                xl6ImportDepsFree(deps);
            }
            if (diags) diags->defaultFile = previousFile;
            return 0;
        }
        activeSourcePushed = 1;
    }
    xl6ImportsAddSourceCompanionC(sourcePath, deps);
    xl6ImportsSymbolTableInit(&symbols);
    xl6ImportsNameMapInit(&directImports);
    initialCount = root->childCount;
    ok = 1;
    for ( i = 0; i < initialCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( child != NULL && child->kind == XL6_AST_IMPORT ) {
            int selective = (child->flags & XL6_AST_FLAG_IMPORT_SELECTIVE) != 0;
            XL6_AstNode* aliasNode = !selective && child->childCount > 0 ?
                                     child->children[0] : NULL;
            const char* alias = aliasNode != NULL && aliasNode->kind == XL6_AST_NAME ?
                                aliasNode->text : NULL;
            char* canonicalNs = NULL;
            int builtin = 0;

            if ( !xl6ImportsLoadOne(
                     root,
                     sourcePath,
                     child,
                     diags,
                     deps,
                     &symbols,
                     !selective && alias == NULL,
                     &canonicalNs,
                     &builtin) ) {
                ok = 0;
            } else if ( selective ) {
                if ( builtin ) {
                    xl6DiagAddFile(diags, xl6AstSourcePath(child), child->line, child->column,
                               "selective import requires a source or package module");
                    ok = 0;
                } else if ( !xl6ImportsBindSelectiveSymbols(
                                root,
                                child,
                                canonicalNs,
                                &symbols,
                                &directImports,
                                diags) ) {
                    ok = 0;
                }
            } else if ( alias != NULL ) {
                if ( xl6ImportsNameMapFind(&directImports, alias) != NULL ||
                     xl6ImportsNamespaceIsPublic(&symbols, alias) ||
                     xl6TypeRegistryIsBuiltinName(alias) ||
                     xl6ImportsModuleHasNaturalDeclName(root, alias) ) {
                    xl6DiagAddFile(diags, xl6AstSourcePath(aliasNode), aliasNode->line, aliasNode->column,
                               "import alias conflicts with an existing declaration");
                    ok = 0;
                } else if ( builtin ) {
                    if ( !xl6ImportsNameMapAdd(&directImports, alias, canonicalNs) ) {
                        ok = 0;
                    }
                } else if ( !xl6ImportsPublishNamespaceAlias(&symbols, canonicalNs, alias) ) {
                    xl6DiagAddFile(diags, xl6AstSourcePath(aliasNode), aliasNode->line, aliasNode->column,
                               "import alias conflicts with an existing namespace");
                    ok = 0;
                }
            } else if ( xl6ImportsNameMapFind(&directImports, canonicalNs) != NULL ||
                        xl6TypeRegistryIsBuiltinName(canonicalNs) ||
                        xl6ImportsModuleHasNaturalDeclName(root, canonicalNs) ) {
                xl6DiagAddFile(diags, xl6AstSourcePath(child), child->line, child->column,
                           "import namespace conflicts with an existing declaration");
                ok = 0;
            }
            xrtFree(canonicalNs);
        }
    }
    if ( ok ) {
        ok = xl6ImportsRewriteInitialChildren(root, initialCount, &directImports);
    }
    if ( ok ) {
        ok = xl6ImportsMoveAppendedChildrenBeforeBody(root, initialCount, diags);
    }
    if ( ok ) {
        ok = xl6ImportsRewriteNamespaceRefs(root, &symbols, diags);
    }
    xl6ImportsNameMapFree(&directImports);
    xl6ImportsSymbolTableFree(&symbols);
    if ( activeSourcePushed ) {
        xl6ImportsStringListPop(&deps->activeSources);
    }
    xrtFree(activeSource);
    if ( ownsDeps ) {
        xl6ImportDepsFree(deps);
    }
    if (diags) diags->defaultFile = previousFile;
    return ok;
}
