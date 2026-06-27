#include "src/compile/xl6_imports.h"

#include "src/abi/xl6_xapi_load.h"
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
static void xl6ImportsOverrideXapiPackage(XL6_XapiModule* module, const XL6_PackageResolveResult* resolved);
static const char* xl6ImportsNamespaceFor(const XL6_PackageResolveResult* resolved, const XL6_XapiModule* module, const char* target);
static int xl6ImportsSymbolTableAddNamespace(XL6_ImportSymbolTable* table, const char* ns, int isPrivate);
static const XL6_ImportNamespace* xl6ImportsFindNamespace(const XL6_ImportSymbolTable* table, const char* ns);

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

static int xl6ImportsCollectXapiDeps(
    XL6_ImportDeps* deps,
    const XL6_XapiModule* module,
    const char* packageDir,
    XL6_AstNode* importNode,
    XL6_DiagList* diags)
{
    size_t i;

    if ( deps == NULL || module == NULL ) {
        return 1;
    }
    if ( !xl6ImportsDepsAddPackageId(deps, module->packageId) ) {
        return 0;
    }
    for ( i = 0; i < module->cSourceCount; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->cSources, packageDir, module->cSources[i], 1) ) {
            return 0;
        }
    }
    for ( i = 0; i < module->linkCount; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->links, packageDir, module->links[i], 0) ) {
            return 0;
        }
    }
    for ( i = 0; i < module->loadlibCount; ++i ) {
        if ( !xl6ImportsAppendPackagePath(&deps->loadlibs, packageDir, module->loadlibs[i], 0) ) {
            return 0;
        }
    }
    return xl6ImportsDepsAddNative(
        deps,
        module->packageId,
        module->nativeSingleton,
        module->nativeLoadName,
        module->nativeVersion,
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

    if ( dep == NULL || resolved == NULL ) {
        return 0;
    }
    if ( !xl6ImportsTextEquals(dep->version, resolved->packageVersion) ||
         !xl6ImportsTextEquals(dep->abiVersion, resolved->abiVersion) ) {
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
    char versionedName[256];
    int sawMismatchedPackage;

    if ( manifest == NULL || dep == NULL || out == NULL ) {
        return 0;
    }
    sawMismatchedPackage = 0;
    xl6PackageResolveImport(manifest->dir, dep->name, &candidate);
    if ( candidate.resolved ) {
        if ( xl6ImportsDependencyMatches(dep, &candidate, NULL, NULL) ) {
            *out = candidate;
            return 1;
        }
        sawMismatchedPackage = 1;
    }

    snprintf(versionedName, sizeof(versionedName), "%s@%s", dep->name, dep->version);
    xl6PackageResolveImport(manifest->dir, versionedName, &candidate);
    if ( candidate.resolved ) {
        if ( xl6ImportsDependencyMatches(dep, &candidate, importNode, diags) ) {
            *out = candidate;
            return 1;
        }
        return 0;
    }

    if ( sawMismatchedPackage ) {
        return xl6ImportsDependencyMatches(dep, &candidate, importNode, diags);
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
            return 0;
        }
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
    XL6_XapiModule module;
    char xapiPath[512];
    int hasManifest;
    int ok;

    if ( deps == NULL || resolved == NULL || !resolved->resolved ) {
        return 1;
    }
    if ( resolved->packageId[0] != '\0' && xl6ImportsDepsHasPackageId(deps, resolved->packageId) ) {
        return 1;
    }
    if ( !xl6PackageResolveInterfacePath(resolved, xapiPath, sizeof(xapiPath)) || !xrtFileExists((char*)xapiPath) ) {
        if ( diags != NULL ) {
            xl6DiagAdd(diags,
                       importNode != NULL ? importNode->line : 1,
                       importNode != NULL ? importNode->column : 1,
                       "import package missing package.xapi.json interface file");
        }
        return 0;
    }

    xl6PackageManifestInit(&manifest);
    hasManifest = resolved->manifestPath[0] != '\0' &&
                  !xl6ImportsTextEquals(resolved->manifestPath, resolved->interfacePath) &&
                  xl6PackageLoadManifest(resolved->manifestPath, &manifest);

    xl6XapiModuleInit(&module);
    ok = xl6XapiLoadJsonFile(&module, xapiPath, diags);
    if ( ok ) {
        xl6ImportsOverrideXapiPackage(&module, resolved);
        if ( symbols != NULL ) {
            ok = xl6ImportsSymbolTableAddNamespace(
                symbols,
                xl6ImportsNamespaceFor(resolved, &module, resolved->packageName),
                1);
        }
    }
    if ( ok && hasManifest ) {
        ok = xl6ImportsCollectManifestDeps(deps, &manifest, importNode, diags);
    }
    if ( ok ) {
        ok = xl6ImportsCollectXapiDeps(deps, &module, resolved->packageDir, importNode, diags);
    }
    if ( ok && hasManifest ) {
        ok = xl6ImportsCollectManifestDependencyGraph(deps, &manifest, importNode, diags, symbols);
    }

    xl6XapiModuleFree(&module);
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

static int xl6ImportsIsIdentChar(int ch)
{
    return (ch >= 'a' && ch <= 'z') ||
           (ch >= 'A' && ch <= 'Z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_';
}

static int xl6ImportsIsTypeTextIdentChar(int ch)
{
    return xl6ImportsIsIdentChar(ch) || ch >= 0x80;
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

static int xl6ImportsRangeEquals(const char* text, size_t len, const char* value)
{
    return text != NULL && value != NULL && strlen(value) == len && strncmp(text, value, len) == 0;
}

static const XL6_XapiTypeDecl* xl6ImportsFindXapiType(const XL6_XapiModule* module, const char* text, size_t len)
{
    size_t i;

    if ( module == NULL || text == NULL ) {
        return NULL;
    }
    for ( i = 0; i < module->typeCount; ++i ) {
        const XL6_XapiTypeDecl* item = &module->types[i];
        if ( item->name != NULL && xl6ImportsRangeEquals(text, len, item->name) ) {
            return item;
        }
    }
    return NULL;
}

static int xl6ImportsIsOwnerTypeParam(const XL6_XapiTypeDecl* owner, const char* text, size_t len)
{
    size_t i;

    if ( owner == NULL || text == NULL ) {
        return 0;
    }
    for ( i = 0; i < owner->typeParamCount; ++i ) {
        if ( xl6ImportsRangeEquals(text, len, owner->typeParams[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6ImportsAppendRewrittenType(
    XL6_ImportTextBuilder* builder,
    const XL6_XapiModule* module,
    const char* ns,
    const char* typeText,
    const XL6_XapiTypeDecl* owner)
{
    const char* p;

    if ( builder == NULL || typeText == NULL ) {
        return;
    }
    p = typeText;
    while ( *p != '\0' ) {
        unsigned char ch = (unsigned char)*p;
        if ( xl6ImportsIsTypeTextIdentChar(ch) && !(ch >= '0' && ch <= '9') ) {
            const char* start = p;
            size_t len;
            const XL6_XapiTypeDecl* typeItem;

            p += 1;
            while ( *p != '\0' && xl6ImportsIsTypeTextIdentChar((unsigned char)*p) ) {
                p += 1;
            }
            len = (size_t)(p - start);
            typeItem = !xl6ImportsIsOwnerTypeParam(owner, start, len) ?
                       xl6ImportsFindXapiType(module, start, len) :
                       NULL;
            if ( typeItem != NULL ) {
                char* internalName = xl6ImportsMakeInternalName(ns, typeItem->name);
                xl6ImportsBuilderAppend(builder, internalName);
                xrtFree(internalName);
            } else {
                xl6ImportsBuilderAppendRange(builder, start, len);
            }
        } else {
            xl6ImportsBuilderAppendRange(builder, p, 1);
            p += 1;
        }
    }
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

static int xl6ImportsFunctionCanEnterExtern(const XL6_XapiExport* item)
{
    size_t i;

    if ( item == NULL || !xl6ImportsTextEquals(item->kind, "function") ) {
        return 0;
    }
    if ( item->callingConvention != NULL && !xl6ImportsTextEquals(item->callingConvention, "cdecl") ) {
        return 0;
    }
    if ( item->varargMode != NULL && !xl6ImportsTextEquals(item->varargMode, "none") ) {
        return 0;
    }
    if ( item->returnCount > 1 ) {
        return 0;
    }
    for ( i = 0; i < item->paramCount; ++i ) {
        if ( item->params[i].optional || item->params[i].vararg ) {
            return 0;
        }
    }
    return 1;
}

static void xl6ImportsAppendGenericParams(XL6_ImportTextBuilder* builder, const XL6_XapiTypeDecl* item)
{
    size_t i;

    if ( item == NULL || item->typeParamCount == 0 ) {
        return;
    }
    xl6ImportsBuilderAppend(builder, "<");
    for ( i = 0; i < item->typeParamCount; ++i ) {
        if ( i > 0 ) {
            xl6ImportsBuilderAppend(builder, ", ");
        }
        xl6ImportsBuilderAppend(builder, item->typeParams[i]);
    }
    xl6ImportsBuilderAppend(builder, ">");
}

static void xl6ImportsAppendFieldDecl(
    XL6_ImportTextBuilder* builder,
    const XL6_XapiModule* module,
    const char* ns,
    const XL6_XapiTypeDecl* owner,
    const XL6_XapiField* field,
    const char* indent)
{
    if ( field == NULL || field->name == NULL || field->type == NULL ) {
        return;
    }
    xl6ImportsBuilderAppend(builder, indent);
    xl6ImportsAppendRewrittenType(builder, module, ns, field->type, owner);
    xl6ImportsBuilderAppend(builder, " ");
    xl6ImportsBuilderAppend(builder, field->name);
    xl6ImportsBuilderAppend(builder, "\n");
}

static void xl6ImportsAppendUnionTypeDecl(
    XL6_ImportTextBuilder* builder,
    const XL6_XapiModule* module,
    const char* ns,
    const XL6_XapiTypeDecl* item,
    const char* internalName)
{
    size_t i;

    xl6ImportsBuilderAppend(builder, "union ");
    xl6ImportsBuilderAppend(builder, internalName);
    xl6ImportsBuilderAppend(builder, "\n");
    for ( i = 0; i < item->fieldCount; ++i ) {
        xl6ImportsAppendFieldDecl(builder, module, ns, item, &item->fields[i], "    ");
    }
    xl6ImportsBuilderAppend(builder, "end\n");
}

static void xl6ImportsAppendClassTypeDecl(
    XL6_ImportTextBuilder* builder,
    const XL6_XapiModule* module,
    const char* ns,
    const XL6_XapiTypeDecl* item,
    const char* internalName)
{
    size_t i;
    int inlineUnionOpen;

    xl6ImportsBuilderAppend(builder, "class ");
    xl6ImportsBuilderAppend(builder, internalName);
    xl6ImportsAppendGenericParams(builder, item);
    if ( item->base != NULL && item->base[0] != '\0' ) {
        xl6ImportsBuilderAppend(builder, " extends ");
        xl6ImportsAppendRewrittenType(builder, module, ns, item->base, item);
    }
    xl6ImportsBuilderAppend(builder, "\n");

    inlineUnionOpen = 0;
    for ( i = 0; i < item->fieldCount; ++i ) {
        const XL6_XapiField* field = &item->fields[i];
        int isInlineUnion = xl6ImportsTextEquals(field->storage, "inline-union");
        if ( isInlineUnion && !inlineUnionOpen ) {
            xl6ImportsBuilderAppend(builder, "    union\n");
            inlineUnionOpen = 1;
        } else if ( !isInlineUnion && inlineUnionOpen ) {
            xl6ImportsBuilderAppend(builder, "    end\n");
            inlineUnionOpen = 0;
        }
        xl6ImportsAppendFieldDecl(builder, module, ns, item, field, inlineUnionOpen ? "        " : "    ");
    }
    if ( inlineUnionOpen ) {
        xl6ImportsBuilderAppend(builder, "    end\n");
    }
    xl6ImportsBuilderAppend(builder, "end\n");
}

static void xl6ImportsAppendCustomTypeDecl(XL6_ImportTextBuilder* builder, const char* internalName)
{
    xl6ImportsBuilderAppend(builder, "custom ");
    xl6ImportsBuilderAppend(builder, internalName);
    xl6ImportsBuilderAppend(builder, "\nend\n");
}

static int xl6ImportsAppendXapiTypeDecls(XL6_ImportTextBuilder* builder, const XL6_XapiModule* module, const char* ns)
{
    size_t i;

    if ( builder == NULL || module == NULL || ns == NULL ) {
        return 0;
    }
    for ( i = 0; i < module->typeCount; ++i ) {
        const XL6_XapiTypeDecl* item = &module->types[i];
        char* internalName;

        if ( item->name == NULL || item->name[0] == '\0' || item->kind == NULL ) {
            continue;
        }
        internalName = xl6ImportsMakeInternalName(ns, item->name);
        if ( internalName == NULL ) {
            return 0;
        }
        if ( xl6ImportsTextEquals(item->kind, "union") ) {
            xl6ImportsAppendUnionTypeDecl(builder, module, ns, item, internalName);
        } else if ( xl6ImportsTextEquals(item->kind, "class") ) {
            xl6ImportsAppendClassTypeDecl(builder, module, ns, item, internalName);
        } else if ( xl6ImportsTextEquals(item->kind, "custom") ) {
            xl6ImportsAppendCustomTypeDecl(builder, internalName);
        }
        xrtFree(internalName);
    }
    return builder->ok;
}

static void xl6ImportsAppendFunctionDecl(
    XL6_ImportTextBuilder* builder,
    const XL6_XapiModule* module,
    const char* ns,
    const XL6_XapiExport* item,
    const char* internalName)
{
    size_t i;

    xl6ImportsBuilderAppend(builder, "extern ");
    xl6ImportsBuilderAppendQuoted(builder, item->cSymbol);
    xl6ImportsBuilderAppend(builder, " func ");
    xl6ImportsBuilderAppend(builder, internalName);
    xl6ImportsBuilderAppend(builder, "(");
    for ( i = 0; i < item->paramCount; ++i ) {
        if ( i > 0 ) {
            xl6ImportsBuilderAppend(builder, ", ");
        }
        xl6ImportsAppendRewrittenType(builder, module, ns, item->params[i].type, NULL);
        xl6ImportsBuilderAppend(builder, " ");
        xl6ImportsBuilderAppend(builder, item->params[i].name);
    }
    xl6ImportsBuilderAppend(builder, ")");
    if ( item->returnCount == 1 ) {
        xl6ImportsBuilderAppend(builder, " as ");
        xl6ImportsAppendRewrittenType(builder, module, ns, item->returns[0], NULL);
    }
    xl6ImportsBuilderAppend(builder, "\n");
}

static void xl6ImportsAppendVarDecl(
    XL6_ImportTextBuilder* builder,
    const XL6_XapiModule* module,
    const char* ns,
    const XL6_XapiExport* item,
    const char* internalName)
{
    if ( item == NULL || !xl6ImportsTextEquals(item->kind, "var") || item->type == NULL ) {
        return;
    }
    xl6ImportsBuilderAppend(builder, "extern ");
    xl6ImportsBuilderAppendQuoted(builder, item->cSymbol);
    xl6ImportsBuilderAppend(builder, " var ");
    xl6ImportsAppendRewrittenType(builder, module, ns, item->type, NULL);
    xl6ImportsBuilderAppend(builder, " ");
    xl6ImportsBuilderAppend(builder, internalName);
    xl6ImportsBuilderAppend(builder, "\n");
}

static int xl6ImportsAppendXapiDecls(
    XL6_ImportTextBuilder* builder,
    XL6_ImportSymbolTable* symbols,
    const XL6_XapiModule* module,
    const char* ns)
{
    size_t i;

    if ( builder == NULL || symbols == NULL || module == NULL || ns == NULL ) {
        return 0;
    }
    if ( !xl6ImportsAppendXapiTypeDecls(builder, module, ns) ) {
        return 0;
    }
    for ( i = 0; i < module->exportCount; ++i ) {
        const XL6_XapiExport* item = &module->exports[i];
        char* internalName;
        int supported;

        if ( item->name == NULL || item->name[0] == '\0' ) {
            continue;
        }
        supported = xl6ImportsFunctionCanEnterExtern(item) ||
                    (xl6ImportsTextEquals(item->kind, "var") && item->type != NULL);
        internalName = supported ? xl6ImportsMakeInternalName(ns, item->name) : xl6ImportsDupText("");
        if ( internalName == NULL ) {
            return 0;
        }
        if ( !xl6ImportsSymbolTableAdd(symbols, ns, item->name, internalName, supported) ) {
            xrtFree(internalName);
            return 0;
        }
        if ( supported ) {
            if ( xl6ImportsFunctionCanEnterExtern(item) ) {
                xl6ImportsAppendFunctionDecl(builder, module, ns, item, internalName);
            } else {
                xl6ImportsAppendVarDecl(builder, module, ns, item, internalName);
            }
        }
        xrtFree(internalName);
    }
    return builder->ok;
}

static int xl6ImportsMergeGeneratedDecls(XL6_AstNode* root, const char* generated, XL6_DiagList* diags)
{
    XL6_ParseResult result;
    int ok;

    if ( generated == NULL || generated[0] == '\0' ) {
        return 1;
    }
    xl6ParseResultInit(&result);
    ok = xl6ParseSource(generated, "<xapi-import>", &result);
    if ( !ok ) {
        if ( diags != NULL ) {
            xl6DiagAdd(diags, 1, 1, "XAPI import generated invalid extern declarations");
        }
        xl6ParseResultFree(&result);
        return 0;
    }
    if ( result.root != NULL ) {
        xl6AstMoveChildren(root, result.root);
    }
    xl6ParseResultFree(&result);
    return 1;
}

static const char* xl6ImportsNamespaceFor(const XL6_PackageResolveResult* resolved, const XL6_XapiModule* module, const char* target)
{
    if ( resolved != NULL && resolved->packageName[0] != '\0' ) {
        return resolved->packageName;
    }
    if ( module != NULL && module->packageName != NULL && module->packageName[0] != '\0' ) {
        return module->packageName;
    }
    if ( module != NULL && module->name != NULL && module->name[0] != '\0' ) {
        return module->name;
    }
    return target != NULL ? target : "package";
}

static void xl6ImportsOverrideXapiPackage(XL6_XapiModule* module, const XL6_PackageResolveResult* resolved)
{
    if ( module == NULL || resolved == NULL || resolved->packageId[0] == '\0' ) {
        return;
    }
    xrtFree(module->packageName);
    xrtFree(module->packageVersion);
    xrtFree(module->packageAbiVersion);
    xrtFree(module->packageId);
    module->packageName = xl6ImportsDupText(resolved->packageName);
    module->packageVersion = xl6ImportsDupText(resolved->packageVersion);
    module->packageAbiVersion = xl6ImportsDupText(resolved->abiVersion);
    module->packageId = xl6ImportsDupText(resolved->packageId);
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
    XL6_XapiModule module;
    XL6_ImportTextBuilder builder;
    char xapiPath[512];
    char* target;
    char* generated;
    const char* ns;
    int hasManifest;
    int ok;

    target = xl6ImportsDupImportTarget(importNode != NULL ? importNode->text : NULL);
    if ( target == NULL ) {
        return 0;
    }

    xl6PackageResolveImport(sourcePath, target, &resolved);
    if ( !resolved.resolved ) {
        xrtFree(target);
        return 1;
    }
    if ( !xl6PackageResolveInterfacePath(&resolved, xapiPath, sizeof(xapiPath)) || !xrtFileExists((char*)xapiPath) ) {
        xrtFree(target);
        if ( diags != NULL ) {
            xl6DiagAdd(diags,
                       importNode != NULL ? importNode->line : 1,
                       importNode != NULL ? importNode->column : 1,
                       "import package missing package.xapi.json interface file");
        }
        return 0;
    }

    xl6PackageManifestInit(&manifest);
    hasManifest = resolved.manifestPath[0] != '\0' &&
                  !xl6ImportsTextEquals(resolved.manifestPath, resolved.interfacePath) &&
                  xl6PackageLoadManifest(resolved.manifestPath, &manifest);

    xl6XapiModuleInit(&module);
    ok = xl6XapiLoadJsonFile(&module, xapiPath, diags);
    if ( ok ) {
        xl6ImportsOverrideXapiPackage(&module, &resolved);
        ns = xl6ImportsNamespaceFor(&resolved, &module, target);
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
    }
    if ( ok && hasManifest ) {
        ok = xl6ImportsCollectManifestDeps(deps, &manifest, importNode, diags);
    }
    if ( ok ) {
        ok = xl6ImportsCollectXapiDeps(deps, &module, resolved.packageDir, importNode, diags);
    }
    if ( ok && hasManifest ) {
        ok = xl6ImportsCollectManifestDependencyGraph(deps, &manifest, importNode, diags, symbols);
    }
    if ( ok ) {
        xl6ImportsBuilderInit(&builder);
        ok = xl6ImportsAppendXapiDecls(&builder, symbols, &module, ns);
        generated = xl6ImportsBuilderTake(&builder);
        xl6ImportsBuilderFree(&builder);
        if ( ok ) {
            ok = xl6ImportsMergeGeneratedDecls(root, generated, diags);
        }
        xrtFree(generated);
    }

    xl6XapiModuleFree(&module);
    xl6PackageManifestFree(&manifest);
    xrtFree(target);
    return ok;
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

static int xl6ImportsRewriteNamespaceRefs(XL6_AstNode* node, const XL6_ImportSymbolTable* symbols, XL6_DiagList* diags)
{
    size_t i;
    int ok;

    if ( node == NULL ) {
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
                             "XAPI 导入符号当前不能进入 C ABI bridge: %s.%s",
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
        ok = xl6ImportsRewriteNamespaceRefs(root, &symbols, diags);
    }
    xl6ImportsSymbolTableFree(&symbols);
    return ok;
}
