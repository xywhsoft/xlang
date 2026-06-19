#include "src/package/xl6_package.h"
#include "src/common/xl6_common.h"

#include <stdio.h>
#include <string.h>

static void xl6PackageCopyText(char* out, size_t outSize, const char* text)
{
    if ( out == NULL || outSize == 0 ) {
        return;
    }
    snprintf(out, outSize, "%s", text != NULL ? text : "");
}

static void xl6PackageDirName(char* out, size_t outSize, const char* path)
{
    const char* lastSep;
    const char* p;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    out[0] = '\0';
    if ( path == NULL || path[0] == '\0' ) {
        snprintf(out, outSize, ".");
        return;
    }

    lastSep = NULL;
    for ( p = path; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            lastSep = p;
        }
    }
    if ( lastSep == NULL ) {
        snprintf(out, outSize, ".");
        return;
    }

    len = (size_t)(lastSep - path);
    if ( *lastSep == ':' ) {
        len += 1;
    }
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, path, len);
    out[len] = '\0';
}

static int xl6PackageJoin(char* out, size_t outSize, const char* a, const char* b)
{
    const char* sep;
    size_t aLen;
    int written;

    if ( out == NULL || outSize == 0 || a == NULL || b == NULL ) {
        return 0;
    }
    aLen = strlen(a);
    sep = (aLen > 0 && (a[aLen - 1] == '\\' || a[aLen - 1] == '/')) ? "" : "\\";
    written = snprintf(out, outSize, "%s%s%s", a, sep, b);
    return written > 0 && (size_t)written < outSize;
}

static int xl6PackageTryManifest(XL6_PackageResolveResult* result, const char* path, XL6_PackageResolveKind kind)
{
    if ( result == NULL || path == NULL ) {
        return 0;
    }
    if ( !xrtFileExists((char*)path) ) {
        return 0;
    }
    result->resolved = 1;
    result->kind = kind;
    xl6PackageCopyText(result->manifestPath, sizeof(result->manifestPath), path);
    return 1;
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

void xl6PackageResolveResultInit(XL6_PackageResolveResult* result)
{
    if ( result == NULL ) {
        return;
    }
    memset(result, 0, sizeof(*result));
    result->kind = XL6_PACKAGE_RESOLVE_UNRESOLVED;
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
    char importerDir[512];
    char path[512];
    char path2[512];

    if ( result == NULL ) {
        return 0;
    }
    xl6PackageResolveResultInit(result);
    xl6PackageCopyText(result->target, sizeof(result->target), target);
    if ( target == NULL || target[0] == '\0' ) {
        return 0;
    }

    xl6PackageDirName(importerDir, sizeof(importerDir), importerPath);

    if ( xl6PackageLooksLikePath(target) ) {
        if ( xl6PackageTryManifest(result, target, XL6_PACKAGE_RESOLVE_FILE) ) {
            return 1;
        }
        snprintf(path, sizeof(path), "%s.xapi.json", target);
        if ( xl6PackageTryManifest(result, path, XL6_PACKAGE_RESOLVE_FILE) ) {
            return 1;
        }
        if ( xl6PackageJoin(path, sizeof(path), importerDir, target) ) {
            if ( xl6PackageTryManifest(result, path, XL6_PACKAGE_RESOLVE_FILE) ) {
                return 1;
            }
            snprintf(path2, sizeof(path2), "%s.xapi.json", path);
            if ( xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_FILE) ) {
                return 1;
            }
            if ( xl6PackageJoin(path2, sizeof(path2), path, "xlang.package.json") &&
                 xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
                return 1;
            }
            if ( xl6PackageJoin(path2, sizeof(path2), path, "package.xapi.json") &&
                 xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
                return 1;
            }
        }
        return 0;
    }

    if ( xl6PackageJoin(path, sizeof(path), "packages", target) ) {
        if ( xl6PackageJoin(path2, sizeof(path2), path, "xlang.package.json") &&
             xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
            return 1;
        }
        if ( xl6PackageJoin(path2, sizeof(path2), path, "package.xapi.json") &&
             xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
            return 1;
        }
    }
    if ( xl6PackageJoin(path, sizeof(path), "lib", target) ) {
        if ( xl6PackageJoin(path2, sizeof(path2), path, "xlang.package.json") &&
             xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
            return 1;
        }
        if ( xl6PackageJoin(path2, sizeof(path2), path, "package.xapi.json") &&
             xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
            return 1;
        }
    }
    return 0;
}

int xl6PackageResolveInterfacePath(const XL6_PackageResolveResult* result, char* out, size_t outSize)
{
    char dir[512];
    char path[512];

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    out[0] = '\0';
    if ( result == NULL || !result->resolved || result->manifestPath[0] == '\0' ) {
        return 0;
    }
    if ( xl6PackageEndsWith(result->manifestPath, ".xapi.json") ) {
        xl6PackageCopyText(out, outSize, result->manifestPath);
        return 1;
    }
    xl6PackageDirName(dir, sizeof(dir), result->manifestPath);
    if ( xl6PackageJoin(path, sizeof(path), dir, "package.xapi.json") && xrtFileExists((char*)path) ) {
        xl6PackageCopyText(out, outSize, path);
        return 1;
    }
    return 0;
}
