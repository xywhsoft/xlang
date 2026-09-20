#include "src/package/xl6_package.h"
#include "src/common/xl6_common.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static char* xl6PackageDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtStrDupN(src, strlen(src));
}

static char* xl6PackageDirName(const char* path)
{
    char* result;

    if ( path == NULL || path[0] == '\0' ) {
        return xl6PackageDupText(".");
    }
    result = xrtPathParent(path);
    if ( result == NULL || result[0] == '\0' ) {
        xrtFree(result);
        return xl6PackageDupText(".");
    }
    return result;
}

static char* xl6PackageJoinPathAlloc(const char* a, const char* b)
{
    if ( a == NULL || b == NULL ) {
        return NULL;
    }
    return xrtPathJoin(a, b);
}

int xl6PackageJoinPath(char* out, size_t outSize, const char* a, const char* b)
{
    char* joined;
    size_t len;

    if ( out == NULL || outSize == 0 || a == NULL || b == NULL ) {
        return 0;
    }
    joined = xl6PackageJoinPathAlloc(a, b);
    if ( joined == NULL ) {
        return 0;
    }
    len = strlen(joined);
    if ( len >= outSize ) {
        xrtFree(joined);
        return 0;
    }
    memcpy(out, joined, len + 1);
    xrtFree(joined);
    return 1;
}

static char* xl6PackageBaseNameNoExt(const char* path)
{
    char* result;

    result = path != NULL ? xrtPathStem(path) : NULL;
    if ( result == NULL || result[0] == '\0' ) {
        xrtFree(result);
        return xl6PackageDupText("package");
    }
    return result;
}

static int xl6PackageEndsWith(const char* text, const char* suffix)
{
    size_t textLen;
    size_t suffixLen;

    if ( text == NULL || suffix == NULL ) {
        return 0;
    }
    textLen = strlen(text);
    suffixLen = strlen(suffix);
    return textLen >= suffixLen && strcmp(text + textLen - suffixLen, suffix) == 0;
}

static char* xl6PackageMakePackageId(const char* name, const char* version, const char* abiVersion)
{
    return (char*)xrtFormat(
        "%s@%s#abi%s",
        name != NULL && name[0] != '\0' ? name : "package",
        version != NULL && version[0] != '\0' ? version : "0.0.0",
        abiVersion != NULL && abiVersion[0] != '\0' ? abiVersion : "1");
}

void xl6PackageStringListInit(XL6_PackageStringList* list)
{
    if ( list != NULL ) {
        memset(list, 0, sizeof(*list));
    }
}

void xl6PackageStringListFree(XL6_PackageStringList* list)
{
    size_t i;

    if ( list == NULL ) {
        return;
    }
    for ( i = 0; i < list->count; ++i ) {
        xrtFree(list->items[i]);
    }
    xrtFree(list->items);
    memset(list, 0, sizeof(*list));
}

int xl6PackageStringListAppend(XL6_PackageStringList* list, const char* text)
{
    char** newItems;
    char* copy;

    if ( list == NULL || text == NULL ) {
        return 0;
    }
    copy = xl6PackageDupText(text);
    if ( copy == NULL ) {
        return 0;
    }
    newItems = (char**)xrtRealloc(list->items, (list->count + 1) * sizeof(char*));
    if ( newItems == NULL ) {
        xrtFree(copy);
        return 0;
    }
    list->items = newItems;
    list->items[list->count] = copy;
    list->count += 1;
    return 1;
}

void xl6PackageManifestInit(XL6_PackageManifest* manifest)
{
    if ( manifest != NULL ) {
        memset(manifest, 0, sizeof(*manifest));
    }
}

void xl6PackageManifestFree(XL6_PackageManifest* manifest)
{
    size_t i;

    if ( manifest == NULL ) {
        return;
    }
    xrtFree(manifest->format);
    xrtFree(manifest->name);
    xrtFree(manifest->version);
    xrtFree(manifest->abiVersion);
    xrtFree(manifest->packageId);
    xrtFree(manifest->dir);
    xl6PackageStringListFree(&manifest->cSources);
    xl6PackageStringListFree(&manifest->includeDirs);
    xl6PackageStringListFree(&manifest->xlSources);
    xl6PackageStringListFree(&manifest->xrtModules);
    xl6PackageStringListFree(&manifest->links);
    xl6PackageStringListFree(&manifest->loadlibs);
    for ( i = 0; i < manifest->dependencyCount; ++i ) {
        xrtFree(manifest->dependencies[i].name);
        xrtFree(manifest->dependencies[i].version);
        xrtFree(manifest->dependencies[i].abiVersion);
        xrtFree(manifest->dependencies[i].packageId);
    }
    xrtFree(manifest->dependencies);
    xrtFree(manifest->nativeInfo.loadName);
    xrtFree(manifest->nativeInfo.version);
    memset(manifest, 0, sizeof(*manifest));
}

/*
    package 层只验证字段约束，JSON 词法、转义和 Unicode 统一交给 XRT。
    未识别字段允许保留，便于清单元数据向前扩展；所有已知字段必须严格匹配类型。
*/
static int xl6PackageValueHasField(const xvalue* object, const char* name)
{
    return object != NULL && name != NULL &&
           xrtValueType(object) == XVALUE_OBJECT &&
           xrtValueObjectHas(object, xrtStrView(name));
}

static xvalue* xl6PackageValueField(const xvalue* object, const char* name)
{
    return xrtValueObjectGet(object, xrtStrView(name));
}

static int xl6PackageValueReadText(const xvalue* object, const char* name, char** out, int required)
{
    xvalue* value;
    xstrview text;
    char* copy;

    if ( out == NULL ) {
        return 0;
    }
    if ( !xl6PackageValueHasField(object, name) ) {
        return !required;
    }
    value = xl6PackageValueField(object, name);
    if ( !xrtValueGetString(value, &text) ) {
        return 0;
    }
    copy = xrtStrDupN(text.Data, text.Size);
    if ( copy == NULL ) {
        return 0;
    }
    xrtFree(*out);
    *out = copy;
    return 1;
}

static int xl6PackageValueReadStringArray(
    const xvalue* object,
    const char* name,
    XL6_PackageStringList* list)
{
    xvalue* array;
    size_t count;
    size_t i;

    if ( !xl6PackageValueHasField(object, name) ) {
        return 1;
    }
    array = xl6PackageValueField(object, name);
    if ( xrtValueType(array) != XVALUE_ARRAY ) {
        return 0;
    }
    count = xrtValueCount(array);
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xrtValueArrayGet(array, i);
        xstrview text;
        char* copy;

        if ( !xrtValueGetString(item, &text) ) {
            return 0;
        }
        copy = xrtStrDupN(text.Data, text.Size);
        if ( copy == NULL || !xl6PackageStringListAppend(list, copy) ) {
            xrtFree(copy);
            return 0;
        }
        xrtFree(copy);
    }
    return 1;
}

static int xl6PackageManifestAppendDependency(
    XL6_PackageManifest* manifest,
    xstrview name,
    xstrview version,
    xstrview abiVersion)
{
    XL6_PackageDependency* newItems;
    XL6_PackageDependency* dep;
    char* nameCopy;
    char* versionCopy;
    char* abiCopy;
    char* packageId;

    if ( manifest == NULL || name.Data == NULL || name.Size == 0 ||
         manifest->dependencyCount == SIZE_MAX / sizeof(XL6_PackageDependency) ) {
        return 0;
    }

    /* 所有字段先构造完成；扩容失败时 manifest 保持原状。 */
    nameCopy = xrtStrDupN(name.Data, name.Size);
    versionCopy = version.Data != NULL && version.Size != 0 ?
        xrtStrDupN(version.Data, version.Size) : xl6PackageDupText("0.0.0");
    abiCopy = abiVersion.Data != NULL && abiVersion.Size != 0 ?
        xrtStrDupN(abiVersion.Data, abiVersion.Size) : xl6PackageDupText("1");
    packageId = xl6PackageMakePackageId(nameCopy, versionCopy, abiCopy);
    if ( nameCopy == NULL || versionCopy == NULL || abiCopy == NULL || packageId == NULL ) {
        xrtFree(nameCopy);
        xrtFree(versionCopy);
        xrtFree(abiCopy);
        xrtFree(packageId);
        return 0;
    }

    newItems = (XL6_PackageDependency*)xrtRealloc(
        manifest->dependencies,
        (manifest->dependencyCount + 1) * sizeof(XL6_PackageDependency));
    if ( newItems == NULL ) {
        xrtFree(nameCopy);
        xrtFree(versionCopy);
        xrtFree(abiCopy);
        xrtFree(packageId);
        return 0;
    }

    manifest->dependencies = newItems;
    dep = &manifest->dependencies[manifest->dependencyCount];
    dep->name = nameCopy;
    dep->version = versionCopy;
    dep->abiVersion = abiCopy;
    dep->packageId = packageId;
    manifest->dependencyCount += 1;
    return 1;
}

static int xl6PackageValueReadDependencies(const xvalue* root, XL6_PackageManifest* manifest)
{
    xvalue* array;
    size_t count;
    size_t i;

    if ( !xl6PackageValueHasField(root, "dependencies") ) {
        return 1;
    }
    array = xl6PackageValueField(root, "dependencies");
    if ( xrtValueType(array) != XVALUE_ARRAY ) {
        return 0;
    }
    count = xrtValueCount(array);
    for ( i = 0; i < count; ++i ) {
        xvalue* item = xrtValueArrayGet(array, i);
        if ( xrtValueType(item) == XVALUE_STRING ) {
            xstrview name;

            if ( !xrtValueGetString(item, &name) || name.Size == 0 ||
                 !xl6PackageManifestAppendDependency(
                     manifest, name, XRT_STR_LITERAL("*"), XRT_STR_LITERAL("*")) ) {
                return 0;
            }
        } else if ( xrtValueType(item) == XVALUE_OBJECT ) {
            xvalue* nameValue;
            xvalue* versionValue;
            xvalue* abiValue;
            xstrview name;
            xstrview version = XRT_STR_LITERAL("*");
            xstrview abiVersion = XRT_STR_LITERAL("*");

            if ( !xl6PackageValueHasField(item, "name") ) {
                return 0;
            }
            nameValue = xl6PackageValueField(item, "name");
            if ( !xrtValueGetString(nameValue, &name) || name.Size == 0 ) {
                return 0;
            }
            if ( xl6PackageValueHasField(item, "version") ) {
                versionValue = xl6PackageValueField(item, "version");
                if ( !xrtValueGetString(versionValue, &version) ) {
                    return 0;
                }
            }
            if ( xl6PackageValueHasField(item, "abiVersion") ) {
                abiValue = xl6PackageValueField(item, "abiVersion");
                if ( !xrtValueGetString(abiValue, &abiVersion) ) {
                    return 0;
                }
            }
            if ( !xl6PackageManifestAppendDependency(
                    manifest, name, version, abiVersion) ) {
                return 0;
            }
        } else {
            return 0;
        }
    }
    return 1;
}

static int xl6PackageValueReadArtifact(const xvalue* root, XL6_PackageManifest* manifest)
{
    xvalue* artifact;
    size_t i;

    if ( !xl6PackageValueHasField(root, "artifact") ) {
        return 1;
    }
    artifact = xl6PackageValueField(root, "artifact");
    if ( xrtValueType(artifact) != XVALUE_OBJECT ||
         !xl6PackageValueReadStringArray(artifact, "cSources", &manifest->cSources) ||
         !xl6PackageValueReadStringArray(artifact, "includeDirs", &manifest->includeDirs) ||
         !xl6PackageValueReadStringArray(artifact, "xlSources", &manifest->xlSources) ||
         !xl6PackageValueReadStringArray(artifact, "xrtModules", &manifest->xrtModules) ) {
        return 0;
    }

    manifest->xrtModulesDeclared = xl6PackageValueHasField(artifact, "xrtModules");
    for ( i = 0; i < manifest->xrtModules.count; i += 1 ) {
        const char* name = manifest->xrtModules.items[i];
        size_t prefixSize;
        size_t j;

        if ( name == NULL ) {
            return 0;
        }
        if ( strncmp(name, "XRT_MODULE_", 11u) == 0 ) {
            prefixSize = 11u;
        } else if ( strncmp(name, "XRUNTIME_MODULE_", 16u) == 0 ) {
            prefixSize = 16u;
        } else {
            return 0;
        }
        if ( name[prefixSize] == '\0' ) {
            return 0;
        }
        for ( j = prefixSize; name[j] != '\0'; j += 1 ) {
            if ( !((name[j] >= 'A' && name[j] <= 'Z') ||
                   (name[j] >= '0' && name[j] <= '9') ||
                    name[j] == '_') ) {
                return 0;
            }
        }
    }
    return 1;
}

static int xl6PackageValueReadNative(const xvalue* root, XL6_PackageManifest* manifest)
{
    xvalue* native;
    xvalue* singleton;
    bool value;

    if ( !xl6PackageValueHasField(root, "native") ) {
        return 1;
    }
    native = xl6PackageValueField(root, "native");
    if ( xrtValueType(native) != XVALUE_OBJECT ) {
        return 0;
    }
    if ( xl6PackageValueHasField(native, "singleton") ) {
        singleton = xl6PackageValueField(native, "singleton");
        if ( !xrtValueGetBool(singleton, &value) ) {
            return 0;
        }
        manifest->nativeInfo.singleton = value ? 1 : 0;
    }
    return xl6PackageValueReadText(native, "loadName", &manifest->nativeInfo.loadName, 0) &&
           xl6PackageValueReadText(native, "version", &manifest->nativeInfo.version, 0);
}

static int xl6PackageManifestReadXrtValue(const xvalue* root, XL6_PackageManifest* manifest)
{
    return xrtValueType(root) == XVALUE_OBJECT &&
           xl6PackageValueReadText(root, "format", &manifest->format, 1) &&
           xl6PackageValueReadText(root, "name", &manifest->name, 0) &&
           xl6PackageValueReadText(root, "version", &manifest->version, 0) &&
           xl6PackageValueReadText(root, "abiVersion", &manifest->abiVersion, 0) &&
           xl6PackageValueReadArtifact(root, manifest) &&
           xl6PackageValueReadStringArray(root, "cSources", &manifest->cSources) &&
           xl6PackageValueReadStringArray(root, "xlSources", &manifest->xlSources) &&
           xl6PackageValueReadStringArray(root, "links", &manifest->links) &&
           xl6PackageValueReadStringArray(root, "loadlibs", &manifest->loadlibs) &&
           xl6PackageValueReadDependencies(root, manifest) &&
           xl6PackageValueReadNative(root, manifest);
}

int xl6PackageLoadManifest(const char* path, XL6_PackageManifest* manifest)
{
    char* text;
    char* dir;
    char* fallbackName;
    size_t textLen;
    xvalue* root;
    int ok;

    if ( path == NULL || manifest == NULL ) {
        return 0;
    }

    xl6PackageManifestFree(manifest);
    xl6PackageManifestInit(manifest);

    text = xrtFileReadText(path, XENCODING_UTF8, XUTF_STRICT, &textLen);
    if ( text == NULL ) {
        xrtFree(text);
        return 0;
    }
    root = xrtJsonParse(xrtStrViewN(text, textLen));
    ok = root != NULL && xl6PackageManifestReadXrtValue(root, manifest);
    xrtValueRelease(root);
    xrtFree(text);
    dir = xl6PackageDirName(path);
    fallbackName = xl6PackageBaseNameNoExt(dir);
    if ( dir == NULL || fallbackName == NULL ) {
        xrtFree(dir);
        xrtFree(fallbackName);
        xl6PackageManifestFree(manifest);
        return 0;
    }
    if ( ok && (manifest->format == NULL || strcmp(manifest->format, "xlang-package-1") != 0) ) {
        ok = 0;
    }
    if ( ok && (manifest->name == NULL || manifest->name[0] == '\0') ) {
        manifest->name = xl6PackageDupText(fallbackName);
    }
    if ( ok && manifest->version == NULL ) {
        manifest->version = xl6PackageDupText("0.0.0");
    }
    if ( ok && manifest->abiVersion == NULL ) {
        manifest->abiVersion = xl6PackageDupText("1");
    }
    if ( ok ) {
        manifest->dir = dir;
        dir = NULL;
        manifest->packageId = xl6PackageMakePackageId(
            manifest->name, manifest->version, manifest->abiVersion);
        if ( manifest->name == NULL || manifest->version == NULL || manifest->abiVersion == NULL ||
             manifest->dir == NULL || manifest->packageId == NULL ) {
            ok = 0;
        }
    }
    xrtFree(dir);
    xrtFree(fallbackName);
    if ( !ok ) {
        xl6PackageManifestFree(manifest);
    }
    return ok;
}

static int xl6PackageFinalizeManifestResult(XL6_PackageResolveResult* result)
{
    XL6_PackageManifest manifest;
    char* packageDir;
    char* packageName;
    char* packageVersion;
    char* abiVersion;
    char* packageId;

    xl6PackageManifestInit(&manifest);
    if ( !xl6PackageLoadManifest(result->manifestPath, &manifest) ) {
        xl6PackageManifestFree(&manifest);
        return 0;
    }
    packageDir = xl6PackageDupText(manifest.dir);
    packageName = xl6PackageDupText(manifest.name);
    packageVersion = xl6PackageDupText(manifest.version);
    abiVersion = xl6PackageDupText(manifest.abiVersion);
    packageId = xl6PackageDupText(manifest.packageId);
    xl6PackageManifestFree(&manifest);
    if ( packageDir == NULL || packageName == NULL || packageVersion == NULL ||
         abiVersion == NULL || packageId == NULL ) {
        xrtFree(packageDir);
        xrtFree(packageName);
        xrtFree(packageVersion);
        xrtFree(abiVersion);
        xrtFree(packageId);
        return 0;
    }
    result->packageDir = packageDir;
    result->packageName = packageName;
    result->packageVersion = packageVersion;
    result->abiVersion = abiVersion;
    result->packageId = packageId;
    return 1;
}

static void xl6PackageResolveResultClearResolution(XL6_PackageResolveResult* result)
{
    if ( result == NULL ) {
        return;
    }
    xrtFree(result->manifestPath);
    xrtFree(result->packageDir);
    xrtFree(result->packageName);
    xrtFree(result->packageVersion);
    xrtFree(result->abiVersion);
    xrtFree(result->packageId);
    result->manifestPath = NULL;
    result->packageDir = NULL;
    result->packageName = NULL;
    result->packageVersion = NULL;
    result->abiVersion = NULL;
    result->packageId = NULL;
    result->resolved = 0;
    result->kind = XL6_PACKAGE_RESOLVE_UNRESOLVED;
}

static int xl6PackageTryManifest(XL6_PackageResolveResult* result, const char* path, XL6_PackageResolveKind kind)
{
    if ( result == NULL || path == NULL ) {
        return 0;
    }
    if ( !xrtFileExists((char*)path) ) {
        return 0;
    }
    result->manifestPath = xl6PackageDupText(path);
    if ( result->manifestPath == NULL ) {
        return 0;
    }
    result->resolved = 1;
    result->kind = kind;
    if ( !xl6PackageFinalizeManifestResult(result) ) {
        xl6PackageResolveResultClearResolution(result);
        return 0;
    }
    return 1;
}

static int xl6PackageLooksLikePath(const char* target)
{
    const char* p;

    if ( target == NULL ) {
        return 0;
    }
    for ( p = target; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' || *p == '.' ) {
            return 1;
        }
    }
    return 0;
}

static int xl6PackagePathIsAbsolute(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( path[0] == '/' || path[0] == '\\' ) {
        return 1;
    }
    if ( path[1] == ':' &&
         ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) ) {
        return 1;
    }
    return 0;
}

static int xl6PackageTryPathCandidates(XL6_PackageResolveResult* result, const char* path)
{
    char* path2;
    int ok;

    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( xl6PackageTryManifest(result, path, XL6_PACKAGE_RESOLVE_FILE) ) {
        return 1;
    }
    path2 = xl6PackageJoinPathAlloc(path, "xlang.package.json");
    ok = path2 != NULL && xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE);
    xrtFree(path2);
    return ok;
}

static int xl6PackageCanBePackageName(const char* target)
{
    const char* p;

    if ( target == NULL || target[0] == '\0' || target[0] == '.' ) {
        return 0;
    }
    for ( p = target; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            return 0;
        }
    }
    return 1;
}

static int xl6PackageTryNamedPackageInBase(XL6_PackageResolveResult* result, const char* baseDir, const char* target)
{
    char* path;
    char* path2;
    int ok;

    if ( baseDir == NULL || baseDir[0] == '\0' || !xl6PackageCanBePackageName(target) ) {
        return 0;
    }
    path = xl6PackageJoinPathAlloc(baseDir, target);
    path2 = path != NULL ? xl6PackageJoinPathAlloc(path, "xlang.package.json") : NULL;
    ok = path2 != NULL && xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE);
    xrtFree(path2);
    xrtFree(path);
    return ok;
}

static int xl6PackageTryEnvPackagePath(XL6_PackageResolveResult* result, const char* target)
{
    char* env;
    const char* start;
#if defined(_WIN32) || defined(_WIN64)
    const char sep = ';';
#else
    const char sep = ':';
#endif

    env = (char*)xrtEnvGet((str)"XLANG_PACKAGE_PATH");
    if ( env == NULL || env[0] == '\0' ) {
        xrtFree(env);
        return 0;
    }
    start = env;
    for (;;) {
        const char* end = strchr(start, sep);
        size_t len = end != NULL ? (size_t)(end - start) : strlen(start);
        char* baseDir;

        if ( len > 0 ) {
            baseDir = xrtStrDupN(start, len);
            if ( baseDir != NULL && xl6PackageTryNamedPackageInBase(result, baseDir, target) ) {
                xrtFree(baseDir);
                xrtFree(env);
                return 1;
            }
            xrtFree(baseDir);
        }
        if ( end == NULL ) {
            break;
        }
        start = end + 1;
    }
    xrtFree(env);
    return 0;
}

static char* xl6PackageGetExecutableDir(void)
{
    char* path = (char*)xrtPathAppDir();
    if ( path == NULL || path[0] == '\0' ) {
        xrtFree(path);
        return NULL;
    }
    return path;
}

static int xl6PackageTryExeExtlibsPackage(XL6_PackageResolveResult* result, const char* target)
{
    char* exeDir;
    char* currentDir;
    int ok = 0;

    exeDir = xl6PackageGetExecutableDir();
    if ( exeDir == NULL ) {
        return 0;
    }
    currentDir = exeDir;

    /*
     * 可执行文件可以作为独立宿主嵌在项目的子目录，例如 projects/xs/xs.exe。
     * 扩展库仍然统一发布在 release/extlibs，因此从宿主目录逐级向上查找。
     * 这不是旧路径的兼容映射，而是包运行时的正式定位规则。
     */
    while ( currentDir != NULL && currentDir[0] != '\0' ) {
        char* extlibsDir = xl6PackageJoinPathAlloc(currentDir, "extlibs");
        char* parentDir;

        if ( extlibsDir != NULL &&
             xl6PackageTryNamedPackageInBase(result, extlibsDir, target) ) {
            ok = 1;
            xrtFree(extlibsDir);
            break;
        }
        xrtFree(extlibsDir);

        parentDir = xrtPathParent(currentDir);
        if ( parentDir == NULL || parentDir[0] == '\0' ||
             strcmp(parentDir, currentDir) == 0 ) {
            xrtFree(parentDir);
            break;
        }
        xrtFree(currentDir);
        currentDir = parentDir;
    }
    xrtFree(currentDir);
    return ok;
}

static int xl6PackageTryNamedPackage(XL6_PackageResolveResult* result, const char* importerDir, const char* target)
{
    if ( !xl6PackageCanBePackageName(target) ) {
        return 0;
    }
    if ( xl6PackageTryNamedPackageInBase(result, importerDir, target) ) {
        return 1;
    }
    if ( xl6PackageTryEnvPackagePath(result, target) ) {
        return 1;
    }
    if ( xl6PackageTryExeExtlibsPackage(result, target) ) {
        return 1;
    }
    return 0;
}

void xl6PackageResolveResultInit(XL6_PackageResolveResult* result)
{
    if ( result == NULL ) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->kind = XL6_PACKAGE_RESOLVE_UNRESOLVED;
}

void xl6PackageResolveResultFree(XL6_PackageResolveResult* result)
{
    if ( result == NULL ) {
        return;
    }
    xrtFree(result->target);
    xrtFree(result->manifestPath);
    xrtFree(result->packageDir);
    xrtFree(result->packageName);
    xrtFree(result->packageVersion);
    xrtFree(result->abiVersion);
    xrtFree(result->packageId);
    xl6PackageResolveResultInit(result);
}

const char* xl6PackageResolveKindName(XL6_PackageResolveKind kind)
{
    switch ( kind ) {
    case XL6_PACKAGE_RESOLVE_FILE: return "file";
    case XL6_PACKAGE_RESOLVE_PACKAGE: return "package";
    case XL6_PACKAGE_RESOLVE_UNRESOLVED:
    default:
        return "unresolved";
    }
}

int xl6PackageResolveImport(const char* importerPath, const char* target, XL6_PackageResolveResult* result)
{
    char* importerDir;
    char* path;
    int ok;

    if ( result == NULL ) {
        return 0;
    }
    xl6PackageResolveResultInit(result);
    if ( target == NULL || target[0] == '\0' ) {
        return 0;
    }
    result->target = xl6PackageDupText(target);
    if ( result->target == NULL ) {
        return 0;
    }

    importerDir = xl6PackageDirName(importerPath);
    if ( importerDir == NULL ) {
        xl6PackageResolveResultFree(result);
        return 0;
    }

    if ( xl6PackageTryNamedPackage(result, importerDir, target) ) {
        xrtFree(importerDir);
        return 1;
    }

    if ( xl6PackageLooksLikePath(target) ) {
        if ( xl6PackagePathIsAbsolute(target) ) {
            if ( xl6PackageTryPathCandidates(result, target) ) {
                xrtFree(importerDir);
                return 1;
            }
        } else {
            path = xl6PackageJoinPathAlloc(importerDir, target);
            ok = path != NULL && xl6PackageTryPathCandidates(result, path);
            xrtFree(path);
            if ( ok ) {
                xrtFree(importerDir);
                return 1;
            }
            if ( xl6PackageTryPathCandidates(result, target) ) {
                xrtFree(importerDir);
                return 1;
            }
        }
        xrtFree(importerDir);
        return 0;
    }

    xrtFree(importerDir);
    return 0;
}

