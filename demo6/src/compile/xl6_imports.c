#include "src/compile/xl6_imports.h"

#include "src/common/xl6_common.h"
#include "src/parser/xl6_parser_api.h"

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
    int supported;
} XL6_ImportSymbol;

typedef struct XL6_ImportNamespace {
    char* ns;
    int isPrivate;
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
    return xrtCopyStr((char*)src, strlen(src));
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
        builder->data = xrtCopyStr((char*)"", 0);
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
        return xrtCopyStr((char*)"", 0);
    }
    len = strlen(text);
    if ( len >= 2 && ((text[0] == '"' && text[len - 1] == '"') ||
                      (text[0] == '\'' && text[len - 1] == '\'')) ) {
        return xrtCopyStr((char*)text + 1, len - 2);
    }
    return xrtCopyStr((char*)text, len);
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
    return 0;
}

void xl6ImportDepsInit(XL6_ImportDeps* deps)
{
    if ( deps == NULL ) {
        return;
    }
    memset(deps, 0, sizeof(*deps));
    xl6PackageStringListInit(&deps->cSources);
    xl6PackageStringListInit(&deps->links);
    xl6PackageStringListInit(&deps->loadlibs);
}

void xl6ImportDepsFree(XL6_ImportDeps* deps)
{
    size_t i;

    if ( deps == NULL ) {
        return;
    }
    xl6PackageStringListFree(&deps->cSources);
    xl6PackageStringListFree(&deps->links);
    xl6PackageStringListFree(&deps->loadlibs);
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
    if ( xrtFileExists((char*)cPath) ) {
        (void)xl6ImportsStringListAppendUnique(&deps->cSources, cPath);
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
    size_t i;

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
                xl6DiagAdd(diags,
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
    if ( !xl6ImportsDepsAddPackageId(deps, manifest->packageId) ) {
        return 0;
    }
    for ( i = 0; i < manifest->cSources.count; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->cSources, manifest->dir, manifest->cSources.items[i], 1) ) {
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
            xl6DiagAdd(diags,
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
    XL6_DiagList* diags)
{
    XL6_PackageResolveResult candidate;
    char* versionedName;
    int sawMismatchedPackage;

    if ( manifest == NULL || dep == NULL || out == NULL ) {
        return 0;
    }
    xl6PackageResolveResultInit(out);
    sawMismatchedPackage = 0;
    xl6PackageResolveImport(manifest->dir, dep->name, &candidate);
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
            xl6DiagAdd(diags,
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
    xl6PackageResolveImport(manifest->dir, versionedName, &candidate);
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
            xl6DiagAdd(diags,
                       importNode != NULL ? importNode->line : 1,
                       importNode != NULL ? importNode->column : 1,
                       "package dependency version mismatch");
        }
        return 0;
    }
    if ( diags != NULL ) {
        xl6DiagAdd(diags,
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
        if ( !xl6ImportsResolveDependency(manifest, dep, &resolved, importNode, diags) ) {
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
                  xl6PackageLoadManifest(resolved->manifestPath, &manifest);
    if ( !hasManifest ) {
        if ( diags != NULL ) {
            xl6DiagAdd(diags,
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
        return 0;
    }
    table->namespaceCount += 1;
    return 1;
}

static int xl6ImportsNamespaceExists(const XL6_ImportSymbolTable* table, const char* ns)
{
    return xl6ImportsFindNamespace(table, ns) != NULL;
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
    return xrtCopyStr((char*)publicName, len);
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
    if ( node->kind == XL6_AST_NAME && node->text != NULL ) {
        if ( xl6ImportsScopeContains(scope, node->text) ) {
            return 1;
        }
        internalName = xl6ImportsNameMapFind(map, node->text);
        if ( internalName != NULL ) {
            xrtFree(node->text);
            node->text = xl6ImportsDupText(internalName);
            return node->text != NULL;
        }
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && node->text != NULL ) {
        internalName = xl6ImportsNameMapFind(map, node->text);
        if ( internalName != NULL ) {
            xrtFree(node->text);
            node->text = xl6ImportsDupText(internalName);
            return node->text != NULL;
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
            ok = xl6ImportsSymbolTableAdd(symbols, ns, oldNameCopy, internalName, 1);
        }
        if ( ok && publicName != NULL ) {
            ok = xl6ImportsSymbolTableAdd(symbols, ns, publicName, internalName, 1);
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
    item->supported = supported;
    if ( item->ns == NULL || item->name == NULL || item->internalName == NULL ) {
        return 0;
    }
    table->count += 1;
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
        if ( !xrtFileExists((char*)sourcePath) ) {
            if ( diags != NULL ) {
                char message[640];
                snprintf(message, sizeof(message), "package xlSources file is missing: %s", sourcePath);
                xl6DiagAdd(diags, 1, 1, message);
            }
            return 0;
        }

        xl6ParseResultInit(&result);
        ok = xl6ParseFile(sourcePath, &result);
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
                xl6DiagAdd(diags,
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
        xl6DiagAdd(dst, src->items[i].line, src->items[i].column, src->items[i].message);
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

static int xl6ImportsTrySourcePath(char* out, size_t outSize, const char* path)
{
    if ( out == NULL || outSize == 0 || path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( xl6ImportsEndsWithNoCase(path, ".xl") ) {
        if ( xrtFileExists((char*)path) ) {
            snprintf(out, outSize, "%s", path);
            return 1;
        }
        return 0;
    }
    if ( snprintf(out, outSize, "%s.xl", path) <= 0 ) {
        return 0;
    }
    if ( xrtFileExists((char*)out) ) {
        return 1;
    }
    return 0;
}

static int xl6ImportsResolveSourcePath(const char* sourcePath, const char* target, char* out, size_t outSize)
{
    char importerDir[512];
    char path[512];
    char dotted[512];
    size_t i;

    if ( out == NULL || outSize == 0 || target == NULL || target[0] == '\0' ) {
        return 0;
    }
    out[0] = '\0';
    if ( xl6ImportsPathIsAbsolute(target) ) {
        return xl6ImportsTrySourcePath(out, outSize, target);
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
        if ( len > 0 && len < sizeof(importerDir) ) {
            memcpy(importerDir, sourcePath, len);
            importerDir[len] = '\0';
        }
    }
    if ( xl6PackageJoinPath(path, sizeof(path), importerDir, target) &&
         xl6ImportsTrySourcePath(out, outSize, path) ) {
        return 1;
    }
    snprintf(dotted, sizeof(dotted), "%s", target);
    for ( i = 0; dotted[i] != '\0'; ++i ) {
        if ( dotted[i] == '.' ) {
            dotted[i] = '\\';
        }
    }
    if ( xl6PackageJoinPath(path, sizeof(path), importerDir, dotted) &&
         xl6ImportsTrySourcePath(out, outSize, path) ) {
        return 1;
    }
    return xl6ImportsTrySourcePath(out, outSize, target);
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
    int ok;

    if ( root == NULL || sourcePath == NULL || ns == NULL || symbols == NULL ) {
        return 1;
    }
    xl6ParseResultInit(&result);
    ok = xl6ParseFile(sourcePath, &result);
    if ( ok ) {
        ok = xl6ImportsExpandModule(result.root, sourcePath, diags, deps);
    }
    if ( ok ) {
        ok = xl6ImportsPrepareSourceModule(result.root, ns, symbols, diags);
    }
    if ( ok && result.root != NULL ) {
        xl6ImportsDropDuplicateImportedDecls(root, result.root);
        xl6AstMoveChildren(root, result.root);
    } else if ( diags != NULL && result.diags.count == 0 ) {
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

    if ( root == NULL || manifest == NULL || symbols == NULL ) {
        return 1;
    }
    ok = 1;
    for ( i = 0; ok && i < manifest->xlSources.count; ++i ) {
        char path[512];

        if ( !xl6PackageJoinPath(path, sizeof(path), manifest->dir, manifest->xlSources.items[i]) ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags, 1, 1, "package xlSources path is too long");
            }
            return 0;
        }
        ok = xl6ImportsMergeSourceFile(root, path, ns, diags, deps, symbols);
    }
    return ok;
}

static int xl6ImportsLoadOne(
    XL6_AstNode* root,
    const char* sourcePath,
    XL6_AstNode* importNode,
    XL6_DiagList* diags,
    XL6_ImportDeps* deps,
    XL6_ImportSymbolTable* symbols)
{
    XL6_PackageResolveResult resolved;
    XL6_PackageManifest manifest;
    char xlSourcePath[512];
    char nsBuffer[256];
    char* target;
    const char* ns;
    int hasManifest;
    int ok;

    target = xl6ImportsDupImportTarget(importNode != NULL ? importNode->text : NULL);
    if ( target == NULL ) {
        return 0;
    }

    if ( xl6ImportsTryBuiltinFeatureImport(target, deps) ) {
        xrtFree(target);
        return 1;
    }

    xl6PackageResolveImport(sourcePath, target, &resolved);
    if ( resolved.resolved ) {
        xl6PackageManifestInit(&manifest);
        hasManifest = resolved.manifestPath != NULL && resolved.manifestPath[0] != '\0' &&
                      xl6PackageLoadManifest(resolved.manifestPath, &manifest);
        if ( !hasManifest ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags,
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
        if ( xl6ImportsNamespaceExists(symbols, ns) &&
             xl6ImportsFindNamespace(symbols, ns) != NULL &&
             !xl6ImportsFindNamespace(symbols, ns)->isPrivate ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags,
                           importNode != NULL ? importNode->line : 1,
                           importNode != NULL ? importNode->column : 1,
                           "duplicate import namespace");
            }
            ok = 0;
        } else if ( !xl6ImportsSymbolTableAddNamespace(symbols, ns, 0) ) {
            ok = 0;
        } else if ( manifest.xlSources.count == 0 ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags,
                           importNode != NULL ? importNode->line : 1,
                           importNode != NULL ? importNode->column : 1,
                           "import package has no xlSources");
            }
            ok = 0;
        } else {
            ok = 1;
        }
        if ( ok ) {
            ok = xl6ImportsCollectManifestDeps(deps, &manifest, importNode, diags);
        }
        if ( ok ) {
            ok = xl6ImportsCollectManifestDependencyGraph(deps, &manifest, importNode, diags, symbols);
        }
        if ( ok ) {
            ok = xl6ImportsMergeManifestSourcesAsModule(root, &manifest, ns, diags, deps, symbols);
        }
        xl6PackageManifestFree(&manifest);
        xl6PackageResolveResultFree(&resolved);
        xrtFree(target);
        return ok;
    }
    xl6PackageResolveResultFree(&resolved);

    if ( xl6ImportsResolveSourcePath(sourcePath, target, xlSourcePath, sizeof(xlSourcePath)) ) {
        xl6ImportsNamespaceFromTarget(nsBuffer, sizeof(nsBuffer), target);
        ns = nsBuffer[0] != '\0' ? nsBuffer : target;
        ok = 1;
        if ( xl6ImportsNamespaceExists(symbols, ns) &&
             xl6ImportsFindNamespace(symbols, ns) != NULL &&
             !xl6ImportsFindNamespace(symbols, ns)->isPrivate ) {
            if ( diags != NULL ) {
                xl6DiagAdd(diags,
                           importNode != NULL ? importNode->line : 1,
                           importNode != NULL ? importNode->column : 1,
                           "duplicate import namespace");
            }
            ok = 0;
        } else if ( !xl6ImportsSymbolTableAddNamespace(symbols, ns, 0) ) {
            ok = 0;
        }
        if ( ok ) {
            ok = xl6ImportsMergeSourceFile(root, xlSourcePath, ns, diags, deps, symbols);
        }
        xrtFree(target);
        return ok;
    }

    if ( diags != NULL ) {
        xl6DiagAdd(diags,
                   importNode != NULL ? importNode->line : 1,
                   importNode != NULL ? importNode->column : 1,
                   "cannot resolve import");
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
            xl6DiagAdd(diags,
                       node->line,
                       node->column,
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
            xl6DiagAdd(diags, node->line, node->column, message);
        }
        xrtFree(ns);
        return 0;
    }
    xl6ImportsRewriteMemberToName(node, symbol->internalName);
    xrtFree(ns);
    return node->text != NULL;
}

static int xl6ImportsRewriteNamespaceRefs(XL6_AstNode* node, const XL6_ImportSymbolTable* symbols, XL6_DiagList* diags)
{
    size_t i;
    int ok;
    int handled;

    if ( node == NULL ) {
        return 1;
    }
    handled = 0;
    if ( !xl6ImportsTryRewriteQualifiedNamespaceRef(node, symbols, diags, &handled) ) {
        return 0;
    }
    if ( handled ) {
        return 1;
    }
    if ( node->kind == XL6_AST_MEMBER && node->childCount > 0 && node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME ) {
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
                    xl6DiagAdd(diags, node->line, node->column, message);
                }
                return 0;
            }
            xl6ImportsRewriteMemberToName(node, symbol->internalName);
            return node->text != NULL;
        }
        if ( xl6ImportsNamespaceExists(symbols, ns) ) {
            const XL6_ImportNamespace* nsInfo = xl6ImportsFindNamespace(symbols, ns);
            if ( diags != NULL ) {
                xl6DiagAdd(diags,
                           node->line,
                           node->column,
                           nsInfo != NULL && nsInfo->isPrivate ?
                               "private package dependency is not exported" :
                               "import package has no such exported symbol");
            }
            return 0;
        }
    }

    ok = 1;
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6ImportsRewriteNamespaceRefs(node->children[i], symbols, diags) ) {
            ok = 0;
        }
    }
    return ok;
}

int xl6ImportsExpandModule(XL6_AstNode* root, const char* sourcePath, XL6_DiagList* diags, XL6_ImportDeps* deps)
{
    XL6_ImportSymbolTable symbols;
    size_t i;
    size_t initialCount;
    int ok;

    if ( root == NULL ) {
        return 1;
    }
    xl6ImportsAddSourceCompanionC(sourcePath, deps);
    xl6ImportsSymbolTableInit(&symbols);
    initialCount = root->childCount;
    ok = 1;
    for ( i = 0; i < initialCount; ++i ) {
        XL6_AstNode* child = root->children[i];
        if ( child != NULL && child->kind == XL6_AST_IMPORT ) {
            if ( !xl6ImportsLoadOne(root, sourcePath, child, diags, deps, &symbols) ) {
                ok = 0;
            }
        }
    }
    if ( ok ) {
        ok = xl6ImportsMoveAppendedChildrenBeforeBody(root, initialCount, diags);
    }
    if ( ok ) {
        ok = xl6ImportsRewriteNamespaceRefs(root, &symbols, diags);
    }
    xl6ImportsSymbolTableFree(&symbols);
    return ok;
}
