#include "src/api/xl_api_path.h"

#include "src/common/xl6_xrt.h"
#include "src/backend/xl6_toolchain.h"

#include <stdio.h>
#include <string.h>

#if defined(CONFIG_TCC_STATIC) && !defined(XL_STATIC_TCC)
#define XL_STATIC_TCC 1
#endif

#if defined(XL_STATIC_TCC) && !defined(TCC_DISABLE_BUILTIN_VFS)
#define XL_STATIC_TCC_VFS 1
#endif

#ifdef XL_STATIC_TCC_VFS
#include "tcc_builtin_vfs.h"
#endif

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <unistd.h>
#endif

char* xlDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtStrDupN(src, strlen(src));
}

int xlPathIsAbsolute(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( xrtPathIsAbs(path) ) {
        return 1;
    }
#ifdef _WIN32
    return (path[0] == '\\' && path[1] == '\\') || (path[0] == '/' && path[1] == '/');
#else
    return 0;
#endif
}

int xlCopyToBuffer(char* out, size_t outSize, const char* text)
{
    size_t len;

    if ( out == NULL || outSize == 0 || text == NULL ) {
        return 0;
    }
    len = strlen(text);
    if ( len >= outSize ) {
        return 0;
    }
    memcpy(out, text, len + 1u);
    return 1;
}

const char* xlBaseName(const char* path)
{
    const char* base;
    const char* p;

    base = path != NULL ? path : "";
    for ( p = base; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            base = p + 1;
        }
    }
    return base;
}

int xlTextEqualsNoCase(const char* a, const char* b)
{
    size_t i;

    if ( a == NULL || b == NULL ) {
        return 0;
    }
    for ( i = 0; a[i] != '\0' || b[i] != '\0'; ++i ) {
        char ca = a[i];
        char cb = b[i];
        if ( ca >= 'A' && ca <= 'Z' ) {
            ca = (char)(ca - 'A' + 'a');
        }
        if ( cb >= 'A' && cb <= 'Z' ) {
            cb = (char)(cb - 'A' + 'a');
        }
        if ( ca != cb ) {
            return 0;
        }
    }
    return 1;
}

void xlDirName(char* out, size_t outSize, const char* path)
{
    const char* p;
    const char* lastSep;
    size_t len;

    if ( out == NULL || outSize == 0 ) {
        return;
    }
    out[0] = '\0';
    if ( path == NULL || path[0] == '\0' ) {
        return;
    }
    lastSep = NULL;
    for ( p = path; *p != '\0'; ++p ) {
        if ( *p == '\\' || *p == '/' || *p == ':' ) {
            lastSep = p;
        }
    }
    if ( lastSep == NULL ) {
        return;
    }
    len = (size_t)(lastSep - path);
    if ( *lastSep == ':' ) {
        len += 1u;
    }
    if ( len >= outSize ) {
        len = outSize - 1u;
    }
    memcpy(out, path, len);
    out[len] = '\0';
}

int xlJoinPathBuffer(char* out, size_t outSize, const char* dir, const char* name)
{
    const char* sep;
    size_t len;
    int written;

    if ( out == NULL || outSize == 0 || dir == NULL || name == NULL ) {
        return 0;
    }
    len = strlen(dir);
    sep = (len > 0 && (dir[len - 1u] == '\\' || dir[len - 1u] == '/')) ?
          "" :
#ifdef _WIN32
          "\\";
#else
          "/";
#endif
    written = snprintf(out, outSize, "%s%s%s", dir, sep, name);
    return written > 0 && (size_t)written < outSize;
}

int xlGetModuleDir(char* out, size_t outSize)
{
    char path[512];

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    out[0] = '\0';
#ifdef _WIN32
    {
        HMODULE module = NULL;
        DWORD len;
        if ( !GetModuleHandleExA(
                 GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                 (LPCSTR)&xlGetModuleDir,
                 &module) ) {
            module = NULL;
        }
        len = GetModuleFileNameA(module, path, (DWORD)sizeof(path));
        if ( len == 0 || len >= sizeof(path) ) {
            return 0;
        }
        path[len] = '\0';
    }
#else
    {
        Dl_info info;
        if ( dladdr((void*)&xlGetModuleDir, &info) && info.dli_fname != NULL && info.dli_fname[0] != '\0' ) {
            if ( !xlCopyToBuffer(path, sizeof(path), info.dli_fname) ) {
                return 0;
            }
        } else {
            ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1u);
            if ( len <= 0 || (size_t)len >= sizeof(path) ) {
                return 0;
            }
            path[len] = '\0';
        }
    }
#endif
    xlDirName(out, outSize, path);
    return out[0] != '\0';
}

int xlGetProjectRoot(char* out, size_t outSize)
{
    char moduleDir[512];

    if ( out == NULL || outSize == 0 ) {
        return 0;
    }
    out[0] = '\0';
    if ( !xlGetModuleDir(moduleDir, sizeof(moduleDir)) ) {
        return 0;
    }
    if ( xlTextEqualsNoCase(xlBaseName(moduleDir), "release") ) {
        xlDirName(out, outSize, moduleDir);
        return out[0] != '\0';
    }
    return xlCopyToBuffer(out, outSize, moduleDir);
}

int xlTextLooksLikePath(const char* text)
{
    return text != NULL &&
           (strchr(text, '\\') != NULL || strchr(text, '/') != NULL ||
            strchr(text, ':') != NULL || strchr(text, '.') != NULL);
}

char* xlPathSnapshot(const char* baseDir, const char* path)
{
    str joined;

    if ( path == NULL || path[0] == '\0' ) {
        return xlDupText("");
    }
    if ( xlPathIsAbsolute(path) || baseDir == NULL || baseDir[0] == '\0' ) {
        return xlDupText(path);
    }
    joined = xrtPathJoin(baseDir, path);
    return joined != NULL ? (char*)joined : NULL;
}

char* xlToolchainPathSnapshot(const char* baseDir, const char* path)
{
#ifdef XL_STATIC_TCC_VFS
    if ( path != NULL &&
         (strcmp(path, XL6_TCC_ROOT_PATH) == 0 ||
          strcmp(path, XL6_TCC_INCLUDE_PATH) == 0 ||
          strcmp(path, XL6_TCC_WIN_INCLUDE_PATH) == 0 ||
          strcmp(path, XL6_TCC_WINAPI_INCLUDE_PATH) == 0 ||
          strcmp(path, XL6_TCC_LIB_PATH) == 0 ||
          strcmp(path, XL6_TCC_LIBTCC1_PATH) == 0) ) {
        /* 静态 TCC 的默认工具链路径是 VFS 虚拟路径，不能展开成宿主文件系统绝对路径。 */
        return xlDupText(path);
    }
#endif
    return xlPathSnapshot(baseDir, path);
}

char* xlDefaultTempDirSnapshot(const char* baseDir)
{
#ifdef _WIN32
    return xlPathSnapshot(baseDir, "build\\tmp");
#else
    return xlPathSnapshot(baseDir, "build/tmp");
#endif
}
