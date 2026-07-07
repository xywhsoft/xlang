#include "src/package/xl6_package.h"
#include "src/common/xl6_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

static char* xl6PackageDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtCopyStr((char*)src, strlen(src));
}

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

int xl6PackageJoinPath(char* out, size_t outSize, const char* a, const char* b)
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

static void xl6PackageBaseNameNoExt(char* out, size_t outSize, const char* path)
{
    const char* base;
    const char* p;
    const char* dot;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    out[0] = '\0';

    base = path != NULL ? path : "package";
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            base = p + 1;
        }
    }
    dot = NULL;
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '.' ) {
            dot = p;
        }
    }
    len = dot != NULL ? (size_t)(dot - base) : strlen(base);
    if ( len == 0 ) {
        base = "package";
        len = strlen(base);
    }
    if ( len >= outSize ) {
        len = outSize - 1;
    }
    memcpy(out, base, len);
    out[len] = '\0';
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

static void xl6PackageMakePackageId(char* out, size_t outSize, const char* name, const char* version, const char* abiVersion)
{
    if ( out == NULL || outSize == 0 ) {
        return;
    }
    snprintf(out, outSize, "%s@%s#abi%s",
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
    xl6PackageStringListFree(&manifest->xlSources);
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

typedef struct XL6_PackageJson {
    const char* text;
    size_t len;
    size_t pos;
} XL6_PackageJson;

static void xl6PackageJsonSkipWs(XL6_PackageJson* json)
{
    while ( json != NULL && json->pos < json->len ) {
        char ch = json->text[json->pos];
        if ( ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n' ) {
            break;
        }
        json->pos += 1;
    }
}

static int xl6PackageJsonPeek(XL6_PackageJson* json)
{
    xl6PackageJsonSkipWs(json);
    if ( json == NULL || json->pos >= json->len ) {
        return 0;
    }
    return (unsigned char)json->text[json->pos];
}

static int xl6PackageJsonConsume(XL6_PackageJson* json, char ch)
{
    xl6PackageJsonSkipWs(json);
    if ( json == NULL || json->pos >= json->len || json->text[json->pos] != ch ) {
        return 0;
    }
    json->pos += 1;
    return 1;
}

static int xl6PackageJsonAppendChar(char** out, size_t* len, size_t* cap, char ch)
{
    char* newData;
    size_t newCap;

    if ( out == NULL || len == NULL || cap == NULL ) {
        return 0;
    }
    if ( *len + 2 > *cap ) {
        newCap = *cap == 0 ? 32 : *cap * 2;
        while ( *len + 2 > newCap ) {
            newCap *= 2;
        }
        newData = (char*)xrtRealloc(*out, newCap);
        if ( newData == NULL ) {
            return 0;
        }
        *out = newData;
        *cap = newCap;
    }
    (*out)[*len] = ch;
    *len += 1;
    (*out)[*len] = '\0';
    return 1;
}

static char* xl6PackageJsonReadString(XL6_PackageJson* json)
{
    char* out;
    size_t len;
    size_t cap;

    if ( !xl6PackageJsonConsume(json, '"') ) {
        return NULL;
    }
    out = NULL;
    len = 0;
    cap = 0;
    while ( json->pos < json->len ) {
        char ch = json->text[json->pos++];
        if ( ch == '"' ) {
            return out != NULL ? out : xl6PackageDupText("");
        }
        if ( ch == '\\' && json->pos < json->len ) {
            ch = json->text[json->pos++];
            switch ( ch ) {
            case '"': ch = '"'; break;
            case '\\': ch = '\\'; break;
            case '/': ch = '/'; break;
            case 'b': ch = '\b'; break;
            case 'f': ch = '\f'; break;
            case 'n': ch = '\n'; break;
            case 'r': ch = '\r'; break;
            case 't': ch = '\t'; break;
            case 'u':
                if ( json->pos + 4 <= json->len ) {
                    json->pos += 4;
                }
                ch = '?';
                break;
            default:
                break;
            }
        }
        if ( !xl6PackageJsonAppendChar(&out, &len, &cap, ch) ) {
            xrtFree(out);
            return NULL;
        }
    }
    xrtFree(out);
    return NULL;
}

static int xl6PackageJsonReadBool(XL6_PackageJson* json, int* outValue)
{
    xl6PackageJsonSkipWs(json);
    if ( json->pos + 4 <= json->len && memcmp(json->text + json->pos, "true", 4) == 0 ) {
        json->pos += 4;
        if ( outValue != NULL ) {
            *outValue = 1;
        }
        return 1;
    }
    if ( json->pos + 5 <= json->len && memcmp(json->text + json->pos, "false", 5) == 0 ) {
        json->pos += 5;
        if ( outValue != NULL ) {
            *outValue = 0;
        }
        return 1;
    }
    return 0;
}

static int xl6PackageJsonSkipValue(XL6_PackageJson* json);

static int xl6PackageJsonSkipObject(XL6_PackageJson* json)
{
    if ( !xl6PackageJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6PackageJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6PackageJsonReadString(json);
        if ( key == NULL ) {
            return 0;
        }
        xrtFree(key);
        if ( !xl6PackageJsonConsume(json, ':') || !xl6PackageJsonSkipValue(json) ) {
            return 0;
        }
        if ( xl6PackageJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6PackageJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6PackageJsonSkipArray(XL6_PackageJson* json)
{
    if ( !xl6PackageJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6PackageJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        if ( !xl6PackageJsonSkipValue(json) ) {
            return 0;
        }
        if ( xl6PackageJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6PackageJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6PackageJsonSkipValue(XL6_PackageJson* json)
{
    int ch;

    ch = xl6PackageJsonPeek(json);
    if ( ch == '"' ) {
        char* text = xl6PackageJsonReadString(json);
        xrtFree(text);
        return text != NULL;
    }
    if ( ch == '{' ) {
        return xl6PackageJsonSkipObject(json);
    }
    if ( ch == '[' ) {
        return xl6PackageJsonSkipArray(json);
    }
    if ( ch == 't' || ch == 'f' ) {
        return xl6PackageJsonReadBool(json, NULL);
    }
    if ( ch == 'n' && json->pos + 4 <= json->len && memcmp(json->text + json->pos, "null", 4) == 0 ) {
        json->pos += 4;
        return 1;
    }
    xl6PackageJsonSkipWs(json);
    while ( json->pos < json->len ) {
        ch = (unsigned char)json->text[json->pos];
        if ( ch == ',' || ch == ']' || ch == '}' || ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' ) {
            break;
        }
        json->pos += 1;
    }
    return 1;
}

static int xl6PackageManifestReadStringArray(XL6_PackageJson* json, XL6_PackageStringList* list)
{
    if ( !xl6PackageJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6PackageJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        char* text = xl6PackageJsonReadString(json);
        int ok = text != NULL && xl6PackageStringListAppend(list, text);
        xrtFree(text);
        if ( !ok ) {
            return 0;
        }
        if ( xl6PackageJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6PackageJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6PackageManifestAppendDependency(
    XL6_PackageManifest* manifest,
    const char* name,
    const char* version,
    const char* abiVersion)
{
    XL6_PackageDependency* newItems;
    XL6_PackageDependency* dep;
    char id[256];

    if ( manifest == NULL || name == NULL || name[0] == '\0' ) {
        return 0;
    }
    newItems = (XL6_PackageDependency*)xrtRealloc(
        manifest->dependencies,
        (manifest->dependencyCount + 1) * sizeof(XL6_PackageDependency));
    if ( newItems == NULL ) {
        return 0;
    }
    manifest->dependencies = newItems;
    dep = &manifest->dependencies[manifest->dependencyCount];
    memset(dep, 0, sizeof(*dep));
    dep->name = xl6PackageDupText(name);
    dep->version = xl6PackageDupText(version != NULL && version[0] != '\0' ? version : "0.0.0");
    dep->abiVersion = xl6PackageDupText(abiVersion != NULL && abiVersion[0] != '\0' ? abiVersion : "1");
    xl6PackageMakePackageId(id, sizeof(id), dep->name, dep->version, dep->abiVersion);
    dep->packageId = xl6PackageDupText(id);
    if ( dep->name == NULL || dep->version == NULL || dep->abiVersion == NULL || dep->packageId == NULL ) {
        return 0;
    }
    manifest->dependencyCount += 1;
    return 1;
}

static int xl6PackageManifestReadDependencyString(XL6_PackageJson* json, XL6_PackageManifest* manifest)
{
    char* name;
    int ok;

    name = xl6PackageJsonReadString(json);
    ok = name != NULL && xl6PackageManifestAppendDependency(manifest, name, "*", "*");
    xrtFree(name);
    return ok;
}

static int xl6PackageManifestReadDependencyObject(XL6_PackageJson* json, XL6_PackageManifest* manifest)
{
    char* name;
    char* version;
    char* abiVersion;
    int ok;

    name = NULL;
    version = NULL;
    abiVersion = NULL;
    if ( !xl6PackageJsonConsume(json, '{') ) {
        return 0;
    }
    if ( !xl6PackageJsonConsume(json, '}') ) {
        for (;;) {
            char* key = xl6PackageJsonReadString(json);
            if ( key == NULL || !xl6PackageJsonConsume(json, ':') ) {
                xrtFree(key);
                xrtFree(name);
                xrtFree(version);
                xrtFree(abiVersion);
                return 0;
            }
            if ( strcmp(key, "name") == 0 ) {
                xrtFree(name);
                name = xl6PackageJsonReadString(json);
            } else if ( strcmp(key, "version") == 0 ) {
                xrtFree(version);
                version = xl6PackageJsonReadString(json);
            } else if ( strcmp(key, "abiVersion") == 0 ) {
                xrtFree(abiVersion);
                abiVersion = xl6PackageJsonReadString(json);
            } else if ( !xl6PackageJsonSkipValue(json) ) {
                xrtFree(key);
                xrtFree(name);
                xrtFree(version);
                xrtFree(abiVersion);
                return 0;
            }
            xrtFree(key);
            if ( xl6PackageJsonConsume(json, '}') ) {
                break;
            }
            if ( !xl6PackageJsonConsume(json, ',') ) {
                xrtFree(name);
                xrtFree(version);
                xrtFree(abiVersion);
                return 0;
            }
        }
    }
    ok = xl6PackageManifestAppendDependency(manifest, name, version, abiVersion);
    xrtFree(name);
    xrtFree(version);
    xrtFree(abiVersion);
    return ok;
}

static int xl6PackageManifestReadDependencies(XL6_PackageJson* json, XL6_PackageManifest* manifest)
{
    if ( !xl6PackageJsonConsume(json, '[') ) {
        return 0;
    }
    if ( xl6PackageJsonConsume(json, ']') ) {
        return 1;
    }
    for (;;) {
        int ok;

        xl6PackageJsonSkipWs(json);
        if ( json->pos < json->len && json->text[json->pos] == '"' ) {
            ok = xl6PackageManifestReadDependencyString(json, manifest);
        } else {
            ok = xl6PackageManifestReadDependencyObject(json, manifest);
        }
        if ( !ok ) {
            return 0;
        }
        if ( xl6PackageJsonConsume(json, ']') ) {
            return 1;
        }
        if ( !xl6PackageJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6PackageManifestReadArtifact(XL6_PackageJson* json, XL6_PackageManifest* manifest)
{
    if ( !xl6PackageJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6PackageJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6PackageJsonReadString(json);
        if ( key == NULL || !xl6PackageJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "cSources") == 0 ) {
            if ( !xl6PackageManifestReadStringArray(json, &manifest->cSources) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "xlSources") == 0 ) {
            if ( !xl6PackageManifestReadStringArray(json, &manifest->xlSources) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6PackageJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6PackageJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6PackageJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6PackageManifestReadNative(XL6_PackageJson* json, XL6_PackageManifest* manifest)
{
    if ( !xl6PackageJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6PackageJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6PackageJsonReadString(json);
        if ( key == NULL || !xl6PackageJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "singleton") == 0 ) {
            if ( !xl6PackageJsonReadBool(json, &manifest->nativeInfo.singleton) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "loadName") == 0 ) {
            xrtFree(manifest->nativeInfo.loadName);
            manifest->nativeInfo.loadName = xl6PackageJsonReadString(json);
        } else if ( strcmp(key, "version") == 0 ) {
            xrtFree(manifest->nativeInfo.version);
            manifest->nativeInfo.version = xl6PackageJsonReadString(json);
        } else if ( !xl6PackageJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6PackageJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6PackageJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

static int xl6PackageManifestParseRoot(XL6_PackageJson* json, XL6_PackageManifest* manifest)
{
    if ( !xl6PackageJsonConsume(json, '{') ) {
        return 0;
    }
    if ( xl6PackageJsonConsume(json, '}') ) {
        return 1;
    }
    for (;;) {
        char* key = xl6PackageJsonReadString(json);
        if ( key == NULL || !xl6PackageJsonConsume(json, ':') ) {
            xrtFree(key);
            return 0;
        }
        if ( strcmp(key, "format") == 0 ) {
            xrtFree(manifest->format);
            manifest->format = xl6PackageJsonReadString(json);
        } else if ( strcmp(key, "name") == 0 ) {
            xrtFree(manifest->name);
            manifest->name = xl6PackageJsonReadString(json);
        } else if ( strcmp(key, "version") == 0 ) {
            xrtFree(manifest->version);
            manifest->version = xl6PackageJsonReadString(json);
        } else if ( strcmp(key, "abiVersion") == 0 ) {
            xrtFree(manifest->abiVersion);
            manifest->abiVersion = xl6PackageJsonReadString(json);
        } else if ( strcmp(key, "artifact") == 0 ) {
            if ( !xl6PackageManifestReadArtifact(json, manifest) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "links") == 0 ) {
            if ( !xl6PackageManifestReadStringArray(json, &manifest->links) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "loadlibs") == 0 ) {
            if ( !xl6PackageManifestReadStringArray(json, &manifest->loadlibs) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "cSources") == 0 ) {
            if ( !xl6PackageManifestReadStringArray(json, &manifest->cSources) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "xlSources") == 0 ) {
            if ( !xl6PackageManifestReadStringArray(json, &manifest->xlSources) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "dependencies") == 0 ) {
            if ( !xl6PackageManifestReadDependencies(json, manifest) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( strcmp(key, "native") == 0 ) {
            if ( !xl6PackageManifestReadNative(json, manifest) ) {
                xrtFree(key);
                return 0;
            }
        } else if ( !xl6PackageJsonSkipValue(json) ) {
            xrtFree(key);
            return 0;
        }
        xrtFree(key);
        if ( xl6PackageJsonConsume(json, '}') ) {
            return 1;
        }
        if ( !xl6PackageJsonConsume(json, ',') ) {
            return 0;
        }
    }
}

int xl6PackageLoadManifest(const char* path, XL6_PackageManifest* manifest)
{
    char* text;
    size_t textLen;
    XL6_PackageJson json;
    char dir[512];
    char fallbackName[128];
    char packageId[256];
    int ok;

    if ( path == NULL || manifest == NULL ) {
        return 0;
    }

    xl6PackageManifestFree(manifest);
    xl6PackageManifestInit(manifest);

    xrtClearError();
    text = xrtFileReadAll((char*)path, XRT_CP_UTF8, &textLen);
    if ( xrtGetError()[0] != '\0' || text == NULL ) {
        xrtFree(text);
        return 0;
    }
    memset(&json, 0, sizeof(json));
    json.text = text;
    json.len = textLen;
    if ( json.len >= 3 &&
         (unsigned char)json.text[0] == 0xEF &&
         (unsigned char)json.text[1] == 0xBB &&
         (unsigned char)json.text[2] == 0xBF ) {
        json.pos = 3;
    }
    ok = xl6PackageManifestParseRoot(&json, manifest);
    xrtFree(text);
    xl6PackageDirName(dir, sizeof(dir), path);
    xl6PackageBaseNameNoExt(fallbackName, sizeof(fallbackName), dir);
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
        manifest->dir = xl6PackageDupText(dir);
        xl6PackageMakePackageId(packageId, sizeof(packageId), manifest->name, manifest->version, manifest->abiVersion);
        manifest->packageId = xl6PackageDupText(packageId);
        if ( manifest->name == NULL || manifest->version == NULL || manifest->abiVersion == NULL ||
             manifest->dir == NULL || manifest->packageId == NULL ) {
            ok = 0;
        }
    }
    if ( !ok ) {
        xl6PackageManifestFree(manifest);
    }
    return ok;
}

static int xl6PackageFinalizeManifestResult(XL6_PackageResolveResult* result)
{
    XL6_PackageManifest manifest;
    int ok;

    xl6PackageManifestInit(&manifest);
    if ( !xl6PackageLoadManifest(result->manifestPath, &manifest) ) {
        xl6PackageManifestFree(&manifest);
        return 0;
    }
    ok = 1;
    xl6PackageCopyText(result->packageDir, sizeof(result->packageDir), manifest.dir);
    xl6PackageCopyText(result->packageName, sizeof(result->packageName), manifest.name);
    xl6PackageCopyText(result->packageVersion, sizeof(result->packageVersion), manifest.version);
    xl6PackageCopyText(result->abiVersion, sizeof(result->abiVersion), manifest.abiVersion);
    xl6PackageCopyText(result->packageId, sizeof(result->packageId), manifest.packageId);
    xl6PackageManifestFree(&manifest);
    return ok;
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
    if ( !xl6PackageFinalizeManifestResult(result) ) {
        xl6PackageResolveResultInit(result);
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
    char path2[512];

    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( xl6PackageTryManifest(result, path, XL6_PACKAGE_RESOLVE_FILE) ) {
        return 1;
    }
    if ( xl6PackageJoinPath(path2, sizeof(path2), path, "xlang.package.json") &&
         xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
        return 1;
    }
    return 0;
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
    char path[512];
    char path2[512];

    if ( baseDir == NULL || baseDir[0] == '\0' || !xl6PackageCanBePackageName(target) ) {
        return 0;
    }
    if ( xl6PackageJoinPath(path, sizeof(path), baseDir, target) ) {
        if ( xl6PackageJoinPath(path2, sizeof(path2), path, "xlang.package.json") &&
             xl6PackageTryManifest(result, path2, XL6_PACKAGE_RESOLVE_PACKAGE) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6PackageTryEnvPackagePath(XL6_PackageResolveResult* result, const char* target)
{
    const char* env;
    const char* start;
#if defined(_WIN32) || defined(_WIN64)
    const char sep = ';';
#else
    const char sep = ':';
#endif

    env = getenv("XLANG_PACKAGE_PATH");
    if ( env == NULL || env[0] == '\0' ) {
        return 0;
    }
    start = env;
    for (;;) {
        const char* end = strchr(start, sep);
        size_t len = end != NULL ? (size_t)(end - start) : strlen(start);
        char baseDir[512];

        if ( len > 0 ) {
            if ( len >= sizeof(baseDir) ) {
                len = sizeof(baseDir) - 1;
            }
            memcpy(baseDir, start, len);
            baseDir[len] = '\0';
            if ( xl6PackageTryNamedPackageInBase(result, baseDir, target) ) {
                return 1;
            }
        }
        if ( end == NULL ) {
            break;
        }
        start = end + 1;
    }
    return 0;
}

static int xl6PackageGetExecutableDir(char* out, size_t outSize)
{
    char path[512];

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    out[0] = '\0';
#if defined(_WIN32) || defined(_WIN64)
    {
        DWORD len = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
        if ( len == 0 || len >= sizeof(path) ) {
            return 0;
        }
        path[len] = '\0';
    }
#else
    {
        ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
        if ( len <= 0 || (size_t)len >= sizeof(path) ) {
            return 0;
        }
        path[len] = '\0';
    }
#endif
    xl6PackageDirName(out, outSize, path);
    return out[0] != '\0';
}

static int xl6PackageTryExeExtlibsPackage(XL6_PackageResolveResult* result, const char* target)
{
    char exeDir[512];
    char extlibsDir[512];

    if ( !xl6PackageGetExecutableDir(exeDir, sizeof(exeDir)) ) {
        return 0;
    }
    if ( !xl6PackageJoinPath(extlibsDir, sizeof(extlibsDir), exeDir, "extlibs") ) {
        return 0;
    }
    return xl6PackageTryNamedPackageInBase(result, extlibsDir, target);
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

    if ( result == NULL ) {
        return 0;
    }
    xl6PackageResolveResultInit(result);
    xl6PackageCopyText(result->target, sizeof(result->target), target);
    if ( target == NULL || target[0] == '\0' ) {
        return 0;
    }

    xl6PackageDirName(importerDir, sizeof(importerDir), importerPath);

    if ( xl6PackageTryNamedPackage(result, importerDir, target) ) {
        return 1;
    }

    if ( xl6PackageLooksLikePath(target) ) {
        if ( xl6PackagePathIsAbsolute(target) ) {
            if ( xl6PackageTryPathCandidates(result, target) ) {
                return 1;
            }
        } else {
            if ( xl6PackageJoinPath(path, sizeof(path), importerDir, target) &&
                 xl6PackageTryPathCandidates(result, path) ) {
                return 1;
            }
            if ( xl6PackageTryPathCandidates(result, target) ) {
                return 1;
            }
        }
        return 0;
    }

    return 0;
}

