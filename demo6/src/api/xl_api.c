#include "include/xl.h"

#include "lib/xrt/xrt.h"
#include "libtcc.h"
#include "src/abi/xl6_abi_dump.h"
#include "src/ast/xl6_ast_dump.h"
#include "src/backend/xl6_toolchain.h"
#include "src/cgen/xl6_cgen.h"
#include "src/common/xl6_common.h"
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_xrt_trim.h"
#include "src/diag/xl6_diag.h"
#include "src/ir/xl6_ir_dump.h"
#include "src/lexer/xl6_lexer_api.h"

#include <stdio.h>
#include <stdlib.h>
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

#ifndef XL_ENABLE_EXE_COMPILER
# if defined(XL_RUNTIME_ONLY_DLL)
#  define XL_ENABLE_EXE_COMPILER 0
# else
#  define XL_ENABLE_EXE_COMPILER 1
# endif
#endif

#ifdef _WIN32
#include <windows.h>
int tcc_tool_impdef(int argc, char** argv);
#else
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#endif

typedef TCCState* (*XL_TccNewProc)(void);
typedef void (*XL_TccDeleteProc)(TCCState* s);
typedef void (*XL_TccSetLibPathProc)(TCCState* s, const char* path);
typedef void (*XL_TccSetErrorFuncProc)(TCCState* s, void* opaque, TCCErrorFunc* errorFunc);
typedef int (*XL_TccSetOptionsProc)(TCCState* s, const char* options);
typedef int (*XL_TccAddIncludePathProc)(TCCState* s, const char* path);
typedef int (*XL_TccAddSysIncludePathProc)(TCCState* s, const char* path);
typedef void (*XL_TccDefineSymbolProc)(TCCState* s, const char* sym, const char* value);
typedef void (*XL_TccUndefineSymbolProc)(TCCState* s, const char* sym);
typedef int (*XL_TccAddLibraryPathProc)(TCCState* s, const char* path);
typedef int (*XL_TccAddLibraryProc)(TCCState* s, const char* name);
typedef int (*XL_TccAddFileProc)(TCCState* s, const char* path);
typedef int (*XL_TccAddSymbolProc)(TCCState* s, const char* name, const void* value);
typedef int (*XL_TccSetOutputTypeProc)(TCCState* s, int outputType);
typedef int (*XL_TccCompileStringProc)(TCCState* s, const char* source);
typedef int (*XL_TccOutputFileProc)(TCCState* s, const char* path);
typedef int (*XL_TccRunProc)(TCCState* s, int argc, char** argv);
typedef int (*XL_TccRelocateProc)(TCCState* s);
typedef void* (*XL_TccGetSymbolProc)(TCCState* s, const char* name);
typedef void (*XL_TccListSymbolsProc)(TCCState* s, void* ctx, void (*symbol_cb)(void* ctx, const char* name, const void* value));
typedef void (*XL_TccSetBacktraceFuncProc)(TCCState* s, void* userdata, TCCBtFunc* func);
typedef int (*XL_XlangMainProc)(xvalue arg);
typedef int (*XL_CMainProc)(int argc, char** argv);
typedef int (*XL_ScriptHasErrorProc)(void);
typedef void (*XL_ScriptVoidProc)(void);

typedef struct XL_TccApi {
    int ready;
    char* path;
#ifdef _WIN32
    HMODULE module;
#else
    void* module;
#endif
    XL_TccNewProc tcc_new;
    XL_TccDeleteProc tcc_delete;
    XL_TccSetLibPathProc tcc_set_lib_path;
    XL_TccSetErrorFuncProc tcc_set_error_func;
    XL_TccSetOptionsProc tcc_set_options;
    XL_TccAddIncludePathProc tcc_add_include_path;
    XL_TccAddSysIncludePathProc tcc_add_sysinclude_path;
    XL_TccDefineSymbolProc tcc_define_symbol;
    XL_TccUndefineSymbolProc tcc_undefine_symbol;
    XL_TccAddLibraryPathProc tcc_add_library_path;
    XL_TccAddLibraryProc tcc_add_library;
    XL_TccAddFileProc tcc_add_file;
    XL_TccAddSymbolProc tcc_add_symbol;
    XL_TccSetOutputTypeProc tcc_set_output_type;
    XL_TccCompileStringProc tcc_compile_string;
    XL_TccOutputFileProc tcc_output_file;
    XL_TccRunProc tcc_run;
    XL_TccRelocateProc tcc_relocate;
    XL_TccGetSymbolProc tcc_get_symbol;
    XL_TccListSymbolsProc tcc_list_symbols;
    XL_TccSetBacktraceFuncProc tcc_set_backtrace_func;
} XL_TccApi;

typedef struct XL_StringList {
    char** items;
    size_t count;
} XL_StringList;

typedef struct XL_CSymbol {
    char* name;
    void* value;
} XL_CSymbol;

typedef struct XL_LoadedLibrary {
    char* path;
#ifdef _WIN32
    HMODULE module;
#else
    void* module;
#endif
} XL_LoadedLibrary;

struct XL_Context {
    char* baseDir;
    char* workDir;
    char* tempDir;
    char* tccDllPath;
    char* tccLibPath;
    char* tccIncludePath;
    char* tccWinIncludePath;
    char* tccWinapiIncludePath;
    char* tccLibSearchPath;
    char* tccLibtcc1Path;
    int saveTemps;
    char* lastError;
    int lastExitCode;
    XL_StringList includePaths;
    XL_StringList libraryPaths;
    XL_StringList linkLibraries;
    XL_CSymbol* symbols;
    size_t symbolCount;
    XL_LoadedLibrary* loadedLibraries;
    size_t loadedLibraryCount;
};

struct XL_Module {
    TCCState* state;
    int ownsRuntimeRef;
};

#if XL_ENABLE_EXE_COMPILER
typedef enum XLC_SourceKind {
    XLC_SOURCE_NONE = 0,
    XLC_SOURCE_FILE,
    XLC_SOURCE_STRING
} XLC_SourceKind;

typedef XL_ContextOptions XLC_ContextOptions;

typedef struct XLC_Context {
    XL_Context* runtime;
    XLC_SourceKind sourceKind;
    char* sourcePath;
    char* sourceText;
    char* virtualPath;
    XL_StringList cSourceFiles;
    XL6_CompileUnit unit;
    int unitReady;
} XLC_Context;

XL_API XLC_ContextOptions xlcDefaultContextOptions(void);
XL_API XLC_Context* xlcCreateContext(const XLC_ContextOptions* options);
XL_API void xlcDestroyContext(XLC_Context* ctx);
XL_API int xlcSetWorkDir(XLC_Context* ctx, const char* path);
XL_API int xlcSetTempDir(XLC_Context* ctx, const char* path);
XL_API int xlcSetSaveTemps(XLC_Context* ctx, int enabled);
XL_API int xlcAddIncludePath(XLC_Context* ctx, const char* path);
XL_API int xlcAddLibraryPath(XLC_Context* ctx, const char* path);
XL_API int xlcAddLinkLibrary(XLC_Context* ctx, const char* nameOrPath);
XL_API int xlcAddCSourceFile(XLC_Context* ctx, const char* path);
XL_API int xlcLoadDependencyLibrary(XLC_Context* ctx, const char* path);
XL_API int xlcRegisterCFunction(XLC_Context* ctx, const char* symbolName, void* fn);
XL_API int xlcAddSourceFile(XLC_Context* ctx, const char* path);
XL_API int xlcAddSourceString(XLC_Context* ctx, const char* virtualPath, const char* source);
XL_API int xlcCompile(XLC_Context* ctx);
XL_API int xlcCheck(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XL_API int xlcEmitTokens(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XL_API int xlcEmitAst(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XL_API int xlcEmitIr(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XL_API int xlcEmitC(XLC_Context* ctx, const char* outputPath);
XL_API int xlcEmitDebugC(XLC_Context* ctx, const char* outputPath, const char* debugSourcePath);
XL_API int xlcEmitHeader(XLC_Context* ctx, const char* outputPath, const char* guardName);
XL_API int xlcEmitAbi(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XL_API int xlcEmitExe(XLC_Context* ctx, const char* outputExe);
XL_API int xlcEmitDebugExe(XLC_Context* ctx, const char* outputExe, const char* debugSourcePath);
XL_API int xlcEmitSharedLibrary(XLC_Context* ctx, const char* outputDll);
XL_API XL_Context* xlcRuntimeContext(XLC_Context* ctx);
XL_API XL_Module* xlcRelocateModule(XLC_Context* ctx);
XL_API int xlcRun(XLC_Context* ctx, int argc, char** argv);
XL_API const char* xlcGetLastError(XLC_Context* ctx);
XL_API void xlcClearLastError(XLC_Context* ctx);
XL_API int xlcGetLastExitCode(XLC_Context* ctx);
#endif

typedef struct XL_CwdScope {
    int active;
    int locked;
#ifdef _WIN32
    wchar_t* oldDir;
#else
    char* oldDir;
#endif
} XL_CwdScope;

static XL_TccApi xl__tcc;
static int xl__runtimeManualCount = 0;
static int xl__runtimeContextCount = 0;
static const float xl__mzerosf = -0.0f;
static const double xl__mzerodf = -0.0;

#define XL_TCC_VFS_LIBTCC1 "lib/libtcc1.a"
#define XL_TCC_VFS_MODULE_C "__xlvfs__/module.c"
#define XL_TCC_VFS_MODULE_H "__xlvfs__/module.h"
#define XL_TCC_VFS_C_MODULE_C "__xlvfs__/c_module.c"
#define XL_TCC_VFS_RUNTIME_C "__xlvfs__/runtime.c"
#define XL_TCC_VFS_XRT_C "__xlvfs__/xrt_impl.c"
#define XLC_MAX_C_SOURCE_FILES 32

/* DLL API 会修改全局 TCC 状态和进程 cwd，公开入口必须串行化。 */
#ifdef _WIN32
static CRITICAL_SECTION xl__apiLock;
static volatile LONG xl__apiLockState = 0;

static void xlApiEnsureLock(void)
{
    LONG state;

    state = InterlockedCompareExchange(&xl__apiLockState, 1, 0);
    if ( state == 0 ) {
        InitializeCriticalSection(&xl__apiLock);
        InterlockedCompareExchange(&xl__apiLockState, 2, 1);
        return;
    }
    while ( xl__apiLockState != 2 ) {
        Sleep(0);
    }
}

static void xlApiLock(void)
{
    xlApiEnsureLock();
    EnterCriticalSection(&xl__apiLock);
}

static void xlApiUnlock(void)
{
    LeaveCriticalSection(&xl__apiLock);
}

static CRITICAL_SECTION xl__cwdLock;
static volatile LONG xl__cwdLockState = 0;

static void xlCwdEnsureLock(void)
{
    LONG state;

    state = InterlockedCompareExchange(&xl__cwdLockState, 1, 0);
    if ( state == 0 ) {
        InitializeCriticalSection(&xl__cwdLock);
        InterlockedCompareExchange(&xl__cwdLockState, 2, 1);
        return;
    }
    while ( xl__cwdLockState != 2 ) {
        Sleep(0);
    }
}

static void xlCwdLock(void)
{
    xlCwdEnsureLock();
    EnterCriticalSection(&xl__cwdLock);
}

static void xlCwdUnlock(void)
{
    LeaveCriticalSection(&xl__cwdLock);
}
#else
static pthread_mutex_t xl__apiLock;
static pthread_once_t xl__apiLockOnce = PTHREAD_ONCE_INIT;

static void xlApiInitLock(void)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&xl__apiLock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void xlApiLock(void)
{
    pthread_once(&xl__apiLockOnce, xlApiInitLock);
    pthread_mutex_lock(&xl__apiLock);
}

static void xlApiUnlock(void)
{
    pthread_mutex_unlock(&xl__apiLock);
}

static pthread_mutex_t xl__cwdLock;
static pthread_once_t xl__cwdLockOnce = PTHREAD_ONCE_INIT;

static void xlCwdInitLock(void)
{
    pthread_mutexattr_t attr;

    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&xl__cwdLock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void xlCwdLock(void)
{
    pthread_once(&xl__cwdLockOnce, xlCwdInitLock);
    pthread_mutex_lock(&xl__cwdLock);
}

static void xlCwdUnlock(void)
{
    pthread_mutex_unlock(&xl__cwdLock);
}
#endif

static char* xlDupText(const char* text)
{
    const char* src = text != NULL ? text : "";
    return xrtCopyStr((char*)src, strlen(src));
}

static int xlPathIsAbsolute(const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    if ( xrtPathIsAbs((str)path, 0) ) {
        return 1;
    }
#ifdef _WIN32
    return (path[0] == '\\' && path[1] == '\\') || (path[0] == '/' && path[1] == '/');
#else
    return 0;
#endif
}

static int xlCopyToBuffer(char* out, size_t outSize, const char* text)
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

static const char* xlBaseName(const char* path)
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

static int xlTextEqualsNoCase(const char* a, const char* b)
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

static void xlDirName(char* out, size_t outSize, const char* path)
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

static int xlJoinPathBuffer(char* out, size_t outSize, const char* dir, const char* name)
{
    const char* sep;
    size_t len;
    int written;

    if ( out == NULL || outSize == 0 || dir == NULL || name == NULL ) {
        return 0;
    }
    len = strlen(dir);
    sep = (len > 0 && (dir[len - 1u] == '\\' || dir[len - 1u] == '/')) ? "" : "\\";
    written = snprintf(out, outSize, "%s%s%s", dir, sep, name);
    return written > 0 && (size_t)written < outSize;
}

static int xlGetModuleDir(char* out, size_t outSize)
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

static int xlGetProjectRoot(char* out, size_t outSize)
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

static int xlTextLooksLikePath(const char* text)
{
    return text != NULL &&
           (strchr(text, '\\') != NULL || strchr(text, '/') != NULL ||
            strchr(text, ':') != NULL || strchr(text, '.') != NULL);
}

static char* xlPathSnapshot(const char* baseDir, const char* path)
{
    str joined;

    if ( path == NULL || path[0] == '\0' ) {
        return xlDupText("");
    }
    if ( xlPathIsAbsolute(path) || baseDir == NULL || baseDir[0] == '\0' ) {
        return xlDupText(path);
    }
    joined = xrtPathJoin(2, (str)baseDir, (str)path);
    return joined != NULL ? (char*)joined : NULL;
}

static char* xlToolchainPathSnapshot(const char* baseDir, const char* path)
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

static char* xlDefaultTempDirSnapshot(const char* baseDir)
{
    return xlPathSnapshot(baseDir, "build\\tmp");
}

static void xlSetError(XL_Context* ctx, const char* text)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->lastError);
    ctx->lastError = xlDupText(text != NULL && text[0] != '\0' ? text : "xlang api error");
}

static void xlSetError2(XL_Context* ctx, const char* prefix, const char* detail)
{
    char text[1024];

    snprintf(text, sizeof(text), "%s%s%s",
             prefix != NULL ? prefix : "xlang api error",
             detail != NULL && detail[0] != '\0' ? ": " : "",
             detail != NULL ? detail : "");
    xlSetError(ctx, text);
}

static char* xlGetCurrentDirText(XL_Context* ctx)
{
#ifdef _WIN32
    DWORD need;
    DWORD got;
    wchar_t* wide;
    char* text;

    need = GetCurrentDirectoryW(0, NULL);
    if ( need == 0 ) {
        xlSetError(ctx, "cannot query current directory");
        return NULL;
    }
    wide = (wchar_t*)xrtMalloc(((size_t)need + 1) * sizeof(wchar_t));
    if ( wide == NULL ) {
        xlSetError(ctx, "cannot allocate current directory buffer");
        return NULL;
    }
    got = GetCurrentDirectoryW(need + 1, wide);
    if ( got == 0 || got > need ) {
        xrtFree(wide);
        xlSetError(ctx, "cannot save current directory");
        return NULL;
    }
    text = (char*)xrtUTF16to8((u16str)wide, 0, NULL);
    xrtFree(wide);
    if ( text == NULL ) {
        xlSetError(ctx, "cannot convert current directory");
    }
    return text;
#else
    size_t size = 1024;
    char* buffer;

    while ( size <= 65536 ) {
        buffer = (char*)xrtMalloc(size);
        if ( buffer == NULL ) {
            xlSetError(ctx, "cannot allocate current directory buffer");
            return NULL;
        }
        if ( getcwd(buffer, size) != NULL ) {
            return buffer;
        }
        xrtFree(buffer);
        if ( errno != ERANGE ) {
            xlSetError(ctx, "cannot save current directory");
            return NULL;
        }
        size *= 2;
    }
    xlSetError(ctx, "current directory path is too long");
    return NULL;
#endif
}

static char* xlPathSnapshotFromCurrentDir(XL_Context* ctx, const char* path, const char* errorText)
{
    char* baseDir;
    char* copy;

    baseDir = xlGetCurrentDirText(ctx);
    if ( baseDir == NULL ) {
        return NULL;
    }
    copy = xlPathSnapshot(baseDir, path);
    xrtFree(baseDir);
    if ( copy == NULL ) {
        xlSetError(ctx, errorText != NULL ? errorText : "cannot snapshot path");
    }
    return copy;
}

static char* xlLinkValueSnapshotFromCurrentDir(XL_Context* ctx, const char* value, const char* errorText)
{
    char* copy;

    if ( !xlTextLooksLikePath(value) ) {
        copy = xlDupText(value);
        if ( copy == NULL ) {
            xlSetError(ctx, errorText != NULL ? errorText : "cannot copy link library");
        }
        return copy;
    }
    return xlPathSnapshotFromCurrentDir(ctx, value, errorText);
}

static void xlStringListFree(XL_StringList* list)
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

static int xlStringListAppend(XL_StringList* list, const char* text)
{
    char** newItems;
    char* copy;

    if ( list == NULL || text == NULL || text[0] == '\0' ) {
        return 0;
    }
    copy = xlDupText(text);
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

static int xlContextAddSymbol(XL_Context* ctx, const char* name, void* value)
{
    XL_CSymbol* newItems;
    char* nameCopy;
    size_t i;

    if ( ctx == NULL || name == NULL || name[0] == '\0' || value == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->symbolCount; ++i ) {
        if ( strcmp(ctx->symbols[i].name, name) == 0 ) {
            ctx->symbols[i].value = value;
            return 1;
        }
    }
    nameCopy = xlDupText(name);
    if ( nameCopy == NULL ) {
        return 0;
    }
    newItems = (XL_CSymbol*)xrtRealloc(ctx->symbols, (ctx->symbolCount + 1) * sizeof(XL_CSymbol));
    if ( newItems == NULL ) {
        xrtFree(nameCopy);
        return 0;
    }
    ctx->symbols = newItems;
    ctx->symbols[ctx->symbolCount].name = nameCopy;
    ctx->symbols[ctx->symbolCount].value = value;
    ctx->symbolCount += 1;
    return 1;
}

static void xlFreeSymbols(XL_Context* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->symbolCount; ++i ) {
        xrtFree(ctx->symbols[i].name);
    }
    xrtFree(ctx->symbols);
    ctx->symbols = NULL;
    ctx->symbolCount = 0;
}

static void xlCloseLoadedLibrary(XL_LoadedLibrary* item)
{
    if ( item == NULL ) {
        return;
    }
#ifdef _WIN32
    if ( item->module != NULL ) {
        FreeLibrary(item->module);
    }
#else
    if ( item->module != NULL ) {
        dlclose(item->module);
    }
#endif
    xrtFree(item->path);
    memset(item, 0, sizeof(*item));
}

static void xlLoadedLibrariesFree(XL_Context* ctx)
{
    size_t i;

    if ( ctx == NULL ) {
        return;
    }
    for ( i = 0; i < ctx->loadedLibraryCount; ++i ) {
        xlCloseLoadedLibrary(&ctx->loadedLibraries[i]);
    }
    xrtFree(ctx->loadedLibraries);
    ctx->loadedLibraries = NULL;
    ctx->loadedLibraryCount = 0;
}

static int xlContextAddLoadedLibrary(XL_Context* ctx, const char* path, XL_LoadedLibrary* loaded)
{
    XL_LoadedLibrary* newItems;
    XL_LoadedLibrary* item;
    char* pathCopy;

    if ( ctx == NULL || path == NULL || path[0] == '\0' || loaded == NULL || loaded->module == NULL ) {
        return 0;
    }
    pathCopy = xlDupText(path);
    if ( pathCopy == NULL ) {
        return 0;
    }
    newItems = (XL_LoadedLibrary*)xrtRealloc(ctx->loadedLibraries, (ctx->loadedLibraryCount + 1) * sizeof(XL_LoadedLibrary));
    if ( newItems == NULL ) {
        xrtFree(pathCopy);
        return 0;
    }
    ctx->loadedLibraries = newItems;
    item = &ctx->loadedLibraries[ctx->loadedLibraryCount];
    memset(item, 0, sizeof(*item));
    item->path = pathCopy;
    item->module = loaded->module;
    loaded->module = NULL;
    ctx->loadedLibraryCount += 1;
    return 1;
}

static int xlEnterWorkDir(XL_Context* ctx, XL_CwdScope* scope)
{
    char* workDir;

    if ( scope == NULL ) {
        return 0;
    }
    memset(scope, 0, sizeof(*scope));
    if ( ctx == NULL || ctx->workDir == NULL || ctx->workDir[0] == '\0' ) {
        return 1;
    }
    workDir = xlDupText(ctx->workDir);
    if ( workDir == NULL ) {
        xlSetError(ctx, "cannot allocate work dir snapshot");
        return 0;
    }
    /*
        进程 cwd 是全局状态。脚本执行期可以释放 API 锁以避免嵌套运行死锁，
        但只要进入了 workDir，就必须用独立 cwd 锁保护到恢复 cwd 为止。
        等待 cwd 锁时不能持有 API 锁，否则执行线程回收时会反向等待 API 锁。
    */
    xlApiUnlock();
    xlCwdLock();
    xlApiLock();
    scope->locked = 1;
#ifdef _WIN32
    {
        DWORD need;
        DWORD got;
        u16str workDirW;

        need = GetCurrentDirectoryW(0, NULL);
        if ( need == 0 ) {
            xlSetError(ctx, "cannot query current directory");
            xrtFree(workDir);
            xlCwdUnlock();
            scope->locked = 0;
            return 0;
        }
        scope->oldDir = (wchar_t*)xrtMalloc(((size_t)need + 1) * sizeof(wchar_t));
        if ( scope->oldDir == NULL ) {
            xlSetError(ctx, "cannot allocate current directory buffer");
            xrtFree(workDir);
            xlCwdUnlock();
            scope->locked = 0;
            return 0;
        }
        got = GetCurrentDirectoryW(need + 1, scope->oldDir);
        if ( got == 0 || got > need ) {
            xrtFree(scope->oldDir);
            scope->oldDir = NULL;
            xlSetError(ctx, "cannot save current directory");
            xrtFree(workDir);
            xlCwdUnlock();
            scope->locked = 0;
            return 0;
        }
        workDirW = xrtUTF8to16((u8str)workDir, 0, NULL);
        if ( workDirW == NULL || !SetCurrentDirectoryW((LPCWSTR)workDirW) ) {
            xrtFree(workDirW);
            xrtFree(scope->oldDir);
            scope->oldDir = NULL;
            xlSetError2(ctx, "cannot enter work dir", workDir);
            xrtFree(workDir);
            xlCwdUnlock();
            scope->locked = 0;
            return 0;
        }
        xrtFree(workDirW);
        xrtFree(workDir);
        scope->active = 1;
        return 1;
    }
#else
    {
        char oldBuf[1024];
        if ( getcwd(oldBuf, sizeof(oldBuf)) == NULL ) {
            xlSetError(ctx, "cannot save current directory");
            xrtFree(workDir);
            xlCwdUnlock();
            scope->locked = 0;
            return 0;
        }
        scope->oldDir = xlDupText(oldBuf);
        if ( scope->oldDir == NULL || chdir(workDir) != 0 ) {
            xrtFree(scope->oldDir);
            scope->oldDir = NULL;
            xlSetError2(ctx, "cannot enter work dir", workDir);
            xrtFree(workDir);
            xlCwdUnlock();
            scope->locked = 0;
            return 0;
        }
        xrtFree(workDir);
        scope->active = 1;
        return 1;
    }
#endif
}

static void xlLeaveWorkDir(XL_CwdScope* scope)
{
    if ( scope == NULL ) {
        return;
    }
    if ( scope->active ) {
#ifdef _WIN32
        if ( scope->oldDir != NULL ) {
            SetCurrentDirectoryW(scope->oldDir);
            xrtFree(scope->oldDir);
        }
#else
        if ( scope->oldDir != NULL ) {
            chdir(scope->oldDir);
            xrtFree(scope->oldDir);
        }
#endif
    }
    if ( scope->locked ) {
        xlCwdUnlock();
    }
    memset(scope, 0, sizeof(*scope));
}

static void xlTccError(void* opaque, const char* msg)
{
    XL_Context* ctx = (XL_Context*)opaque;

    if ( ctx != NULL && msg != NULL && msg[0] != '\0' ) {
        xlSetError2(ctx, "tcc", msg);
    }
}

#ifndef XL_STATIC_TCC
static void* xlLoadProc(XL_TccApi* api, const char* name)
{
#ifdef _WIN32
    return api != NULL && api->module != NULL ? (void*)GetProcAddress(api->module, name) : NULL;
#else
    return api != NULL && api->module != NULL ? dlsym(api->module, name) : NULL;
#endif
}
#endif

static void xlUnloadTccApi(XL_TccApi* api)
{
    if ( api == NULL ) {
        return;
    }
#ifndef XL_STATIC_TCC
#ifdef _WIN32
    if ( api->module != NULL ) {
        FreeLibrary(api->module);
    }
#else
    if ( api->module != NULL ) {
        dlclose(api->module);
    }
#endif
#endif
    xrtFree(api->path);
    memset(api, 0, sizeof(*api));
}

static const char* xlEnvPathOrNull(const char* name)
{
    const char* value;

    if ( name == NULL || name[0] == '\0' ) {
        return NULL;
    }
    value = getenv(name);
    return value != NULL && value[0] != '\0' ? value : NULL;
}

static const char* xlEnvPathOrDefault(const char* name, const char* fallback)
{
    const char* envPath = xlEnvPathOrNull(name);

    if ( envPath == NULL ) {
        return fallback;
    }
#ifdef XL_STATIC_TCC_VFS
    /* 静态内嵌 TCC 时，环境变量只覆盖真实存在的外部资源；否则回到 VFS 虚拟路径。 */
    return xrtFileExists((char*)envPath) ? envPath : fallback;
#else
    return envPath;
#endif
}

static const char* xlDefaultTccRootPath(void)
{
#ifdef XL_STATIC_TCC_VFS
    return XL6_TCC_ROOT_PATH;
#else
    return xlEnvPathOrDefault("XL6_TCC_ROOT", XL6_TCC_ROOT_PATH);
#endif
}

static const char* xlDefaultTccIncludePath(void)
{
#ifdef XL_STATIC_TCC_VFS
    return XL6_TCC_INCLUDE_PATH;
#else
    return xlEnvPathOrDefault("XL6_TCC_INCLUDE", XL6_TCC_INCLUDE_PATH);
#endif
}

static const char* xlDefaultTccWinIncludePath(void)
{
#ifdef XL_STATIC_TCC_VFS
    return XL6_TCC_WIN_INCLUDE_PATH;
#else
    return xlEnvPathOrDefault("XL6_TCC_WIN_INCLUDE", XL6_TCC_WIN_INCLUDE_PATH);
#endif
}

static const char* xlDefaultTccWinapiIncludePath(void)
{
#ifdef XL_STATIC_TCC_VFS
    return XL6_TCC_WINAPI_INCLUDE_PATH;
#else
    return xlEnvPathOrDefault("XL6_TCC_WINAPI_INCLUDE", XL6_TCC_WINAPI_INCLUDE_PATH);
#endif
}

static const char* xlDefaultTccLibPath(void)
{
#ifdef XL_STATIC_TCC_VFS
    return XL6_TCC_LIB_PATH;
#else
    return xlEnvPathOrDefault("XL6_TCC_LIB", XL6_TCC_LIB_PATH);
#endif
}

static const char* xlDefaultTccLibtcc1Path(void)
{
#ifdef XL_STATIC_TCC_VFS
    return XL6_TCC_LIBTCC1_PATH;
#else
    return xlEnvPathOrDefault("XL6_TCC_LIBTCC1", XL6_TCC_LIBTCC1_PATH);
#endif
}

static void xlAddDefaultTccRuntime(TCCState* s, XL_Context* ctx, int outputType)
{
    const char* path;

    if ( s == NULL ) {
        return;
    }

#ifdef XL_STATIC_TCC_VFS
    if ( outputType == TCC_OUTPUT_MEMORY ) {
        (void)xl__tcc.tcc_add_file(s, XL_TCC_VFS_LIBTCC1);
        return;
    }
#else
    (void)outputType;
#endif

    path = ctx != NULL && ctx->tccLibtcc1Path != NULL && ctx->tccLibtcc1Path[0] != '\0' ?
           ctx->tccLibtcc1Path :
           xlDefaultTccLibtcc1Path();
    if ( xrtFileExists((char*)path) ) {
        (void)xl__tcc.tcc_add_file(s, path);
        return;
    }
#ifdef XL_STATIC_TCC_VFS
    (void)xl__tcc.tcc_add_file(s, XL_TCC_VFS_LIBTCC1);
#endif
}

#ifdef XL_STATIC_TCC
static int xlLoadStaticTcc(XL_Context* ctx)
{
    XL_TccApi loaded;

    if ( xl__tcc.ready ) {
        return 1;
    }

    memset(&loaded, 0, sizeof(loaded));
    loaded.tcc_new = tcc_new;
    loaded.tcc_delete = tcc_delete;
    loaded.tcc_set_lib_path = tcc_set_lib_path;
    loaded.tcc_set_error_func = tcc_set_error_func;
    loaded.tcc_set_options = tcc_set_options;
    loaded.tcc_add_include_path = tcc_add_include_path;
    loaded.tcc_add_sysinclude_path = tcc_add_sysinclude_path;
    loaded.tcc_define_symbol = tcc_define_symbol;
    loaded.tcc_undefine_symbol = tcc_undefine_symbol;
    loaded.tcc_add_library_path = tcc_add_library_path;
    loaded.tcc_add_library = tcc_add_library;
    loaded.tcc_add_file = tcc_add_file;
    loaded.tcc_add_symbol = tcc_add_symbol;
    loaded.tcc_set_output_type = tcc_set_output_type;
    loaded.tcc_compile_string = tcc_compile_string;
    loaded.tcc_output_file = tcc_output_file;
    loaded.tcc_run = tcc_run;
    loaded.tcc_relocate = tcc_relocate;
    loaded.tcc_get_symbol = tcc_get_symbol;
    loaded.tcc_list_symbols = tcc_list_symbols;
    loaded.tcc_set_backtrace_func = tcc_set_backtrace_func;
    loaded.path = xlDupText("<static libtcc>");
    if ( loaded.path == NULL ) {
        xlSetError(ctx, "cannot remember static libtcc path");
        return 0;
    }
    loaded.ready = 1;
    xl__tcc = loaded;
    return 1;
}

static int xlEnsureTcc(XL_Context* ctx)
{
    return xlLoadStaticTcc(ctx);
}
#else
static int xlLoadTccFromPath(XL_Context* ctx, const char* path)
{
    XL_TccApi* api;
    XL_TccApi loaded;

    api = &xl__tcc;
    if ( api->ready ) {
        return 1;
    }
    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    memset(&loaded, 0, sizeof(loaded));
#ifdef _WIN32
    {
        u16str pathW = xrtUTF8to16((u8str)path, 0, NULL);
        if ( pathW != NULL ) {
            loaded.module = LoadLibraryW((LPCWSTR)pathW);
            xrtFree(pathW);
        }
    }
#else
    loaded.module = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
    if ( loaded.module == NULL ) {
        return 0;
    }

    loaded.tcc_new = (XL_TccNewProc)xlLoadProc(&loaded, "tcc_new");
    loaded.tcc_delete = (XL_TccDeleteProc)xlLoadProc(&loaded, "tcc_delete");
    loaded.tcc_set_lib_path = (XL_TccSetLibPathProc)xlLoadProc(&loaded, "tcc_set_lib_path");
    loaded.tcc_set_error_func = (XL_TccSetErrorFuncProc)xlLoadProc(&loaded, "tcc_set_error_func");
    loaded.tcc_set_options = (XL_TccSetOptionsProc)xlLoadProc(&loaded, "tcc_set_options");
    loaded.tcc_add_include_path = (XL_TccAddIncludePathProc)xlLoadProc(&loaded, "tcc_add_include_path");
    loaded.tcc_add_sysinclude_path = (XL_TccAddSysIncludePathProc)xlLoadProc(&loaded, "tcc_add_sysinclude_path");
    loaded.tcc_define_symbol = (XL_TccDefineSymbolProc)xlLoadProc(&loaded, "tcc_define_symbol");
    loaded.tcc_undefine_symbol = (XL_TccUndefineSymbolProc)xlLoadProc(&loaded, "tcc_undefine_symbol");
    loaded.tcc_add_library_path = (XL_TccAddLibraryPathProc)xlLoadProc(&loaded, "tcc_add_library_path");
    loaded.tcc_add_library = (XL_TccAddLibraryProc)xlLoadProc(&loaded, "tcc_add_library");
    loaded.tcc_add_file = (XL_TccAddFileProc)xlLoadProc(&loaded, "tcc_add_file");
    loaded.tcc_add_symbol = (XL_TccAddSymbolProc)xlLoadProc(&loaded, "tcc_add_symbol");
    loaded.tcc_set_output_type = (XL_TccSetOutputTypeProc)xlLoadProc(&loaded, "tcc_set_output_type");
    loaded.tcc_compile_string = (XL_TccCompileStringProc)xlLoadProc(&loaded, "tcc_compile_string");
    loaded.tcc_output_file = (XL_TccOutputFileProc)xlLoadProc(&loaded, "tcc_output_file");
    loaded.tcc_run = (XL_TccRunProc)xlLoadProc(&loaded, "tcc_run");
    loaded.tcc_relocate = (XL_TccRelocateProc)xlLoadProc(&loaded, "tcc_relocate");
    loaded.tcc_get_symbol = (XL_TccGetSymbolProc)xlLoadProc(&loaded, "tcc_get_symbol");
    loaded.tcc_list_symbols = (XL_TccListSymbolsProc)xlLoadProc(&loaded, "tcc_list_symbols");
    loaded.tcc_set_backtrace_func = (XL_TccSetBacktraceFuncProc)xlLoadProc(&loaded, "tcc_set_backtrace_func");

    if ( loaded.tcc_new == NULL || loaded.tcc_delete == NULL || loaded.tcc_set_lib_path == NULL ||
         loaded.tcc_set_error_func == NULL || loaded.tcc_add_include_path == NULL ||
         loaded.tcc_set_options == NULL || loaded.tcc_add_library_path == NULL || loaded.tcc_add_library == NULL ||
         loaded.tcc_add_file == NULL || loaded.tcc_add_symbol == NULL ||
         loaded.tcc_set_output_type == NULL || loaded.tcc_compile_string == NULL ||
         loaded.tcc_relocate == NULL || loaded.tcc_get_symbol == NULL ) {
        xlSetError2(ctx, "invalid libtcc dll", path);
        xlUnloadTccApi(&loaded);
        return 0;
    }
#if XL_ENABLE_EXE_COMPILER
    if ( loaded.tcc_output_file == NULL ) {
        xlSetError2(ctx, "invalid compiler-capable libtcc dll", path);
        xlUnloadTccApi(&loaded);
        return 0;
    }
#endif
    loaded.path = xlDupText(path);
    if ( loaded.path == NULL ) {
        xlSetError(ctx, "cannot remember libtcc path");
        xlUnloadTccApi(&loaded);
        return 0;
    }
    loaded.ready = 1;
    *api = loaded;
    return 1;
}

static int xlLoadTccFromBasePath(XL_Context* ctx, const char* path)
{
    char* snapshot;
    int ok;

    if ( path == NULL || path[0] == '\0' ) {
        return 0;
    }
    snapshot = xlPathSnapshot(ctx != NULL ? ctx->baseDir : NULL, path);
    if ( snapshot == NULL ) {
        xlSetError(ctx, "cannot snapshot libtcc path");
        return 0;
    }
    ok = xlLoadTccFromPath(ctx, snapshot);
    xrtFree(snapshot);
    return ok;
}

static int xlEnsureTcc(XL_Context* ctx)
{
    const char* path;
    const char* envPath;

    if ( xl__tcc.ready ) {
        return 1;
    }
    path = ctx != NULL && ctx->tccDllPath != NULL && ctx->tccDllPath[0] != '\0' ? ctx->tccDllPath : NULL;
    if ( xlLoadTccFromPath(ctx, path) ) {
        return 1;
    }
    envPath = xlEnvPathOrNull("XL6_TCC_DLL");
    if ( xlLoadTccFromBasePath(ctx, envPath) ) {
        return 1;
    }
    if ( xlLoadTccFromPath(ctx, "libtcc.dll") ) {
        return 1;
    }
    if ( xlLoadTccFromBasePath(ctx, "release\\libtcc.dll") ) {
        return 1;
    }
    if ( xlLoadTccFromBasePath(ctx, XL6_TCC_DLL_PATH) ) {
        return 1;
    }
    xlSetError(ctx, "cannot load libtcc.dll");
    return 0;
}
#endif

static int xlAddTccSymbol(TCCState* s, const char* name, const void* value)
{
#ifdef XL_STATIC_TCC
    return tcc_add_symbol(s, name, value) == 0;
#else
    return xl__tcc.tcc_add_symbol(s, name, value) == 0;
#endif
}

#define XL_ADD_XRT_SYMBOL(s, name) \
    do { \
        if ( !xlAddTccSymbol((s), #name, (const void*)(name)) ) { \
            return 0; \
        } \
    } while (0)

#define XL_ADD_TCC_API_SYMBOL(s, member, name) \
    do { \
        if ( xl__tcc.member != NULL && !xlAddTccSymbol((s), (name), (const void*)xl__tcc.member) ) { \
            return 0; \
        } \
    } while (0)

static int xlAddDefaultTccApiSymbols(TCCState* s)
{
    if ( s == NULL || !xl__tcc.ready ) {
        return 1;
    }

    XL_ADD_TCC_API_SYMBOL(s, tcc_new, "tcc_new");
    XL_ADD_TCC_API_SYMBOL(s, tcc_delete, "tcc_delete");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_lib_path, "tcc_set_lib_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_error_func, "tcc_set_error_func");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_options, "tcc_set_options");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_include_path, "tcc_add_include_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_sysinclude_path, "tcc_add_sysinclude_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_define_symbol, "tcc_define_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_undefine_symbol, "tcc_undefine_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_library_path, "tcc_add_library_path");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_library, "tcc_add_library");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_file, "tcc_add_file");
    XL_ADD_TCC_API_SYMBOL(s, tcc_add_symbol, "tcc_add_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_output_type, "tcc_set_output_type");
    XL_ADD_TCC_API_SYMBOL(s, tcc_compile_string, "tcc_compile_string");
    XL_ADD_TCC_API_SYMBOL(s, tcc_output_file, "tcc_output_file");
    XL_ADD_TCC_API_SYMBOL(s, tcc_run, "tcc_run");
    XL_ADD_TCC_API_SYMBOL(s, tcc_relocate, "tcc_relocate");
    XL_ADD_TCC_API_SYMBOL(s, tcc_get_symbol, "tcc_get_symbol");
    XL_ADD_TCC_API_SYMBOL(s, tcc_list_symbols, "tcc_list_symbols");
    XL_ADD_TCC_API_SYMBOL(s, tcc_set_backtrace_func, "tcc_set_backtrace_func");
    if ( !xlAddTccSymbol(s, "xlDefaultContextOptions", (const void*)xlDefaultContextOptions) ||
         !xlAddTccSymbol(s, "xlRuntimeInit", (const void*)xlRuntimeInit) ||
         !xlAddTccSymbol(s, "xlRuntimeUnit", (const void*)xlRuntimeUnit) ||
         !xlAddTccSymbol(s, "xlCreateContext", (const void*)xlCreateContext) ||
         !xlAddTccSymbol(s, "xlDestroyContext", (const void*)xlDestroyContext) ||
         !xlAddTccSymbol(s, "xlSetWorkDir", (const void*)xlSetWorkDir) ||
         !xlAddTccSymbol(s, "xlSetTempDir", (const void*)xlSetTempDir) ||
         !xlAddTccSymbol(s, "xlSetSaveTemps", (const void*)xlSetSaveTemps) ||
         !xlAddTccSymbol(s, "xlAddIncludePath", (const void*)xlAddIncludePath) ||
         !xlAddTccSymbol(s, "xlAddLibraryPath", (const void*)xlAddLibraryPath) ||
         !xlAddTccSymbol(s, "xlAddLinkLibrary", (const void*)xlAddLinkLibrary) ||
         !xlAddTccSymbol(s, "xlLoadDependencyLibrary", (const void*)xlLoadDependencyLibrary) ||
         !xlAddTccSymbol(s, "xlRegisterCFunction", (const void*)xlRegisterCFunction) ||
         !xlAddTccSymbol(s, "xlRunScriptString", (const void*)xlRunScriptString) ||
         !xlAddTccSymbol(s, "xlRunScriptFile", (const void*)xlRunScriptFile) ||
         !xlAddTccSymbol(s, "xlRunScriptStringArg", (const void*)xlRunScriptStringArg) ||
         !xlAddTccSymbol(s, "xlRunScriptFileArg", (const void*)xlRunScriptFileArg) ||
         !xlAddTccSymbol(s, "xlCompileScriptStringToModule", (const void*)xlCompileScriptStringToModule) ||
         !xlAddTccSymbol(s, "xlCompileScriptFileToModule", (const void*)xlCompileScriptFileToModule) ||
         !xlAddTccSymbol(s, "xlCompileCSourceStringToModule", (const void*)xlCompileCSourceStringToModule) ||
         !xlAddTccSymbol(s, "xlCompileCSourceFileToModule", (const void*)xlCompileCSourceFileToModule) ||
         !xlAddTccSymbol(s, "xlCompileCSourceFilesToModule", (const void*)xlCompileCSourceFilesToModule) ||
         !xlAddTccSymbol(s, "xlRunCSourceString", (const void*)xlRunCSourceString) ||
         !xlAddTccSymbol(s, "xlRunCSourceFile", (const void*)xlRunCSourceFile) ||
         !xlAddTccSymbol(s, "xlRunCSourceFiles", (const void*)xlRunCSourceFiles) ||
         !xlAddTccSymbol(s, "xlRunModuleMain", (const void*)xlRunModuleMain) ||
         !xlAddTccSymbol(s, "xlModuleGetSymbol", (const void*)xlModuleGetSymbol) ||
         !xlAddTccSymbol(s, "xlDestroyModule", (const void*)xlDestroyModule) ||
         !xlAddTccSymbol(s, "xlGetLastError", (const void*)xlGetLastError) ||
         !xlAddTccSymbol(s, "xlClearLastError", (const void*)xlClearLastError) ||
         !xlAddTccSymbol(s, "xlGetLastExitCode", (const void*)xlGetLastExitCode) ) {
        return 0;
    }
    if ( !xlAddTccSymbol(s, "xlTccRegisterXrtEnvironment", (const void*)xlTccRegisterXrtEnvironment) ) {
        return 0;
    }
#if XL_ENABLE_EXE_COMPILER
    if ( !xlAddTccSymbol(s, "xlCompileScriptStringToExe", (const void*)xlCompileScriptStringToExe) ||
         !xlAddTccSymbol(s, "xlCompileScriptFileToExe", (const void*)xlCompileScriptFileToExe) ||
         !xlAddTccSymbol(s, "xlCompileCSourceFileToExe", (const void*)xlCompileCSourceFileToExe) ||
         !xlAddTccSymbol(s, "xlCompileCSourceFilesToExe", (const void*)xlCompileCSourceFilesToExe) ||
         !xlAddTccSymbol(s, "xlCompileCSourceFileToSharedLibrary", (const void*)xlCompileCSourceFileToSharedLibrary) ||
         !xlAddTccSymbol(s, "xlCompileCSourceFilesToSharedLibrary", (const void*)xlCompileCSourceFilesToSharedLibrary) ||
         !xlAddTccSymbol(s, "xlcDefaultContextOptions", (const void*)xlcDefaultContextOptions) ||
         !xlAddTccSymbol(s, "xlcCreateContext", (const void*)xlcCreateContext) ||
         !xlAddTccSymbol(s, "xlcDestroyContext", (const void*)xlcDestroyContext) ||
         !xlAddTccSymbol(s, "xlcSetWorkDir", (const void*)xlcSetWorkDir) ||
         !xlAddTccSymbol(s, "xlcSetTempDir", (const void*)xlcSetTempDir) ||
         !xlAddTccSymbol(s, "xlcSetSaveTemps", (const void*)xlcSetSaveTemps) ||
         !xlAddTccSymbol(s, "xlcAddIncludePath", (const void*)xlcAddIncludePath) ||
         !xlAddTccSymbol(s, "xlcAddLibraryPath", (const void*)xlcAddLibraryPath) ||
         !xlAddTccSymbol(s, "xlcAddLinkLibrary", (const void*)xlcAddLinkLibrary) ||
         !xlAddTccSymbol(s, "xlcAddCSourceFile", (const void*)xlcAddCSourceFile) ||
         !xlAddTccSymbol(s, "xlcLoadDependencyLibrary", (const void*)xlcLoadDependencyLibrary) ||
         !xlAddTccSymbol(s, "xlcRegisterCFunction", (const void*)xlcRegisterCFunction) ||
         !xlAddTccSymbol(s, "xlcAddSourceFile", (const void*)xlcAddSourceFile) ||
         !xlAddTccSymbol(s, "xlcAddSourceString", (const void*)xlcAddSourceString) ||
         !xlAddTccSymbol(s, "xlcCompile", (const void*)xlcCompile) ||
         !xlAddTccSymbol(s, "xlcCheck", (const void*)xlcCheck) ||
         !xlAddTccSymbol(s, "xlcEmitTokens", (const void*)xlcEmitTokens) ||
         !xlAddTccSymbol(s, "xlcEmitAst", (const void*)xlcEmitAst) ||
         !xlAddTccSymbol(s, "xlcEmitIr", (const void*)xlcEmitIr) ||
         !xlAddTccSymbol(s, "xlcEmitC", (const void*)xlcEmitC) ||
         !xlAddTccSymbol(s, "xlcEmitDebugC", (const void*)xlcEmitDebugC) ||
         !xlAddTccSymbol(s, "xlcEmitHeader", (const void*)xlcEmitHeader) ||
         !xlAddTccSymbol(s, "xlcEmitAbi", (const void*)xlcEmitAbi) ||
         !xlAddTccSymbol(s, "xlcEmitExe", (const void*)xlcEmitExe) ||
         !xlAddTccSymbol(s, "xlcEmitDebugExe", (const void*)xlcEmitDebugExe) ||
         !xlAddTccSymbol(s, "xlcRuntimeContext", (const void*)xlcRuntimeContext) ||
         !xlAddTccSymbol(s, "xlcRelocateModule", (const void*)xlcRelocateModule) ||
         !xlAddTccSymbol(s, "xlcRun", (const void*)xlcRun) ||
         !xlAddTccSymbol(s, "xlcGetLastError", (const void*)xlcGetLastError) ||
         !xlAddTccSymbol(s, "xlcClearLastError", (const void*)xlcClearLastError) ||
         !xlAddTccSymbol(s, "xlcGetLastExitCode", (const void*)xlcGetLastExitCode) ||
         !xlAddTccSymbol(s, "xlcEmitSharedLibrary", (const void*)xlcEmitSharedLibrary) ) {
        return 0;
    }
#endif

#ifdef XL_STATIC_TCC_VFS
    if ( !xlAddTccSymbol(s, "tcc_vfs_open", (const void*)tcc_vfs_open) ||
         !xlAddTccSymbol(s, "tcc_vfs_close", (const void*)tcc_vfs_close) ||
         !xlAddTccSymbol(s, "tcc_vfs_read", (const void*)tcc_vfs_read) ||
         !xlAddTccSymbol(s, "tcc_vfs_lseek", (const void*)tcc_vfs_lseek) ||
         !xlAddTccSymbol(s, "tcc_vfs_fopen", (const void*)tcc_vfs_fopen) ||
         !xlAddTccSymbol(s, "tcc_vfs_fclose", (const void*)tcc_vfs_fclose) ||
         !xlAddTccSymbol(s, "tcc_vfs_mount_memory", (const void*)tcc_vfs_mount_memory) ||
         !xlAddTccSymbol(s, "tcc_vfs_clear_dynamic", (const void*)tcc_vfs_clear_dynamic) ) {
        return 0;
    }
#endif
    return 1;
}

static int xlAddDefaultRuntimeSymbols(TCCState* s)
{
    XL_ADD_XRT_SYMBOL(s, xrtInit);
    if ( !xlAddTccSymbol(s, "__mzerosf", &xl__mzerosf) ||
         !xlAddTccSymbol(s, "__mzerodf", &xl__mzerodf) ) {
        return 0;
    }
    XL_ADD_XRT_SYMBOL(s, xrtUnit);
    XL_ADD_XRT_SYMBOL(s, xrtMalloc);
    XL_ADD_XRT_SYMBOL(s, xrtCalloc);
    XL_ADD_XRT_SYMBOL(s, xrtRealloc);
    XL_ADD_XRT_SYMBOL(s, xrtFree);
    XL_ADD_XRT_SYMBOL(s, xrtTempMemory);
    XL_ADD_XRT_SYMBOL(s, xrtFreeTempMemory);
    XL_ADD_XRT_SYMBOL(s, xrtBufferCreate);
    XL_ADD_XRT_SYMBOL(s, xrtBufferFrom);
    XL_ADD_XRT_SYMBOL(s, xrtBufferFromHex);
    XL_ADD_XRT_SYMBOL(s, xrtBufferFromBase64);
    XL_ADD_XRT_SYMBOL(s, xrtBufferDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtBufferInit);
    XL_ADD_XRT_SYMBOL(s, xrtBufferUnit);
    XL_ADD_XRT_SYMBOL(s, xrtBufferMalloc);
    XL_ADD_XRT_SYMBOL(s, xrtBufferInsert);
    XL_ADD_XRT_SYMBOL(s, xrtBufferAppend);
    XL_ADD_XRT_SYMBOL(s, xrtCopyStr);
    XL_ADD_XRT_SYMBOL(s, xrtStrConcat);
    XL_ADD_XRT_SYMBOL(s, xrtStrJoin);
    XL_ADD_XRT_SYMBOL(s, xrtFormat);
    XL_ADD_XRT_SYMBOL(s, xrtHash32);
    XL_ADD_XRT_SYMBOL(s, xrtHash32_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtHash64);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Micro);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Micro_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Nano);
    XL_ADD_XRT_SYMBOL(s, xrtHash64_Nano_WithSeed);
    XL_ADD_XRT_SYMBOL(s, xrtStrComp);
    XL_ADD_XRT_SYMBOL(s, xrtStrByteLen);
    XL_ADD_XRT_SYMBOL(s, xrtUTF8to16);
    XL_ADD_XRT_SYMBOL(s, xrtUTF16to8);
    XL_ADD_XRT_SYMBOL(s, xrtStrUtf8CharSize);
    XL_ADD_XRT_SYMBOL(s, xrtStrCharLen);
    XL_ADD_XRT_SYMBOL(s, xrtStrCharToBytePos);
    XL_ADD_XRT_SYMBOL(s, xrtFindStr);
    XL_ADD_XRT_SYMBOL(s, xrtStrIndexOf);
    XL_ADD_XRT_SYMBOL(s, xrtStrLastIndexOf);
    XL_ADD_XRT_SYMBOL(s, xrtStrCount);
    XL_ADD_XRT_SYMBOL(s, xrtInStr);
    XL_ADD_XRT_SYMBOL(s, xrtCheckStr);
    XL_ADD_XRT_SYMBOL(s, xrtStrLike);
    XL_ADD_XRT_SYMBOL(s, xrtStrApprox);
    XL_ADD_XRT_SYMBOL(s, xrtStrSim);
    XL_ADD_XRT_SYMBOL(s, xrtIsUTF8);
    XL_ADD_XRT_SYMBOL(s, xrtConvCharset);
    XL_ADD_XRT_SYMBOL(s, xrtDetectCharset);
    XL_ADD_XRT_SYMBOL(s, xrtGetCharSize);
    XL_ADD_XRT_SYMBOL(s, xrtIntFormat);
    XL_ADD_XRT_SYMBOL(s, xrtUIntFormat);
    XL_ADD_XRT_SYMBOL(s, xrtNumFormat);
    XL_ADD_XRT_SYMBOL(s, xrtLCase);
    XL_ADD_XRT_SYMBOL(s, xrtUCase);
    XL_ADD_XRT_SYMBOL(s, xrtTrim);
    XL_ADD_XRT_SYMBOL(s, xrtLTrim);
    XL_ADD_XRT_SYMBOL(s, xrtRTrim);
    XL_ADD_XRT_SYMBOL(s, xrtFilterStr);
    XL_ADD_XRT_SYMBOL(s, xrtReplace);
    XL_ADD_XRT_SYMBOL(s, xrtStrSlice);
    XL_ADD_XRT_SYMBOL(s, xrtStrLeft);
    XL_ADD_XRT_SYMBOL(s, xrtStrRight);
    XL_ADD_XRT_SYMBOL(s, xrtStrMid);
    XL_ADD_XRT_SYMBOL(s, xrtStrCharAt);
    XL_ADD_XRT_SYMBOL(s, xrtStrByteAt);
    XL_ADD_XRT_SYMBOL(s, xrtStrRepeat);
    XL_ADD_XRT_SYMBOL(s, xrtStrIsEmpty);
    XL_ADD_XRT_SYMBOL(s, xrtStrIsBlank);
    XL_ADD_XRT_SYMBOL(s, xrtStrRemove);
    XL_ADD_XRT_SYMBOL(s, xrtStrRemoveAt);
    XL_ADD_XRT_SYMBOL(s, xrtStrInsert);
    XL_ADD_XRT_SYMBOL(s, xrtStrPadLeft);
    XL_ADD_XRT_SYMBOL(s, xrtStrPadRight);
    XL_ADD_XRT_SYMBOL(s, xrtStrPadCenter);
    XL_ADD_XRT_SYMBOL(s, xrtStrReverse);
    XL_ADD_XRT_SYMBOL(s, xrtStrSplitLines);
    XL_ADD_XRT_SYMBOL(s, xrtSplit);
    XL_ADD_XRT_SYMBOL(s, xrtIntApprox);
    XL_ADD_XRT_SYMBOL(s, xrtNumApprox);
    XL_ADD_XRT_SYMBOL(s, xrtMathMin);
    XL_ADD_XRT_SYMBOL(s, xrtMathMax);
    XL_ADD_XRT_SYMBOL(s, xrtMathClamp);
    XL_ADD_XRT_SYMBOL(s, xrtMathSign);
    XL_ADD_XRT_SYMBOL(s, xrtMathTrunc);
    XL_ADD_XRT_SYMBOL(s, xrtMathFract);
    XL_ADD_XRT_SYMBOL(s, xrtMathMod);
    XL_ADD_XRT_SYMBOL(s, xrtMathRad);
    XL_ADD_XRT_SYMBOL(s, xrtMathDeg);
    XL_ADD_XRT_SYMBOL(s, xrtMathIsNaN);
    XL_ADD_XRT_SYMBOL(s, xrtMathIsInf);
    XL_ADD_XRT_SYMBOL(s, xrtMathIsFinite);
    XL_ADD_XRT_SYMBOL(s, xrtMathLog2);
    XL_ADD_XRT_SYMBOL(s, xrtMathExp2);
    XL_ADD_XRT_SYMBOL(s, xrtMathLog1p);
    XL_ADD_XRT_SYMBOL(s, xrtMathExpm1);
    XL_ADD_XRT_SYMBOL(s, xrtMathCbrt);
    XL_ADD_XRT_SYMBOL(s, xrtMathHypot);
    XL_ADD_XRT_SYMBOL(s, xrtTimeApprox);
    XL_ADD_XRT_SYMBOL(s, xrtDateAdd);
    XL_ADD_XRT_SYMBOL(s, xrtDateDiff);
    XL_ADD_XRT_SYMBOL(s, xrtIsLeapYear);
    XL_ADD_XRT_SYMBOL(s, xrtDaysInMonth);
    XL_ADD_XRT_SYMBOL(s, xrtDaysInYear);
    XL_ADD_XRT_SYMBOL(s, xrtTimeSerial);
    XL_ADD_XRT_SYMBOL(s, xrtDateSerial);
    XL_ADD_XRT_SYMBOL(s, xrtDateTimeSerial);
    XL_ADD_XRT_SYMBOL(s, xrtStrToTime);
    XL_ADD_XRT_SYMBOL(s, xrtTryStrToTime);
    XL_ADD_XRT_SYMBOL(s, xrtTimeFormat);
    XL_ADD_XRT_SYMBOL(s, xrtTimeParse);
    XL_ADD_XRT_SYMBOL(s, xrtTryTimeParse);
    XL_ADD_XRT_SYMBOL(s, xrtRelativeTime);
    XL_ADD_XRT_SYMBOL(s, xrtNow);
    XL_ADD_XRT_SYMBOL(s, xrtNowUTC);
    XL_ADD_XRT_SYMBOL(s, xrtDate);
    XL_ADD_XRT_SYMBOL(s, xrtTime);
    XL_ADD_XRT_SYMBOL(s, xrtTimer);
    XL_ADD_XRT_SYMBOL(s, xrtSleep);
    XL_ADD_XRT_SYMBOL(s, xrtTimezoneOffset);
    XL_ADD_XRT_SYMBOL(s, xrtYear);
    XL_ADD_XRT_SYMBOL(s, xrtMonth);
    XL_ADD_XRT_SYMBOL(s, xrtDay);
    XL_ADD_XRT_SYMBOL(s, xrtHour);
    XL_ADD_XRT_SYMBOL(s, xrtMinute);
    XL_ADD_XRT_SYMBOL(s, xrtSecond);
    XL_ADD_XRT_SYMBOL(s, xrtWeekday);
    XL_ADD_XRT_SYMBOL(s, xrtDayOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtQuarter);
    XL_ADD_XRT_SYMBOL(s, xrtDatePart);
    XL_ADD_XRT_SYMBOL(s, xrtTimePart);
    XL_ADD_XRT_SYMBOL(s, xrtIsSameDay);
    XL_ADD_XRT_SYMBOL(s, xrtIsSameMonth);
    XL_ADD_XRT_SYMBOL(s, xrtIsSameYear);
    XL_ADD_XRT_SYMBOL(s, xrtTimeInRange);
    XL_ADD_XRT_SYMBOL(s, xrtTimeRangeOverlap);
    XL_ADD_XRT_SYMBOL(s, xrtToUnixTime);
    XL_ADD_XRT_SYMBOL(s, xrtFromUnixTime);
    XL_ADD_XRT_SYMBOL(s, xrtUTCToLocal);
    XL_ADD_XRT_SYMBOL(s, xrtLocalToUTC);
    XL_ADD_XRT_SYMBOL(s, xrtFirstDayOfMonth);
    XL_ADD_XRT_SYMBOL(s, xrtLastDayOfMonth);
    XL_ADD_XRT_SYMBOL(s, xrtFirstDayOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtLastDayOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtFirstDayOfWeek);
    XL_ADD_XRT_SYMBOL(s, xrtLastDayOfWeek);
    XL_ADD_XRT_SYMBOL(s, xrtWeekOfYear);
    XL_ADD_XRT_SYMBOL(s, xrtWeekOfMonth);
    XL_ADD_XRT_SYMBOL(s, xrtStrToI64);
    XL_ADD_XRT_SYMBOL(s, xrtStrToNum);
    XL_ADD_XRT_SYMBOL(s, xrtI64ToStr);
    XL_ADD_XRT_SYMBOL(s, xrtNumToStr);
    XL_ADD_XRT_SYMBOL(s, xrtTimeToStr);
    XL_ADD_XRT_SYMBOL(s, xrtRand32);
    XL_ADD_XRT_SYMBOL(s, xrtRand64);
    XL_ADD_XRT_SYMBOL(s, xrtRandRange);
    XL_ADD_XRT_SYMBOL(s, xrtRandSeedThread);
    XL_ADD_XRT_SYMBOL(s, xrtRandCreate);
    XL_ADD_XRT_SYMBOL(s, xrtRandDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRandSeedObj);
    XL_ADD_XRT_SYMBOL(s, xrtRand32Obj);
    XL_ADD_XRT_SYMBOL(s, xrtRand64Obj);
    XL_ADD_XRT_SYMBOL(s, xrtRandRangeObj);
    XL_ADD_XRT_SYMBOL(s, xrtRandStr);
    XL_ADD_XRT_SYMBOL(s, xrtRandStrObj);
    XL_ADD_XRT_SYMBOL(s, xlogCreate);
    XL_ADD_XRT_SYMBOL(s, xlogDestroy);
    XL_ADD_XRT_SYMBOL(s, xlogDefault);
    XL_ADD_XRT_SYMBOL(s, xlogSetLevel);
    XL_ADD_XRT_SYMBOL(s, xlogGetLevel);
    XL_ADD_XRT_SYMBOL(s, xlogAddConsole);
    XL_ADD_XRT_SYMBOL(s, xlogAddFile);
    XL_ADD_XRT_SYMBOL(s, xlogAddRollingFile);
    XL_ADD_XRT_SYMBOL(s, xlogAppenderSetFormat);
    XL_ADD_XRT_SYMBOL(s, xlogLevelName);
    XL_ADD_XRT_SYMBOL(s, xlogWrite);
    XL_ADD_XRT_SYMBOL(s, xlogFlush);
    XL_ADD_XRT_SYMBOL(s, xrtPathExists);
    XL_ADD_XRT_SYMBOL(s, xrtPathIsAbs);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetNameExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetName);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathGetDir);
    XL_ADD_XRT_SYMBOL(s, xrtPathJoin);
    XL_ADD_XRT_SYMBOL(s, xrtPathRandom);
    XL_ADD_XRT_SYMBOL(s, xrtPathSep);
    XL_ADD_XRT_SYMBOL(s, xrtPathNormalize);
    XL_ADD_XRT_SYMBOL(s, xrtPathAbs);
    XL_ADD_XRT_SYMBOL(s, xrtPathRel);
    XL_ADD_XRT_SYMBOL(s, xrtPathCwd);
    XL_ADD_XRT_SYMBOL(s, xrtPathSetCwd);
    XL_ADD_XRT_SYMBOL(s, xrtPathHome);
    XL_ADD_XRT_SYMBOL(s, xrtPathTemp);
    XL_ADD_XRT_SYMBOL(s, xrtPathAppDir);
    XL_ADD_XRT_SYMBOL(s, xrtPathWithExt);
    XL_ADD_XRT_SYMBOL(s, xrtPathWithName);
    XL_ADD_XRT_SYMBOL(s, xrtFileExists);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetSize);
    XL_ADD_XRT_SYMBOL(s, xrtFileSetSize);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAttr);
    XL_ADD_XRT_SYMBOL(s, xrtFileSetAttr);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAccessTime);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetChangeTime);
    XL_ADD_XRT_SYMBOL(s, xrtFileReadAll);
    XL_ADD_XRT_SYMBOL(s, xrtFileWriteAll);
    XL_ADD_XRT_SYMBOL(s, xrtFileAppend);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAll);
    XL_ADD_XRT_SYMBOL(s, xrtFilePutAll);
    XL_ADD_XRT_SYMBOL(s, xrtOpen);
    XL_ADD_XRT_SYMBOL(s, xrtClose);
    XL_ADD_XRT_SYMBOL(s, xrtSeek);
    XL_ADD_XRT_SYMBOL(s, xrtTell);
    XL_ADD_XRT_SYMBOL(s, xrtGetEOF);
    XL_ADD_XRT_SYMBOL(s, xrtEOF);
    XL_ADD_XRT_SYMBOL(s, xrtSetEOF);
    XL_ADD_XRT_SYMBOL(s, xrtRead);
    XL_ADD_XRT_SYMBOL(s, xrtGet);
    XL_ADD_XRT_SYMBOL(s, xrtWrite);
    XL_ADD_XRT_SYMBOL(s, xrtFileDelete);
    XL_ADD_XRT_SYMBOL(s, xrtFileCopy);
    XL_ADD_XRT_SYMBOL(s, xrtFileMove);
    XL_ADD_XRT_SYMBOL(s, xrtFileRename);
    XL_ADD_XRT_SYMBOL(s, xrtFileReplace);
    XL_ADD_XRT_SYMBOL(s, xrtFileTouch);
    XL_ADD_XRT_SYMBOL(s, xrtFileWriteAllAtomic);
    XL_ADD_XRT_SYMBOL(s, xrtFileAppendAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileReadAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileWriteAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileGetAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFilePutAllAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileCopyAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileMoveAsync);
    XL_ADD_XRT_SYMBOL(s, xrtFileDeleteAsync);
    XL_ADD_XRT_SYMBOL(s, xrtAsyncFileBufDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtAsyncFileIoDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtDirExists);
    XL_ADD_XRT_SYMBOL(s, xrtDirCreate);
    XL_ADD_XRT_SYMBOL(s, xrtDirCreateAll);
    XL_ADD_XRT_SYMBOL(s, xrtDirScanEx);
    XL_ADD_XRT_SYMBOL(s, xrtDirCopy);
    XL_ADD_XRT_SYMBOL(s, xrtDirMove);
    XL_ADD_XRT_SYMBOL(s, xrtDirDelete);
    XL_ADD_XRT_SYMBOL(s, xrtDirIsEmpty);
    XL_ADD_XRT_SYMBOL(s, xrtDirCount);
    XL_ADD_XRT_SYMBOL(s, xrtDirSize);
    XL_ADD_XRT_SYMBOL(s, xrtDirIsRoot);
    XL_ADD_XRT_SYMBOL(s, xrtDirClean);
    XL_ADD_XRT_SYMBOL(s, xrtDirEnsureEmpty);
    XL_ADD_XRT_SYMBOL(s, xrtParseJSON);
    XL_ADD_XRT_SYMBOL(s, xrtJsonValid);
    XL_ADD_XRT_SYMBOL(s, xrtParseJSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyJSON);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyJSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtParseXSON);
    XL_ADD_XRT_SYMBOL(s, xrtXsonValid);
    XL_ADD_XRT_SYMBOL(s, xrtParseXSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyXSON);
    XL_ADD_XRT_SYMBOL(s, xrtStringifyXSON_File);
    XL_ADD_XRT_SYMBOL(s, xrtHexEncode);
    XL_ADD_XRT_SYMBOL(s, xrtHexDecode);
    XL_ADD_XRT_SYMBOL(s, xrtBase64Encode);
    XL_ADD_XRT_SYMBOL(s, xrtBase64Decode);
    XL_ADD_XRT_SYMBOL(s, xrtHexEncodeBytes);
    XL_ADD_XRT_SYMBOL(s, xrtBase64EncodeBytes);
    XL_ADD_XRT_SYMBOL(s, xrtPercentEncode);
    XL_ADD_XRT_SYMBOL(s, xrtPercentDecode);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncode);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlDecode);
    XL_ADD_XRT_SYMBOL(s, xrtSHA1);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256Init);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256Update);
    XL_ADD_XRT_SYMBOL(s, xrtSHA256Final);
    XL_ADD_XRT_SYMBOL(s, xrtSHA384);
    XL_ADD_XRT_SYMBOL(s, xrtSHA384Init);
    XL_ADD_XRT_SYMBOL(s, xrtSHA384Final);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512Init);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512Update);
    XL_ADD_XRT_SYMBOL(s, xrtSHA512Final);
    XL_ADD_XRT_SYMBOL(s, xrtHMAC_SHA256);
    XL_ADD_XRT_SYMBOL(s, xrtHMAC_SHA384);
    XL_ADD_XRT_SYMBOL(s, xrtHMAC_SHA512);
    XL_ADD_XRT_SYMBOL(s, xrtChaCha20);
    XL_ADD_XRT_SYMBOL(s, xrtChaCha20Poly1305Encrypt);
    XL_ADD_XRT_SYMBOL(s, xrtChaCha20Poly1305Decrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES128GCMEncrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES128GCMDecrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES256GCMEncrypt);
    XL_ADD_XRT_SYMBOL(s, xrtAES256GCMDecrypt);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExtract);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExpand);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExtract_SHA384);
    XL_ADD_XRT_SYMBOL(s, xrtHKDFExpand_SHA384);
    XL_ADD_XRT_SYMBOL(s, xrtX25519Keypair);
    XL_ADD_XRT_SYMBOL(s, xrtX25519SharedSecret);
    XL_ADD_XRT_SYMBOL(s, xrtX448Keypair);
    XL_ADD_XRT_SYMBOL(s, xrtX448SharedSecret);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519Keypair);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519PublicKey);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519Sign);
    XL_ADD_XRT_SYMBOL(s, xrtEd25519Verify);
    XL_ADD_XRT_SYMBOL(s, xrtRSAPSSVerify);
    XL_ADD_XRT_SYMBOL(s, xrtRSAPKCS1Verify);
    XL_ADD_XRT_SYMBOL(s, xrtMakeXIDS);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalIP);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalMAC);
    XL_ADD_XRT_SYMBOL(s, xrtGetLocalName);
    XL_ADD_XRT_SYMBOL(s, xrtUrlParseView);
    XL_ADD_XRT_SYMBOL(s, xrtUrlParseViewN);
    XL_ADD_XRT_SYMBOL(s, xrtUrlResolveTo);
    XL_ADD_XRT_SYMBOL(s, xrtUrlNormalizePathTo);
    XL_ADD_XRT_SYMBOL(s, xrtUrlViewCopyTargetTo);
    XL_ADD_XRT_SYMBOL(s, xrtPercentEncodeTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedDecodeTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedAppendFieldTo);
    XL_ADD_XRT_SYMBOL(s, xrtQueryValidate);
    XL_ADD_XRT_SYMBOL(s, xrtQueryAppendPairTo);
    XL_ADD_XRT_SYMBOL(s, xrtFormUrlEncodedValidateN);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCreate);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCreateEx);
    XL_ADD_XRT_SYMBOL(s, xrtRegexDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexIsMatch);
    XL_ADD_XRT_SYMBOL(s, xrtRegexFullMatch);
    XL_ADD_XRT_SYMBOL(s, xrtRegexFind);
    XL_ADD_XRT_SYMBOL(s, xrtRegexFindMatchAt);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchOk);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchByteStart);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchByteEnd);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchStart);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchEnd);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchText);
    XL_ADD_XRT_SYMBOL(s, xrtRegexMatchTextCopy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCaptureCount);
    XL_ADD_XRT_SYMBOL(s, xrtRegexCaptureName);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetCreateEx);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetIsMatchAt);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetFirstAt);
    XL_ADD_XRT_SYMBOL(s, xrtRegexSetMatchesAt);
    XL_ADD_XRT_SYMBOL(s, xrtProcessConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtProcessSpawn);
    XL_ADD_XRT_SYMBOL(s, xrtProcessDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtProcessIsRunning);
    XL_ADD_XRT_SYMBOL(s, xrtProcessExitCode);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWrite);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWriteText);
    XL_ADD_XRT_SYMBOL(s, xrtProcessCloseStdin);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWait);
    XL_ADD_XRT_SYMBOL(s, xrtProcessWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtProcessTerminate);
    XL_ADD_XRT_SYMBOL(s, xrtProcessKill);
    XL_ADD_XRT_SYMBOL(s, xrtProcessKillTree);
    XL_ADD_XRT_SYMBOL(s, xrtProcessGetStdout);
    XL_ADD_XRT_SYMBOL(s, xrtProcessGetStderr);
    XL_ADD_XRT_SYMBOL(s, xrtExecCapture);
    XL_ADD_XRT_SYMBOL(s, xrtProcessRun);
    XL_ADD_XRT_SYMBOL(s, xrtProcessResultUnit);
    XL_ADD_XRT_SYMBOL(s, xrtProcessResultCopy);
    XL_ADD_XRT_SYMBOL(s, xrtProcessResultSuccess);
    XL_ADD_XRT_SYMBOL(s, xrtProcessPipelineRun);
    XL_ADD_XRT_SYMBOL(s, xrtProcessPipelineResultUnit);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterBegin);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterEnd);
    XL_ADD_XRT_SYMBOL(s, xrtAVLTB_IterNext);
    XL_ADD_XRT_SYMBOL(s, xrtListWalk);
    XL_ADD_XRT_SYMBOL(s, xrtDictWalk);
    XL_ADD_XRT_SYMBOL(s, xrtCallableCreate);
    XL_ADD_XRT_SYMBOL(s, xrtCallableAddRef);
    XL_ADD_XRT_SYMBOL(s, xrtCallableUnref);
    XL_ADD_XRT_SYMBOL(s, xrtCallableSig);
    XL_ADD_XRT_SYMBOL(s, xrtCallableInvoke);
    XL_ADD_XRT_SYMBOL(s, xrtCallResultInit);
    XL_ADD_XRT_SYMBOL(s, xrtCallResultSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtCallResultUnit);
    XL_ADD_XRT_SYMBOL(s, xrtFuncSigTypeId);
    XL_ADD_XRT_SYMBOL(s, xrtTypeNull);
    XL_ADD_XRT_SYMBOL(s, xrtTypeBool);
    XL_ADD_XRT_SYMBOL(s, xrtTypeInt);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFloat);
    XL_ADD_XRT_SYMBOL(s, xrtTypeString);
    XL_ADD_XRT_SYMBOL(s, xrtTypeTime);
    XL_ADD_XRT_SYMBOL(s, xrtTypePoint);
    XL_ADD_XRT_SYMBOL(s, xrtTypeType);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFunction);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFuture);
    XL_ADD_XRT_SYMBOL(s, xrtTypeArray);
    XL_ADD_XRT_SYMBOL(s, xrtTypeList);
    XL_ADD_XRT_SYMBOL(s, xrtTypeSet);
    XL_ADD_XRT_SYMBOL(s, xrtTypeDict);
    XL_ADD_XRT_SYMBOL(s, xrtTypeRecord);
    XL_ADD_XRT_SYMBOL(s, xrtTypeHandle);
    XL_ADD_XRT_SYMBOL(s, xrtTypeSame);
    XL_ADD_XRT_SYMBOL(s, xrtTypeName);
    XL_ADD_XRT_SYMBOL(s, xrtTypeGenericArgCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypeGenericArg);
    XL_ADD_XRT_SYMBOL(s, xrtTypeMethods);
    XL_ADD_XRT_SYMBOL(s, xrtTypeFindMethod);
    XL_ADD_XRT_SYMBOL(s, xrtTypeToStringValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypeBoxValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypeUnboxValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypeDropValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayAppend);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayAppendValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArraySet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArraySetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayInsertValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayTakeValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayPopValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayReverse);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayIndexOfValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayContainsValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayExtend);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayConcat);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedArrayItemType);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListSet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListTakeValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListContainsValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListMerge);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListClone);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedListItemType);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetAdd);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetAddValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetExists);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetExistsValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetRemoveValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetMerge);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetClone);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetUnion);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetIntersection);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetDifference);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetSymmetricDifference);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetIsSubset);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetIsSuperset);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedSetItemType);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictCreate);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictInit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictUnit);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictCount);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictSet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictGet);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictSetValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictItemAt);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictExists);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictRemove);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictTakeValue);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictClear);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictMerge);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictClone);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictEquals);
    XL_ADD_XRT_SYMBOL(s, xrtTypedDictItemType);
    XL_ADD_XRT_SYMBOL(s, xTaskRunThread);
    XL_ADD_XRT_SYMBOL(s, xFutureCreate);
    XL_ADD_XRT_SYMBOL(s, xFutureAddRef);
    XL_ADD_XRT_SYMBOL(s, xFutureWait);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitCo);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitValue);
    XL_ADD_XRT_SYMBOL(s, xFutureWaitCoValue);
    XL_ADD_XRT_SYMBOL(s, xFutureState);
    XL_ADD_XRT_SYMBOL(s, xFutureStatus);
    XL_ADD_XRT_SYMBOL(s, xFutureValue);
    XL_ADD_XRT_SYMBOL(s, xFutureValueIsOwned);
    XL_ADD_XRT_SYMBOL(s, xFutureGetResult);
    XL_ADD_XRT_SYMBOL(s, xFutureSetDebugName);
    XL_ADD_XRT_SYMBOL(s, xFutureError);
    XL_ADD_XRT_SYMBOL(s, xFutureRelease);
    XL_ADD_XRT_SYMBOL(s, xFutureRequestCancel);
    XL_ADD_XRT_SYMBOL(s, xFutureForwardCancelTo);
    XL_ADD_XRT_SYMBOL(s, xFutureWhenAny);
    XL_ADD_XRT_SYMBOL(s, xFutureWhenAll);
    XL_ADD_XRT_SYMBOL(s, xFutureRace);
    XL_ADD_XRT_SYMBOL(s, xFutureGetGroupSourceIndex);
    XL_ADD_XRT_SYMBOL(s, xFutureGetAllValueCount);
    XL_ADD_XRT_SYMBOL(s, xFutureGetAllValueItem);
    XL_ADD_XRT_SYMBOL(s, xPromiseCreate);
    XL_ADD_XRT_SYMBOL(s, xPromiseDestroy);
    XL_ADD_XRT_SYMBOL(s, xPromiseGetFuture);
    XL_ADD_XRT_SYMBOL(s, xPromisePeekFuture);
    XL_ADD_XRT_SYMBOL(s, xPromiseResolve);
    XL_ADD_XRT_SYMBOL(s, xPromiseResolveOwned);
    XL_ADD_XRT_SYMBOL(s, xPromiseReject);
    XL_ADD_XRT_SYMBOL(s, xPromiseCancel);
    XL_ADD_XRT_SYMBOL(s, xPromiseClose);
    XL_ADD_XRT_SYMBOL(s, xrtChannelCreate);
    XL_ADD_XRT_SYMBOL(s, xrtChannelDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtChannelSend);
    XL_ADD_XRT_SYMBOL(s, xrtChannelRecv);
    XL_ADD_XRT_SYMBOL(s, xrtChannelTryRecv);
    XL_ADD_XRT_SYMBOL(s, xrtChannelClose);
    XL_ADD_XRT_SYMBOL(s, xrtChannelCount);
    XL_ADD_XRT_SYMBOL(s, xrtChannelIsClosed);
    XL_ADD_XRT_SYMBOL(s, xrtMutexCreate);
    XL_ADD_XRT_SYMBOL(s, xrtMutexDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtMutexLock);
    XL_ADD_XRT_SYMBOL(s, xrtMutexTryLock);
    XL_ADD_XRT_SYMBOL(s, xrtMutexUnlock);
    XL_ADD_XRT_SYMBOL(s, xrtSemCreate);
    XL_ADD_XRT_SYMBOL(s, xrtSemDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtSemWait);
    XL_ADD_XRT_SYMBOL(s, xrtSemTryWait);
    XL_ADD_XRT_SYMBOL(s, xrtSemWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtSemPost);
    XL_ADD_XRT_SYMBOL(s, xrtSemPostMultiple);
    XL_ADD_XRT_SYMBOL(s, xrtCondCreate);
    XL_ADD_XRT_SYMBOL(s, xrtCondDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtCondWait);
    XL_ADD_XRT_SYMBOL(s, xrtCondWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtCondSignal);
    XL_ADD_XRT_SYMBOL(s, xrtCondBroadcast);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockCreate);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockReadLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockTryReadLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockReadUnlock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockWriteLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockTryWriteLock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockWriteUnlock);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockDowngrade);
    XL_ADD_XRT_SYMBOL(s, xrtRWLockUpgrade);
    XL_ADD_XRT_SYMBOL(s, xrtThreadCreate);
    XL_ADD_XRT_SYMBOL(s, xrtThreadDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtThreadWait);
    XL_ADD_XRT_SYMBOL(s, xrtThreadWaitTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtThreadStop);
    XL_ADD_XRT_SYMBOL(s, xrtThreadGetState);
    XL_ADD_XRT_SYMBOL(s, xrtThreadGetExitCode);
    XL_ADD_XRT_SYMBOL(s, xrtThreadGetCurrentId);
    XL_ADD_XRT_SYMBOL(s, xrtCoCreate);
    XL_ADD_XRT_SYMBOL(s, xrtCoDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtCoResume);
    XL_ADD_XRT_SYMBOL(s, xrtCoYield);
    XL_ADD_XRT_SYMBOL(s, xrtCoGetState);
    XL_ADD_XRT_SYMBOL(s, xrtClearError);
    XL_ADD_XRT_SYMBOL(s, xrtGetError);
    XL_ADD_XRT_SYMBOL(s, xrtRandomBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineCreate);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineStart);
    XL_ADD_XRT_SYMBOL(s, xrtNetEngineStop);
    XL_ADD_XRT_SYMBOL(s, xrtNetSyncShutdownHiddenEngine);
    XL_ADD_XRT_SYMBOL(s, xrtNetSyncGetHiddenEngine);
    XL_ADD_XRT_SYMBOL(s, xrtHttpCloseIdleConnections);
    XL_ADD_XRT_SYMBOL(s, xrtNetAddrParse);
    XL_ADD_XRT_SYMBOL(s, xrtNetConnectConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamCreate);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamConnect);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamClose);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSend);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamPendingSend);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamWaitTimeoutEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetChainBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetChainPeek);
    XL_ADD_XRT_SYMBOL(s, xrtNetChainConsume);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpConnect);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpConnectEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpConnectAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListen);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListenEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListenerDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpAccept);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpAcceptAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpListenerPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpStreamDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetTcpStreamClose);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamAvailable);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRead);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamReadBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamReadText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRecvBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRecvText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRecvTextAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSendBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSendText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamSendTextAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamDrain);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamDrainAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamLocalAddrText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRemoteAddrText);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamLocalPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetStreamRemotePort);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpBind);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpBindEx);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpLocalPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpLocalAddrText);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpSendBytes);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpSendText);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpSendTextAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpRecv);
    XL_ADD_XRT_SYMBOL(s, xrtNetUdpRecvAsync);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketBytesCopy);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketText);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketFromText);
    XL_ADD_XRT_SYMBOL(s, xrtNetDgramPacketFromPort);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionCreate);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionFeedCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionPendingRecv);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionPendingCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionPeekCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionConsumeCipher);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionDriveHandshake);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionIsReady);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionReadPlain);
    XL_ADD_XRT_SYMBOL(s, xrtNetTlsSessionWritePlain);
    XL_ADD_XRT_SYMBOL(s, xrtHttpHeaderSplitLine);
    XL_ADD_XRT_SYMBOL(s, xrtHttpHeaderBuildLineTo);
    XL_ADD_XRT_SYMBOL(s, xrtHttpMediaTypeParse);
    XL_ADD_XRT_SYMBOL(s, xrtHttpMediaTypeFindParam);
    XL_ADD_XRT_SYMBOL(s, xrtHttpContentDispositionParse);
    XL_ADD_XRT_SYMBOL(s, xrtHttpContentDispositionDecodeFileNameTo);
    XL_ADD_XRT_SYMBOL(s, xrtMultipartNextN);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestInit);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestUnit);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestCreate);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetMethod);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetURL);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetBodyCopy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetIdleTimeout);
    XL_ADD_XRT_SYMBOL(s, xrtHttpRequestSetVerifyPeer);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseStatusCode);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseReason);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseContentLength);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseBody);
    XL_ADD_XRT_SYMBOL(s, xrtHttpResponseBodyTextCopy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpExecuteSync);
    XL_ADD_XRT_SYMBOL(s, xrtHttpExecuteSyncDefault);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdCreate);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdBoundPort);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdStart);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdStop);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdDrain);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetStatus);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseAddHeader);
    XL_ADD_XRT_SYMBOL(s, xrtHttpdResponseSetBodyCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtWebStaticConfigInit);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppCreate);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRetain);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRelease);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtWebMethodMask);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRouteAsyncEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStreamRouteAsync);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppGet);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppPost);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppAny);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStatic);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStaticDefault);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppStaticEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebAppError);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerCreate);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerCreateHostEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerCreateHostTlsEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerDestroy);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStart);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStartOk);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStop);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerDrain);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerPort);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRequestCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerConnectionCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerSetApp);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerReloadApp);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRemoveHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerTlsHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRemoveTlsHost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerError);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRouteEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRoute);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRouteAsyncEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStreamRouteAsync);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerGet);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerPost);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerAny);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStatic);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStaticDefault);
    XL_ADD_XRT_SYMBOL(s, xrtWebServerStaticEx);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMethod);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMethodCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMethodId);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestTarget);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestTargetCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestPath);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestPathCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQuery);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryHasValueAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeader);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderNameAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderValueAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestHeaderValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestBodyTextCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestContentLength);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestContentType);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestIsFormUrlEncoded);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormPairAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormHasValueAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestFormValueCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestIsMultipartForm);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartPartAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartIsFileAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartFileNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartContentTypeCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartBodyViewAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartBodySizeAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartBodyTextCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestMultipartValueCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestLocalAddrCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestRemoteAddrCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestTlsSNICopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestLocalPort);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestRemotePort);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParam);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamNameAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamValueViewAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestParamValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryValue);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestQueryValueCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookie);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieCopy);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieCount);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookiePairAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieNameCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebRequestCookieValueCopyAt);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseStatus);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseSetHeader);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseAddHeader);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseCookie);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseDeleteCookie);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseJsonText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseReserveBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseAppendBody);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseAppendText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseStart);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseSend);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseWriteText);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseEnd);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseFile);
    XL_ADD_XRT_SYMBOL(s, xrtWebResponseRedirect);
    XL_ADD_XRT_SYMBOL(s, xrtWebMimeByPath);
    XL_ADD_XRT_SYMBOL(s, xteParse);
    XL_ADD_XRT_SYMBOL(s, xteDestroyTemplate);
    XL_ADD_XRT_SYMBOL(s, xteMake);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetNodeCount);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetExprCount);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetArgCount);
    XL_ADD_XRT_SYMBOL(s, xteTemplateGetStringPoolSize);
    XL_ADD_XRT_SYMBOL(s, xvoCreateNull);
    XL_ADD_XRT_SYMBOL(s, xvoCreateBool);
    XL_ADD_XRT_SYMBOL(s, xvoCreateInt);
    XL_ADD_XRT_SYMBOL(s, xvoCreateFloat);
    XL_ADD_XRT_SYMBOL(s, xvoCreateText);
    XL_ADD_XRT_SYMBOL(s, xvoCreateTime);
    XL_ADD_XRT_SYMBOL(s, xvoCreatePoint);
    XL_ADD_XRT_SYMBOL(s, xvoCreateArray);
    XL_ADD_XRT_SYMBOL(s, xvoCreateList);
    XL_ADD_XRT_SYMBOL(s, xvoCreateSet);
    XL_ADD_XRT_SYMBOL(s, xvoCreateTable);
    XL_ADD_XRT_SYMBOL(s, xvoCreateColl);
    XL_ADD_XRT_SYMBOL(s, xvoCreateCallable);
    XL_ADD_XRT_SYMBOL(s, xvoCreateRecord);
    XL_ADD_XRT_SYMBOL(s, xvoCreateHandle);
    XL_ADD_XRT_SYMBOL(s, xvoCreateArrayArgs);
    XL_ADD_XRT_SYMBOL(s, xvoCreateListArgs);
    XL_ADD_XRT_SYMBOL(s, xvoCreateSetArgs);
    XL_ADD_XRT_SYMBOL(s, xvoCreateTableArgs);
    XL_ADD_XRT_SYMBOL(s, xvoAddRef);
    XL_ADD_XRT_SYMBOL(s, xvoUnref);
    XL_ADD_XRT_SYMBOL(s, xvoCopy);
    XL_ADD_XRT_SYMBOL(s, xvoDeepCopy);
    XL_ADD_XRT_SYMBOL(s, xvoType);
    XL_ADD_XRT_SYMBOL(s, xvoTypeDesc);
    XL_ADD_XRT_SYMBOL(s, xvoIsNull);
    XL_ADD_XRT_SYMBOL(s, xvoIsArray);
    XL_ADD_XRT_SYMBOL(s, xvoGetBool);
    XL_ADD_XRT_SYMBOL(s, xvoGetInt);
    XL_ADD_XRT_SYMBOL(s, xvoGetFloat);
    XL_ADD_XRT_SYMBOL(s, xvoGetText);
    XL_ADD_XRT_SYMBOL(s, xvoGetTime);
    XL_ADD_XRT_SYMBOL(s, xvoGetPoint);
    XL_ADD_XRT_SYMBOL(s, xvoGetCallable);
    XL_ADD_XRT_SYMBOL(s, xvoGetColl);
    XL_ADD_XRT_SYMBOL(s, xvoGetList);
    XL_ADD_XRT_SYMBOL(s, xvoGetTable);
    XL_ADD_XRT_SYMBOL(s, xvoGetRecordData);
    XL_ADD_XRT_SYMBOL(s, xvoGetHandleData);
    XL_ADD_XRT_SYMBOL(s, xvoToString);
    XL_ADD_XRT_SYMBOL(s, xvoInvoke);
    XL_ADD_XRT_SYMBOL(s, xvoCallableSig);
    XL_ADD_XRT_SYMBOL(s, xvoArrayAppendValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayInsertValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayRemove);
    XL_ADD_XRT_SYMBOL(s, xvoArrayTakeValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayPopValue);
    XL_ADD_XRT_SYMBOL(s, xvoArrayClear);
    XL_ADD_XRT_SYMBOL(s, xvoArrayAlloc);
    XL_ADD_XRT_SYMBOL(s, xvoArraySwap);
    XL_ADD_XRT_SYMBOL(s, xvoArrayItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoArrayGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoArraySetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoListGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoListRemove);
    XL_ADD_XRT_SYMBOL(s, xvoListTakeValue);
    XL_ADD_XRT_SYMBOL(s, xvoListClear);
    XL_ADD_XRT_SYMBOL(s, xvoSetItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoSetGetValueAt);
    XL_ADD_XRT_SYMBOL(s, xvoSetAddValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetExistsValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetRemoveValue);
    XL_ADD_XRT_SYMBOL(s, xvoSetClear);
    XL_ADD_XRT_SYMBOL(s, xvoTableItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoTableExists);
    XL_ADD_XRT_SYMBOL(s, xvoTableGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableMerge);
    XL_ADD_XRT_SYMBOL(s, xvoTableRemove);
    XL_ADD_XRT_SYMBOL(s, xvoTableTakeValue);
    XL_ADD_XRT_SYMBOL(s, xvoTableClear);
    XL_ADD_XRT_SYMBOL(s, xvoTableWalk);
    XL_ADD_XRT_SYMBOL(s, xvoCollItemCount);
    XL_ADD_XRT_SYMBOL(s, xvoCollExists);
    XL_ADD_XRT_SYMBOL(s, xvoCollSetValue);
    XL_ADD_XRT_SYMBOL(s, xvoBasicCompare);
    XL_ADD_XRT_SYMBOL(s, xvoBasicEqual);
    XL_ADD_XRT_SYMBOL(s, xvoValueTruthy);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetI64);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetKey);
    XL_ADD_XRT_SYMBOL(s, xvoIndexGetValue);
    XL_ADD_XRT_SYMBOL(s, xvoValueContains);
    XL_ADD_XRT_SYMBOL(s, xvoPrint);
    XL_ADD_XRT_SYMBOL(s, xvoSetTypeDesc);
    if ( !xlAddTccSymbol(s, "xCore", (const void*)&xCore) ) {
        return 0;
    }
    return xlAddDefaultTccApiSymbols(s);
}

static int xlAddContextSymbols(XL_Context* ctx, TCCState* s)
{
    size_t i;

    if ( !xlAddDefaultRuntimeSymbols(s) ) {
        xlSetError(ctx, "cannot add XRT symbols to TCC state");
        return 0;
    }
    for ( i = 0; ctx != NULL && i < ctx->symbolCount; ++i ) {
        if ( !xlAddTccSymbol(s, ctx->symbols[i].name, ctx->symbols[i].value) ) {
            xlSetError2(ctx, "cannot add C function symbol", ctx->symbols[i].name);
            return 0;
        }
    }
    return 1;
}

static void xlAddDefaultProjectToolchainPaths(TCCState* s)
{
    char root[512];
    char path[1024];

    if ( s == NULL || !xlGetProjectRoot(root, sizeof(root)) ) {
        return;
    }
    (void)xl__tcc.tcc_add_include_path(s, root);
    if ( xlJoinPathBuffer(path, sizeof(path), root, "src") ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
    if ( xlJoinPathBuffer(path, sizeof(path), root, "lib\\xrt") ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
    if ( xlJoinPathBuffer(path, sizeof(path), root, "release\\tcc\\include_win") ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
    if ( xlJoinPathBuffer(path, sizeof(path), root, XL6_TCC_INCLUDE_PATH) ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
    if ( xlJoinPathBuffer(path, sizeof(path), root, XL6_TCC_WIN_INCLUDE_PATH) ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
    if ( xlJoinPathBuffer(path, sizeof(path), root, XL6_TCC_WINAPI_INCLUDE_PATH) ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
    if ( xlJoinPathBuffer(path, sizeof(path), root, XL6_TCC_LIB_PATH) ) {
        (void)xl__tcc.tcc_add_library_path(s, path);
    }
}

static void xlAddDefaultTccLinkLibraries(TCCState* s);

static int xlConfigureTccStateEnvironment(XL_Context* ctx, TCCState* s, int outputType)
{
    size_t i;
    const char* tccLibPath;
    const char* tccLibSearchPath;

    if ( s == NULL ) {
        xlSetError(ctx, "TCC state is empty");
        return 0;
    }

    tccLibPath = ctx != NULL && ctx->tccLibPath != NULL && ctx->tccLibPath[0] != '\0' ?
                 ctx->tccLibPath : xlDefaultTccRootPath();
    tccLibSearchPath = ctx != NULL && ctx->tccLibSearchPath != NULL ? ctx->tccLibSearchPath : xlDefaultTccLibPath();
#ifdef XL_STATIC_TCC_VFS
    if ( outputType == TCC_OUTPUT_MEMORY ) {
        tccLibPath = XL6_TCC_ROOT_PATH;
        tccLibSearchPath = XL6_TCC_LIB_PATH;
    }
#endif
    xl__tcc.tcc_set_lib_path(s, tccLibPath);
    (void)xl__tcc.tcc_set_options(s, "-std=c11 -D_CRT_SECURE_NO_WARNINGS");
#if !XL_ENABLE_EXE_COMPILER
    (void)xl__tcc.tcc_set_options(s, "-DXL_RUNTIME_ONLY_DLL");
#endif
    (void)xl__tcc.tcc_add_include_path(s, ctx != NULL && ctx->baseDir != NULL ? ctx->baseDir : ".");
    xlAddDefaultProjectToolchainPaths(s);
    (void)xl__tcc.tcc_add_include_path(s, "src");
    (void)xl__tcc.tcc_add_include_path(s, "lib\\xrt");
    (void)xl__tcc.tcc_add_include_path(s, ctx != NULL && ctx->tccIncludePath != NULL ? ctx->tccIncludePath : xlDefaultTccIncludePath());
    (void)xl__tcc.tcc_add_include_path(s, ctx != NULL && ctx->tccWinIncludePath != NULL ? ctx->tccWinIncludePath : xlDefaultTccWinIncludePath());
    (void)xl__tcc.tcc_add_include_path(s, ctx != NULL && ctx->tccWinapiIncludePath != NULL ? ctx->tccWinapiIncludePath : xlDefaultTccWinapiIncludePath());
    (void)xl__tcc.tcc_add_library_path(s, tccLibSearchPath);
    xlAddDefaultTccRuntime(s, ctx, outputType);
    xlAddDefaultTccLinkLibraries(s);
    for ( i = 0; ctx != NULL && i < ctx->includePaths.count; ++i ) {
        if ( xl__tcc.tcc_add_include_path(s, ctx->includePaths.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add include path", ctx->includePaths.items[i]);
            return 0;
        }
    }
    for ( i = 0; ctx != NULL && i < ctx->libraryPaths.count; ++i ) {
        if ( xl__tcc.tcc_add_library_path(s, ctx->libraryPaths.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add library path", ctx->libraryPaths.items[i]);
            return 0;
        }
    }
    return 1;
}

static TCCState* xlCreateTccState(XL_Context* ctx, int outputType)
{
    TCCState* s;

    if ( !xlEnsureTcc(ctx) ) {
        return NULL;
    }
    s = xl__tcc.tcc_new();
    if ( s == NULL ) {
        xlSetError(ctx, "cannot create TCC state");
        return NULL;
    }
    xl__tcc.tcc_set_error_func(s, ctx, xlTccError);
    if ( xl__tcc.tcc_set_output_type(s, outputType) != 0 ) {
        xlSetError(ctx, "cannot set TCC output type");
        xl__tcc.tcc_delete(s);
        return NULL;
    }
    if ( !xlConfigureTccStateEnvironment(ctx, s, outputType) ) {
        xl__tcc.tcc_delete(s);
        return NULL;
    }
    if ( outputType == TCC_OUTPUT_MEMORY && !xlAddContextSymbols(ctx, s) ) {
        xl__tcc.tcc_delete(s);
        return NULL;
    }
    return s;
}

static void xlSetCompileDiagError(XL_Context* ctx, const XL6_CompileUnit* unit)
{
    char text[1200];
    const char* path;
    const char* phase;

    if ( unit != NULL && unit->parse.diags.count > 0 ) {
        const XL6_Diag* diag = &unit->parse.diags.items[0];
        path = unit->path != NULL ? unit->path : "<memory>";
        phase = unit->phase != NULL ? unit->phase : "compile";
        snprintf(text,
                 sizeof(text),
                 "%s:%d:%d: [%s] %s",
                 path,
                 diag->line,
                 diag->column,
                 phase,
                 diag->message != NULL ? diag->message : "compile error");
        xlSetError(ctx, text);
    } else {
        xlSetError(ctx, "xlang compile failed");
    }
}

static int xlCgenToMemoryEx(XL_Context* ctx, XL6_CompileUnit* unit, char** outText, const XL6_CgenOptions* options)
{
    XL6_Output out;
    int ok;

    if ( outText == NULL ) {
        return 0;
    }
    *outText = NULL;
    if ( !xl6OutputOpenMemory(&out) ) {
        xlSetError(ctx, "cannot create cgen memory output");
        return 0;
    }
    ok = xl6CgenEmitCEx(&out, unit->parse.root, &unit->parse.diags, options);
    if ( ok && !xl6OutputHasFailed(&out) ) {
        *outText = xl6OutputTakeMemoryText(&out, NULL);
    }
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot generate C code", xl6OutputError(&out));
        ok = 0;
    } else if ( !ok ) {
        xlSetCompileDiagError(ctx, unit);
    } else if ( *outText == NULL ) {
        xlSetError(ctx, "cannot copy generated C code");
        ok = 0;
    }
    xl6OutputClose(&out);
    return ok && *outText != NULL;
}

static int xlCgenToMemory(XL_Context* ctx, XL6_CompileUnit* unit, char** outText)
{
    return xlCgenToMemoryEx(ctx, unit, outText, NULL);
}

static void xlTccDynamicVfsClear(void)
{
#ifdef XL_STATIC_TCC_VFS
    tcc_vfs_clear_dynamic();
#endif
}

static int xlTccCompileSourceText(XL_Context* ctx, TCCState* s, const char* virtualPath, const char* source, const char* fallbackError)
{
    if ( s == NULL || source == NULL ) {
        xlSetError(ctx, fallbackError != NULL ? fallbackError : "TCC source is empty");
        return 0;
    }
#ifdef XL_STATIC_TCC_VFS
    if ( virtualPath == NULL || virtualPath[0] == '\0' ) {
        xlSetError(ctx, "dynamic TCC VFS path is empty");
        return 0;
    }
    if ( !tcc_vfs_mount_memory(virtualPath, source, strlen(source)) ) {
        xlSetError2(ctx, "cannot mount dynamic TCC VFS source", virtualPath);
        return 0;
    }
    if ( xl__tcc.tcc_add_file(s, virtualPath) != 0 ) {
        if ( ctx != NULL && ctx->lastError == NULL ) {
            xlSetError(ctx, fallbackError != NULL ? fallbackError : "TCC compile failed");
        }
        return 0;
    }
    return 1;
#else
    if ( xl__tcc.tcc_compile_string(s, source) != 0 ) {
        if ( ctx != NULL && ctx->lastError == NULL ) {
            xlSetError(ctx, fallbackError != NULL ? fallbackError : "TCC compile failed");
        }
        return 0;
    }
    return 1;
#endif
}

static int xlLoadUnitFromSource(XL_Context* ctx, XL6_CompileUnit* unit, const char* source, const char* virtualPath)
{
    xl6CompileUnitInit(unit);
    if ( source == NULL ) {
        xlSetError(ctx, "script source is empty");
        return 0;
    }
    if ( !xl6CompileUnitLoadSource(unit, source, virtualPath, 1) ) {
        xlSetCompileDiagError(ctx, unit);
        return 0;
    }
    return 1;
}

static int xlLoadUnitFromFile(XL_Context* ctx, XL6_CompileUnit* unit, const char* path)
{
    xl6CompileUnitInit(unit);
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "script file path is empty");
        return 0;
    }
    if ( !xl6CompileUnitLoadFile(unit, path, 1) ) {
        xlSetCompileDiagError(ctx, unit);
        return 0;
    }
    return 1;
}

static int xlAddDependenciesToTcc(XL_Context* ctx, TCCState* s, const XL6_CompileUnit* unit)
{
    size_t i;

    for ( i = 0; ctx != NULL && i < ctx->linkLibraries.count; ++i ) {
        const char* value = ctx->linkLibraries.items[i];
        if ( xlTextLooksLikePath(value) ) {
            if ( xl__tcc.tcc_add_file(s, value) != 0 ) {
                xlSetError2(ctx, "cannot add link file", value);
                return 0;
            }
        } else if ( xl__tcc.tcc_add_library(s, value) != 0 ) {
            xlSetError2(ctx, "cannot add link library", value);
            return 0;
        }
    }
    for ( i = 0; ctx != NULL && i < ctx->loadedLibraryCount; ++i ) {
        if ( xl__tcc.tcc_add_file(s, ctx->loadedLibraries[i].path) != 0 ) {
            xlSetError2(ctx, "cannot add dependency library", ctx->loadedLibraries[i].path);
            return 0;
        }
    }
    for ( i = 0; unit != NULL && i < unit->importDeps.links.count; ++i ) {
        const char* value = unit->importDeps.links.items[i];
        if ( xlTextLooksLikePath(value) ) {
            if ( xl__tcc.tcc_add_file(s, value) != 0 ) {
                xlSetError2(ctx, "cannot add import link file", value);
                return 0;
            }
        } else if ( xl__tcc.tcc_add_library(s, value) != 0 ) {
            xlSetError2(ctx, "cannot add import link library", value);
            return 0;
        }
    }
    for ( i = 0; unit != NULL && i < unit->importDeps.loadlibs.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, unit->importDeps.loadlibs.items[i]) != 0 ) {
            xlSetError2(ctx, "cannot add import loadlib", unit->importDeps.loadlibs.items[i]);
            return 0;
        }
    }
    return 1;
}

#if XL_ENABLE_EXE_COMPILER
static int xlEmitUnitHeaderToFile(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputPath, const char* guardName);

static char* xlJoinTempFilePath(XL_Context* ctx, const char* fileName)
{
    str path;

    if ( ctx == NULL || ctx->tempDir == NULL || ctx->tempDir[0] == '\0' ) {
        xlSetError(ctx, "compiler temp dir is empty");
        return NULL;
    }
    path = xrtPathJoin(2, (str)ctx->tempDir, (str)fileName);
    if ( path == NULL ) {
        xlSetError2(ctx, "cannot create temp path", fileName);
    }
    return (char*)path;
}

static int xlNormalizeCIncludePath(XL_Context* ctx, char* out, size_t outSize, const char* path)
{
    size_t i;

    if ( out == NULL || outSize == 0 || path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "C source file path is empty");
        return 0;
    }
    for ( i = 0; path[i] != '\0'; ++i ) {
        if ( i + 1 >= outSize ) {
            xlSetError2(ctx, "C source include path is too long", path);
            return 0;
        }
        if ( path[i] == '"' || path[i] == '\n' || path[i] == '\r' ) {
            xlSetError2(ctx, "C source include path contains invalid character", path);
            return 0;
        }
        out[i] = path[i] == '\\' ? '/' : path[i];
    }
    out[i] = '\0';
    return 1;
}

static int xlWriteTempTextFile(XL_Context* ctx, const char* path, const char* text)
{
    size_t size;
    int written;

    if ( path == NULL || path[0] == '\0' || text == NULL ) {
        xlSetError(ctx, "temp file write input is empty");
        return 0;
    }
    size = strlen(text);
    written = xrtFileWriteAll((str)path, (str)text, size, XRT_CP_BINARY);
    if ( written < 0 || (size_t)written != size ) {
        xlSetError2(ctx, "cannot write temp file", path);
        return 0;
    }
    return 1;
}

static int xlSaveTempTextByName(XL_Context* ctx, const char* fileName, const char* text)
{
    char* path;
    int ok;

    if ( ctx == NULL || !ctx->saveTemps ) {
        return 1;
    }
    if ( ctx->tempDir == NULL || ctx->tempDir[0] == '\0' ) {
        xlSetError(ctx, "compiler temp dir is empty");
        return 0;
    }
    if ( !xrtDirExists((str)ctx->tempDir) && !xrtDirCreateAll((str)ctx->tempDir) ) {
        xlSetError2(ctx, "cannot create compiler temp dir", ctx->tempDir);
        return 0;
    }
    path = xlJoinTempFilePath(ctx, fileName);
    if ( path == NULL ) {
        return 0;
    }
    ok = xlWriteTempTextFile(ctx, path, text);
    xrtFree(path);
    return ok;
}

static int xlEmitUnitHeaderToMemory(XL_Context* ctx, XL6_CompileUnit* unit, const char* guardName, char** outText)
{
    XL6_Output out;
    int ok;

    if ( outText == NULL ) {
        return 0;
    }
    *outText = NULL;
    if ( unit == NULL || unit->parse.root == NULL ) {
        xlSetError(ctx, "compile unit is empty");
        return 0;
    }
    if ( !xl6OutputOpenMemory(&out) ) {
        xlSetError(ctx, "cannot create header memory output");
        return 0;
    }
    ok = xl6CgenEmitHeader(&out,
                           unit->parse.root,
                           guardName != NULL && guardName[0] != '\0' ? guardName : "XL6_GENERATED_HEADER_H",
                           &unit->parse.diags);
    if ( ok && !xl6OutputHasFailed(&out) ) {
        *outText = xl6OutputTakeMemoryText(&out, NULL);
    }
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot generate header output", xl6OutputError(&out));
        ok = 0;
    } else if ( !ok ) {
        xlSetCompileDiagError(ctx, unit);
    } else if ( *outText == NULL ) {
        xlSetError(ctx, "cannot copy generated header output");
        ok = 0;
    }
    xl6OutputClose(&out);
    return ok && *outText != NULL;
}

static int xlMountDynamicTccText(XL_Context* ctx, const char* virtualPath, const char* text)
{
#ifdef XL_STATIC_TCC_VFS
    if ( virtualPath == NULL || virtualPath[0] == '\0' || text == NULL ) {
        xlSetError(ctx, "dynamic TCC VFS input is empty");
        return 0;
    }
    if ( !tcc_vfs_mount_memory(virtualPath, text, strlen(text)) ) {
        xlSetError2(ctx, "cannot mount dynamic TCC VFS source", virtualPath);
        return 0;
    }
    return 1;
#else
    (void)virtualPath;
    (void)text;
    xlSetError(ctx, "dynamic TCC VFS is not available");
    return 0;
#endif
}

static int xlOutputNormalizedCIncludePath(XL_Context* ctx, XL6_Output* out, const char* path)
{
    size_t i;

    if ( out == NULL || path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "C source file path is empty");
        return 0;
    }
    for ( i = 0; path[i] != '\0'; ++i ) {
        if ( path[i] == '"' || path[i] == '\n' || path[i] == '\r' ) {
            xlSetError2(ctx, "C source include path contains invalid character", path);
            return 0;
        }
        if ( xl6OutputPutc(path[i] == '\\' ? '/' : path[i], out) == EOF ) {
            xlSetError(ctx, "cannot write C source include path");
            return 0;
        }
    }
    return 1;
}

static int xlBuildCSourceWrapperText(XL_Context* ctx, const char* sourcePath, char** outText)
{
    XL6_Output out;
    int ok;

    if ( outText == NULL ) {
        return 0;
    }
    *outText = NULL;
    if ( !xl6OutputOpenMemory(&out) ) {
        xlSetError(ctx, "cannot create C source wrapper output");
        return 0;
    }
    ok = xl6OutputPuts("#include \"lib/xrt/xrt.h\"\n", &out) != EOF &&
         xl6OutputPuts("#include \"module.h\"\n", &out) != EOF &&
         xl6OutputPuts("#include \"", &out) != EOF &&
         xlOutputNormalizedCIncludePath(ctx, &out, sourcePath) &&
         xl6OutputPuts("\"\n", &out) != EOF;
    if ( ok && !xl6OutputHasFailed(&out) ) {
        *outText = xl6OutputTakeMemoryText(&out, NULL);
    }
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot generate C source wrapper", xl6OutputError(&out));
        ok = 0;
    } else if ( ok && *outText == NULL ) {
        xlSetError(ctx, "cannot copy C source wrapper");
        ok = 0;
    }
    xl6OutputClose(&out);
    return ok && *outText != NULL;
}

static int xlCompileAdditionalCSourceFilesVfs(XL_Context* ctx, TCCState* s, XL6_CompileUnit* unit, const XL_StringList* cSourceFiles)
{
    char* headerText;
    char* wrapperText;
    char wrapperName[64];
    char virtualPath[96];
    size_t i;

    if ( cSourceFiles == NULL || cSourceFiles->count == 0 ) {
        return 1;
    }
    if ( ctx == NULL || s == NULL ) {
        xlSetError(ctx, "compiler context is empty");
        return 0;
    }
    if ( cSourceFiles->count > XLC_MAX_C_SOURCE_FILES ) {
        xlSetError(ctx, "too many C source files");
        return 0;
    }
    headerText = NULL;
    if ( !xlEmitUnitHeaderToMemory(ctx, unit, "XL6_MODULE_H", &headerText) ) {
        return 0;
    }
    if ( !xlMountDynamicTccText(ctx, XL_TCC_VFS_MODULE_H, headerText) ||
         !xlSaveTempTextByName(ctx, "module.h", headerText) ) {
        xrtFree(headerText);
        return 0;
    }
    xrtFree(headerText);

    for ( i = 0; i < cSourceFiles->count; ++i ) {
        wrapperText = NULL;
        snprintf(wrapperName, sizeof(wrapperName), "csrc_%u.c", (unsigned int)i);
        snprintf(virtualPath, sizeof(virtualPath), "__xlvfs__/%s", wrapperName);
        if ( !xlBuildCSourceWrapperText(ctx, cSourceFiles->items[i], &wrapperText) ) {
            return 0;
        }
        if ( !xlSaveTempTextByName(ctx, wrapperName, wrapperText) ||
             !xlTccCompileSourceText(ctx, s, virtualPath, wrapperText, "cannot compile C source file") ) {
            xrtFree(wrapperText);
            return 0;
        }
        xrtFree(wrapperText);
    }
    return 1;
}

static int xlCompileAdditionalCSourceFilesDisk(XL_Context* ctx, TCCState* s, XL6_CompileUnit* unit, const XL_StringList* cSourceFiles)
{
    char* headerPath;
    char* wrapperPath;
    char wrapperName[64];
    char includePath[1024];
    char sourceText[1536];
    int written;
    size_t i;

    if ( cSourceFiles == NULL || cSourceFiles->count == 0 ) {
        return 1;
    }
    if ( ctx == NULL || s == NULL ) {
        xlSetError(ctx, "compiler context is empty");
        return 0;
    }
    if ( ctx->tempDir == NULL || ctx->tempDir[0] == '\0' ) {
        xlSetError(ctx, "compiler temp dir is empty");
        return 0;
    }
    if ( cSourceFiles->count > XLC_MAX_C_SOURCE_FILES ) {
        xlSetError(ctx, "too many C source files");
        return 0;
    }
    if ( !xrtDirCreateAll((str)ctx->tempDir) ) {
        xlSetError2(ctx, "cannot create compiler temp dir", ctx->tempDir);
        return 0;
    }

    headerPath = xlJoinTempFilePath(ctx, "module.h");
    if ( headerPath == NULL ) {
        return 0;
    }
    if ( !xlEmitUnitHeaderToFile(ctx, unit, headerPath, "XL6_MODULE_H") ) {
        xrtFree(headerPath);
        return 0;
    }
    if ( xl__tcc.tcc_add_include_path(s, ctx->tempDir) != 0 ) {
        xlSetError2(ctx, "cannot add compiler temp include path", ctx->tempDir);
        xrtFree(headerPath);
        return 0;
    }

    for ( i = 0; i < cSourceFiles->count; ++i ) {
        if ( !xlNormalizeCIncludePath(ctx, includePath, sizeof(includePath), cSourceFiles->items[i]) ) {
            xrtFree(headerPath);
            return 0;
        }
        snprintf(wrapperName, sizeof(wrapperName), "csrc_%u.c", (unsigned int)i);
        wrapperPath = xlJoinTempFilePath(ctx, wrapperName);
        if ( wrapperPath == NULL ) {
            xrtFree(headerPath);
            return 0;
        }
        written = snprintf(sourceText,
                           sizeof(sourceText),
                           "/* generated wrapper for %s */\n"
                           "#include \"lib/xrt/xrt.h\"\n"
                           "#include \"module.h\"\n"
                           "#include \"%s\"\n",
                           includePath,
                           includePath);
        if ( written <= 0 || (size_t)written >= sizeof(sourceText) ) {
            xlSetError2(ctx, "generated C source wrapper is too long", cSourceFiles->items[i]);
            xrtFree(wrapperPath);
            xrtFree(headerPath);
            return 0;
        }
        if ( !xlWriteTempTextFile(ctx, wrapperPath, sourceText) ) {
            xrtFree(wrapperPath);
            xrtFree(headerPath);
            return 0;
        }
        if ( xl__tcc.tcc_add_file(s, wrapperPath) != 0 ) {
            if ( ctx->lastError == NULL ) {
                xlSetError2(ctx, "cannot compile C source file", cSourceFiles->items[i]);
            }
            xrtFree(wrapperPath);
            xrtFree(headerPath);
            return 0;
        }
        xrtFree(wrapperPath);
    }
    xrtFree(headerPath);
    return 1;
}

static int xlCompileAdditionalCSourceFiles(XL_Context* ctx, TCCState* s, XL6_CompileUnit* unit, const XL_StringList* cSourceFiles)
{
    if ( cSourceFiles == NULL || cSourceFiles->count == 0 ) {
        return 1;
    }
#ifdef XL_STATIC_TCC_VFS
    return xlCompileAdditionalCSourceFilesVfs(ctx, s, unit, cSourceFiles);
#else
    return xlCompileAdditionalCSourceFilesDisk(ctx, s, unit, cSourceFiles);
#endif
}

static int xlAddTccSourceFile(XL_Context* ctx, TCCState* s, const char* path)
{
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "embedded TCC source path is empty");
        return 0;
    }
    if ( xl__tcc.tcc_add_file(s, path) != 0 ) {
        if ( ctx->lastError == NULL ) {
            xlSetError2(ctx, "cannot compile embedded TCC source", path);
        }
        return 0;
    }
    return 1;
}

static int xlAddEmbeddedTccRuntimeSources(XL_Context* ctx, TCCState* s, const XL6_CompileUnit* unit)
{
    unsigned int features;

    if ( ctx == NULL || s == NULL || unit == NULL ) {
        return 1;
    }
    features = unit->importDeps.features;
    if ( (features & (XL6_IMPORT_FEATURE_TCC_RUNTIME |
                      XL6_IMPORT_FEATURE_XLANG_RUNTIME |
                      XL6_IMPORT_FEATURE_XLANG_COMPILER)) == 0 ) {
        return 1;
    }

    /*
     * 这些宏描述“被编进用户产物里的 libtcc”。它们只在产物需要脚本能力时启用，
     * 普通 xlang 程序不会携带 TCC 源码和内置 VFS 资源。
     */
    (void)xl__tcc.tcc_set_options(s, "-DXL_STATIC_TCC -DLIBTCC_AS_DLL -DCONFIG_TCC_STATIC");
    (void)xl__tcc.tcc_add_include_path(s, "lib\\tcc");
    (void)xl__tcc.tcc_add_include_path(s, "lib\\lzma");
    (void)xl__tcc.tcc_add_include_path(s, "release");
    (void)xl__tcc.tcc_add_include_path(s, "release\\include");

    if ( !xlAddTccSourceFile(ctx, s, "lib\\tcc\\libtcc.c") ||
         !xlAddTccSourceFile(ctx, s, "lib\\tcc\\tcctools.c") ||
         !xlAddTccSourceFile(ctx, s, "lib\\tcc\\tcc_builtin_vfs.c") ||
         !xlAddTccSourceFile(ctx, s, "lib\\tcc\\tcc_utf8_io.c") ||
         !xlAddTccSourceFile(ctx, s, "lib\\lzma\\LzmaDec.c") ||
        !xlAddTccSourceFile(ctx, s, "build\\tmp\\tcc_builtin_resources_runtime.c") ) {
        return 0;
    }
    return 1;
}

static int xlAddEmbeddedXlangRuntimeSources(XL_Context* ctx, TCCState* s, const XL6_CompileUnit* unit)
{
    static const char* runtimeSources[] = {
        "src\\api\\xl_api.c",
        "src\\abi\\xl6_abi_json.c",
        "src\\abi\\xl6_callable.c",
        "src\\backend\\xl6_build_backend.c",
        "src\\compile\\xl6_compile_unit.c",
        "src\\compile\\xl6_imports.c",
        "src\\compile\\xl6_xrt_trim.c",
        "src\\common\\xl6_console.c",
        "src\\common\\xl6_output.c",
        "src\\diag\\xl6_diag.c",
        "src\\ast\\xl6_ast.c",
        "src\\cgen\\xl6_cgen_entry.c",
        "src\\cgen\\xl6_cgen_lifecycle.c",
        "src\\cgen\\xl6_cgen_runtime.c",
        "src\\cgen\\xl6_cgen_header.c",
        "src\\cgen\\xl6_cgen_needs_basic.c",
        "src\\cgen\\xl6_cgen.c",
        "src\\lexer\\xl6_token.c",
        "src\\lexer\\xl6_lexer_api.c",
        "src\\lexer\\xl6_lexer_gen.c",
        "src\\parser\\xl6_parser.c",
        "src\\parser\\xl6_parser_api.c",
        "src\\package\\xl6_package.c",
        "src\\stdlib\\xl6_stdlib.c",
        "src\\stdlib\\xl6_stdlib_data.c",
        "src\\type\\xl6_type_model.c",
        "src\\sem\\xl6_protocol.c",
        "src\\sem\\xl6_type_registry.c",
        "src\\sem\\xl6_sem.c"
    };
    static const char* compilerSources[] = {
        "src\\abi\\xl6_abi_dump.c",
        "src\\ast\\xl6_ast_dump.c",
        "src\\ir\\xl6_ir_dump.c"
    };
    unsigned int features;
    size_t i;

    if ( ctx == NULL || s == NULL || unit == NULL ) {
        return 1;
    }
    features = unit->importDeps.features;
    if ( (features & (XL6_IMPORT_FEATURE_XLANG_RUNTIME |
                      XL6_IMPORT_FEATURE_XLANG_COMPILER)) == 0 ) {
        return 1;
    }

    /*
     * import xlang 表示最终产物要内嵌一份可独立运行的 xlang 宿主 API。
     * 普通运行能力只开启 runtime-only 口径；只有 import xlang.compiler 才携带
     * 外部 exe/dll/obj 输出和调试 dump 相关源码。
     */
    (void)xl__tcc.tcc_set_options(s, "-DXL_BUILD_DLL -DXL_STATIC_TCC -DLIBTCC_AS_DLL -DCONFIG_TCC_STATIC");
    if ( (features & XL6_IMPORT_FEATURE_XLANG_COMPILER) == 0 ) {
        (void)xl__tcc.tcc_set_options(s, "-DXL_RUNTIME_ONLY_DLL -DXL_RUNTIME_ONLY_API");
    }
    (void)xl__tcc.tcc_add_include_path(s, ".");
    (void)xl__tcc.tcc_add_include_path(s, "src");
    (void)xl__tcc.tcc_add_include_path(s, "release");
    (void)xl__tcc.tcc_add_include_path(s, "release\\include");
    (void)xl__tcc.tcc_add_include_path(s, "lib\\tcc");
    (void)xl__tcc.tcc_add_include_path(s, "lib\\lzma");

    for ( i = 0; i < sizeof(runtimeSources) / sizeof(runtimeSources[0]); ++i ) {
        if ( !xlAddTccSourceFile(ctx, s, runtimeSources[i]) ) {
            return 0;
        }
    }
    if ( (features & XL6_IMPORT_FEATURE_XLANG_COMPILER) != 0 ) {
        for ( i = 0; i < sizeof(compilerSources) / sizeof(compilerSources[0]); ++i ) {
            if ( !xlAddTccSourceFile(ctx, s, compilerSources[i]) ) {
                return 0;
            }
        }
    }
    return 1;
}
#endif

static int xlPrepareRuntimeArg(XL_Context* ctx, xvalue suppliedArg, xvalue* outArg)
{
    if ( outArg == NULL ) {
        return 0;
    }
    *outArg = NULL;
    if ( suppliedArg != NULL ) {
        if ( !xvoIsArray(suppliedArg) ) {
            xlSetError(ctx, "script arg must be array");
            return 0;
        }
        xvoAddRef(suppliedArg);
        *outArg = suppliedArg;
        return 1;
    }
    *outArg = xvoCreateArray();
    if ( *outArg == NULL ) {
        xlSetError(ctx, "cannot create script arg array");
        return 0;
    }
    return 1;
}

static int xlRunMainInState(XL_Context* ctx, TCCState* state, int argc, char** argv, const char* missingError, const char* returnPrefix)
{
    XL_CMainProc entry;
    int rc;

    if ( ctx == NULL || state == NULL ) {
        xlSetError(ctx, "module is empty");
        return 0;
    }
    entry = (XL_CMainProc)xl__tcc.tcc_get_symbol(state, "main");
    if ( entry == NULL ) {
        xlSetError(ctx, missingError != NULL ? missingError : "entry main not found");
        return 0;
    }
    rc = entry(argc > 0 ? argc : 0, argv);
    ctx->lastExitCode = rc;
    (void)returnPrefix;
    return 1;
}

static void xlAddDefaultTccLinkLibraries(TCCState* s)
{
#ifdef _WIN32
    /*
     * XRT 是 xlang 的基础运行库。Windows 版本的 XRT 会使用 socket、
     * IP Helper 和 shell 等系统能力，因此生成外部 exe/dll 时需要默认
     * 链接这些库；内存运行路径调用它也无害，并能让用户直接使用同一组
     * TCC 能力。这里不强制检查返回值，让最终链接阶段给出精确诊断。
     */
# ifdef XL_STATIC_TCC
    (void)tcc_add_library(s, "Ws2_32");
    (void)tcc_add_library(s, "IPHLPAPI");
    (void)tcc_add_library(s, "Shell32");
# else
    if ( xl__tcc.tcc_add_library != NULL ) {
        (void)xl__tcc.tcc_add_library(s, "Ws2_32");
        (void)xl__tcc.tcc_add_library(s, "IPHLPAPI");
        (void)xl__tcc.tcc_add_library(s, "Shell32");
    }
# endif
#else
    (void)s;
#endif
}

static int xlCheckScriptActiveError(XL_Context* ctx, TCCState* state)
{
    XL_ScriptHasErrorProc hasError;
    XL_ScriptVoidProc printUncaught;
    XL_ScriptVoidProc clearError;

    if ( state == NULL ) {
        return 0;
    }
    hasError = (XL_ScriptHasErrorProc)xl__tcc.tcc_get_symbol(state, "xl_script_has_error");
    if ( hasError == NULL || !hasError() ) {
        return 0;
    }
    printUncaught = (XL_ScriptVoidProc)xl__tcc.tcc_get_symbol(state, "xl_script_print_uncaught");
    clearError = (XL_ScriptVoidProc)xl__tcc.tcc_get_symbol(state, "xl_script_clear_error");
    if ( printUncaught != NULL ) {
        printUncaught();
    } else {
        xlSetError(ctx, "uncaught xlang error");
    }
    if ( clearError != NULL ) {
        clearError();
    }
    return 1;
}

static int xlCompileAndRunUnit(XL_Context* ctx, XL6_CompileUnit* unit, xvalue suppliedArg)
{
    TCCState* s;
    XL_XlangMainProc entry;
    xvalue runArg;
    char* cText;
    XL6_CgenOptions cgenOptions;
    int ok;
    int rc;
    size_t i;

    s = NULL;
    entry = NULL;
    cText = NULL;
    runArg = NULL;
    ok = 0;
    xlTccDynamicVfsClear();
    if ( !xlPrepareRuntimeArg(ctx, suppliedArg, &runArg) ) {
        goto cleanup;
    }
    memset(&cgenOptions, 0, sizeof(cgenOptions));
    cgenOptions.suppressMainWrapper = 1;
    if ( !xlCgenToMemoryEx(ctx, unit, &cText, &cgenOptions) ) {
        goto cleanup;
    }
    s = xlCreateTccState(ctx, TCC_OUTPUT_MEMORY);
    if ( s == NULL ) {
        goto cleanup;
    }
    for ( i = 0; i < unit->importDeps.cSources.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, unit->importDeps.cSources.items[i]) != 0 ) {
            if ( ctx->lastError == NULL ) {
                xlSetError2(ctx, "cannot compile import C source", unit->importDeps.cSources.items[i]);
            }
            goto cleanup;
        }
    }
    if ( !xlTccCompileSourceText(ctx, s, XL_TCC_VFS_MODULE_C, cText, "TCC compile failed") ) {
        goto cleanup;
    }
    if ( !xlAddDependenciesToTcc(ctx, s, unit) ) {
        goto cleanup;
    }
    if ( xl__tcc.tcc_relocate(s) != 0 ) {
        if ( ctx->lastError == NULL ) {
            xlSetError(ctx, "TCC relocate failed");
        }
        goto cleanup;
    }
    entry = (XL_XlangMainProc)xl__tcc.tcc_get_symbol(s, "xlang_main");
    if ( entry == NULL ) {
        xlSetError(ctx, "script entry xlang_main not found");
        goto cleanup;
    }
    /*
     * 编译器/TCC 全局状态需要串行化，但用户脚本执行期不能一直持有
     * xlApiLock。否则脚本中的 HTTP/任务线程再调用 xlRunScript* 会等待
     * 外层脚本结束，形成跨线程嵌套运行死锁。
     */
    xlApiUnlock();
    rc = entry(runArg);
    xlApiLock();
    if ( xlCheckScriptActiveError(ctx, s) ) {
        goto cleanup;
    }
    ctx->lastExitCode = rc;
    ok = 1;

cleanup:
    if ( s != NULL ) {
        xl__tcc.tcc_delete(s);
    }
    if ( runArg != NULL ) {
        xvoUnref(runArg);
    }
    xrtFree(cText);
    xlTccDynamicVfsClear();
    return ok;
}

#if XL_ENABLE_EXE_COMPILER
static int xlCompileUnitToExternalOutputWithC(
    XL_Context* ctx,
    XL6_CompileUnit* unit,
    const char* outputPath,
    int outputType,
    const char* outputKind,
    const XL6_CgenOptions* options,
    const XL_StringList* cSourceFiles)
{
    TCCState* s;
    char* cText;
    char* xrtText;
    int ok;
    size_t i;

    s = NULL;
    if ( outputPath == NULL || outputPath[0] == '\0' ) {
        char text[128];
        snprintf(text, sizeof(text), "output %s path is empty", outputKind != NULL ? outputKind : "file");
        xlSetError(ctx, text);
        return 0;
    }
    cText = NULL;
    xrtText = NULL;
    ok = 0;
    xlTccDynamicVfsClear();
    if ( !xlCgenToMemoryEx(ctx, unit, &cText, options) ) {
        goto cleanup;
    }
    if ( !xlSaveTempTextByName(ctx, "module.c", cText) ) {
        goto cleanup;
    }
    s = xlCreateTccState(ctx, outputType);
    if ( s == NULL ) {
        goto cleanup;
    }
    (void)xl__tcc.tcc_set_options(s, "-DXL6_USE_XRT_LIFECYCLE");
    if ( !xlTccCompileSourceText(ctx, s, XL_TCC_VFS_RUNTIME_C,
            "__attribute__((weak)) unsigned int __mzerosf = 0x80000000u;\n"
            "__attribute__((weak)) unsigned long long __mzerodf = 0x8000000000000000ull;\n",
            "cannot compile TCC runtime constants") ) {
        goto cleanup;
    }
    if ( !xlTccCompileSourceText(ctx, s, XL_TCC_VFS_MODULE_C, cText, "TCC compile failed") ) {
        goto cleanup;
    }
    xlAddDefaultTccRuntime(s, ctx, outputType);
    for ( i = 0; i < unit->importDeps.cSources.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, unit->importDeps.cSources.items[i]) != 0 ) {
            if ( ctx->lastError == NULL ) {
                xlSetError2(ctx, "cannot compile import C source", unit->importDeps.cSources.items[i]);
            }
            goto cleanup;
        }
    }
    if ( !xlCompileAdditionalCSourceFiles(ctx, s, unit, cSourceFiles) ) {
        goto cleanup;
    }
    if ( !xlAddDependenciesToTcc(ctx, s, unit) ) {
        goto cleanup;
    }
    if ( !xlAddEmbeddedTccRuntimeSources(ctx, s, unit) ) {
        goto cleanup;
    }
    if ( !xlAddEmbeddedXlangRuntimeSources(ctx, s, unit) ) {
        goto cleanup;
    }
    if ( !xl6XrtTrimBuildImplementationSourceWithGeneratedSource(unit->parse.root, &unit->importDeps, cText, &xrtText) ) {
        xlSetError(ctx, "cannot generate XRT implementation source");
        goto cleanup;
    }
    if ( !xlSaveTempTextByName(ctx, "xrt_impl.c", xrtText) ) {
        goto cleanup;
    }
    if ( !xlTccCompileSourceText(ctx, s, XL_TCC_VFS_XRT_C, xrtText, "cannot compile XRT single header") ) {
        goto cleanup;
    }
    ok = xl__tcc.tcc_output_file(s, outputPath) == 0;
    if ( !ok ) {
        if ( ctx->lastError == NULL ) {
            xlSetError2(ctx, outputType == TCC_OUTPUT_DLL ? "cannot output shared library" : "cannot output exe", outputPath);
        }
        goto cleanup;
    }
    ok = 1;

cleanup:
    if ( s != NULL ) {
        xl__tcc.tcc_delete(s);
    }
    xrtFree(cText);
    xrtFree(xrtText);
    xlTccDynamicVfsClear();
    return ok;
}

static int xlCompileUnitToExternalOutputEx(
    XL_Context* ctx,
    XL6_CompileUnit* unit,
    const char* outputPath,
    int outputType,
    const char* outputKind,
    const XL6_CgenOptions* options)
{
    return xlCompileUnitToExternalOutputWithC(ctx, unit, outputPath, outputType, outputKind, options, NULL);
}

static int xlCompileUnitToExternalOutput(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputPath, int outputType, const char* outputKind)
{
    return xlCompileUnitToExternalOutputEx(ctx, unit, outputPath, outputType, outputKind, NULL);
}

static int xlCompileUnitToExe(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputExe)
{
    return xlCompileUnitToExternalOutput(ctx, unit, outputExe, TCC_OUTPUT_EXE, "exe");
}

static int xlCompileUnitToDebugExe(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputExe, const char* debugSourcePath)
{
    XL6_CgenOptions options;

    memset(&options, 0, sizeof(options));
    options.debugMode = 1;
    options.debugSourcePath = debugSourcePath;
    return xlCompileUnitToExternalOutputEx(ctx, unit, outputExe, TCC_OUTPUT_EXE, "debug exe", &options);
}

static int xlAstIsTopLevelRuntimeStmt(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return 0;
    }
    switch ( node->kind ) {
    case XL6_AST_IMPORT:
    case XL6_AST_FUNC:
    case XL6_AST_ASYNC_FUNC:
    case XL6_AST_EXTERN_FUNC:
    case XL6_AST_EXPORT_FUNC:
    case XL6_AST_EXTERN_VAR:
    case XL6_AST_EXPORT_VAR:
    case XL6_AST_EXPORT_CONST:
    case XL6_AST_VAR_DECL:
    case XL6_AST_CONST_DECL:
    case XL6_AST_TYPE_ALIAS:
    case XL6_AST_EXTERN_TYPE_ALIAS:
    case XL6_AST_UNION:
    case XL6_AST_CLASS:
        return 0;
    default:
        return 1;
    }
}

static const XL6_AstNode* xlAstFindMainFunc(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return NULL;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        const XL6_AstNode* node = root->children[i];
        const XL6_AstNode* nameNode;
        if ( node == NULL || node->kind != XL6_AST_FUNC || node->childCount == 0 ) {
            continue;
        }
        nameNode = node->children[0];
        if ( nameNode != NULL && nameNode->text != NULL && strcmp(nameNode->text, "main") == 0 ) {
            return node;
        }
    }
    return NULL;
}

static int xlAstHasTopLevelRuntimeStmt(const XL6_AstNode* root)
{
    size_t i;

    if ( root == NULL ) {
        return 0;
    }
    for ( i = 0; i < root->childCount; ++i ) {
        if ( xlAstIsTopLevelRuntimeStmt(root->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xlCompileUnitHasExecutableEntry(XL_Context* ctx, XL6_CompileUnit* unit)
{
    if ( ctx == NULL || unit == NULL ) {
        return -1;
    }
    return xlAstFindMainFunc(unit->parse.root) != NULL ||
           xlAstHasTopLevelRuntimeStmt(unit->parse.root);
}

static int xlCompileUnitToSharedLibraryWithC(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputDll, const XL_StringList* cSourceFiles)
{
    int hasEntry;

    hasEntry = xlCompileUnitHasExecutableEntry(ctx, unit);
    if ( hasEntry < 0 ) {
        return 0;
    }
    if ( hasEntry ) {
        xlSetError(ctx, "shared build cannot contain xlang main or top-level executable statements");
        return 0;
    }
    return xlCompileUnitToExternalOutputWithC(ctx, unit, outputDll, TCC_OUTPUT_DLL, "shared library", NULL, cSourceFiles);
}

static int xlCompileUnitToSharedLibrary(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputDll)
{
    return xlCompileUnitToSharedLibraryWithC(ctx, unit, outputDll, NULL);
}
#endif

XL_ContextOptions xlDefaultContextOptions(void)
{
    XL_ContextOptions options;

    memset(&options, 0, sizeof(options));
    options.size = (unsigned int)sizeof(options);
    options.version = XL_API_VERSION;
    options.workDir = ".";
    options.tempDir = "build\\tmp";
    options.tccDllPath = NULL;
    options.tccLibPath = NULL;
    options.saveTemps = 0;
    return options;
}

static void xlClearLastErrorUnlocked(XL_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->lastError);
    ctx->lastError = NULL;
    ctx->lastExitCode = 0;
}

static int xlRuntimeRefTotal(void)
{
    return xl__runtimeManualCount + xl__runtimeContextCount;
}

static int xlRuntimeAcquireManualUnlocked(void)
{
    if ( xlRuntimeRefTotal() == 0 ) {
        if ( xrtInit() == NULL ) {
            return 0;
        }
    }
    xl__runtimeManualCount += 1;
    return 1;
}

static void xlRuntimeReleaseManualUnlocked(void)
{
    if ( xl__runtimeManualCount <= 0 ) {
        return;
    }
    xl__runtimeManualCount -= 1;
    if ( xlRuntimeRefTotal() == 0 ) {
        xrtUnit();
    }
}

static int xlRuntimeAcquireContextUnlocked(void)
{
    if ( xlRuntimeRefTotal() == 0 ) {
        if ( xrtInit() == NULL ) {
            return 0;
        }
    }
    xl__runtimeContextCount += 1;
    return 1;
}

static void xlRuntimeReleaseContextUnlocked(void)
{
    if ( xl__runtimeContextCount <= 0 ) {
        return;
    }
    xl__runtimeContextCount -= 1;
    if ( xlRuntimeRefTotal() == 0 ) {
        xrtUnit();
    }
}

int xlRuntimeInit(void)
{
    int ok;

    xlApiLock();
    ok = xlRuntimeAcquireManualUnlocked();
    xlApiUnlock();
    return ok;
}

void xlRuntimeUnit(void)
{
    xlApiLock();
    xlRuntimeReleaseManualUnlocked();
    xlApiUnlock();
}

static XL_Module* xlCreateModuleFromState(XL_Context* ctx, TCCState* s)
{
    XL_Module* module;

    if ( s == NULL ) {
        return NULL;
    }
    if ( !xlRuntimeAcquireManualUnlocked() ) {
        xlSetError(ctx, "cannot keep runtime alive for module");
        return NULL;
    }
    module = (XL_Module*)xrtCalloc(1, sizeof(XL_Module));
    if ( module == NULL ) {
        xlRuntimeReleaseManualUnlocked();
        xlSetError(ctx, "cannot allocate module");
        return NULL;
    }
    module->state = s;
    module->ownsRuntimeRef = 1;
    return module;
}

static void xlDestroyModuleUnlocked(XL_Module* module)
{
    if ( module == NULL ) {
        return;
    }
    if ( module->state != NULL ) {
        xl__tcc.tcc_delete(module->state);
        module->state = NULL;
    }
    if ( module->ownsRuntimeRef ) {
        module->ownsRuntimeRef = 0;
        xlRuntimeReleaseManualUnlocked();
    }
    xrtFree(module);
}

static XL_Module* xlCompileUnitToModuleWithC(XL_Context* ctx, XL6_CompileUnit* unit, const XL6_CgenOptions* options, const XL_StringList* cSourceFiles)
{
    TCCState* s;
    XL_Module* module;
    char* cText;
    size_t i;

    s = NULL;
    module = NULL;
    cText = NULL;
    xlTccDynamicVfsClear();
    if ( !xlCgenToMemoryEx(ctx, unit, &cText, options) ) {
        goto cleanup;
    }
#if XL_ENABLE_EXE_COMPILER
    if ( !xlSaveTempTextByName(ctx, "module.c", cText) ) {
        goto cleanup;
    }
#endif
    s = xlCreateTccState(ctx, TCC_OUTPUT_MEMORY);
    if ( s == NULL ) {
        goto cleanup;
    }
    for ( i = 0; unit != NULL && i < unit->importDeps.cSources.count; ++i ) {
        if ( xl__tcc.tcc_add_file(s, unit->importDeps.cSources.items[i]) != 0 ) {
            if ( ctx != NULL && ctx->lastError == NULL ) {
                xlSetError2(ctx, "cannot compile import C source", unit->importDeps.cSources.items[i]);
            }
            goto cleanup;
        }
    }
    if ( !xlTccCompileSourceText(ctx, s, XL_TCC_VFS_MODULE_C, cText, "TCC compile failed") ) {
        goto cleanup;
    }
#if XL_ENABLE_EXE_COMPILER
    if ( !xlCompileAdditionalCSourceFiles(ctx, s, unit, cSourceFiles) ) {
        goto cleanup;
    }
#else
    (void)cSourceFiles;
#endif
    if ( !xlAddDependenciesToTcc(ctx, s, unit) ) {
        goto cleanup;
    }
    if ( xl__tcc.tcc_relocate(s) != 0 ) {
        if ( ctx != NULL && ctx->lastError == NULL ) {
            xlSetError(ctx, "TCC relocate failed");
        }
        goto cleanup;
    }
    module = xlCreateModuleFromState(ctx, s);
    if ( module != NULL ) {
        s = NULL;
    }

cleanup:
    if ( s != NULL ) {
        xl__tcc.tcc_delete(s);
    }
    xrtFree(cText);
    xlTccDynamicVfsClear();
    return module;
}

static XL_Module* xlCompileUnitToModuleEx(XL_Context* ctx, XL6_CompileUnit* unit, const XL6_CgenOptions* options)
{
    return xlCompileUnitToModuleWithC(ctx, unit, options, NULL);
}

static XL_Module* xlCompileUnitToModule(XL_Context* ctx, XL6_CompileUnit* unit)
{
    return xlCompileUnitToModuleEx(ctx, unit, NULL);
}

static XL_Module* xlCompileCSourceTextToModule(XL_Context* ctx, const char* source, const char* virtualPath)
{
    TCCState* s;
    XL_Module* module;
    const char* path;

    if ( source == NULL ) {
        xlSetError(ctx, "C source is empty");
        return NULL;
    }
    path = virtualPath != NULL && virtualPath[0] != '\0' ? virtualPath : XL_TCC_VFS_C_MODULE_C;
    s = NULL;
    module = NULL;
    xlTccDynamicVfsClear();
    s = xlCreateTccState(ctx, TCC_OUTPUT_MEMORY);
    if ( s == NULL ) {
        goto cleanup;
    }
    if ( !xlTccCompileSourceText(ctx, s, path, source, "TCC C compile failed") ) {
        goto cleanup;
    }
    if ( !xlAddDependenciesToTcc(ctx, s, NULL) ) {
        goto cleanup;
    }
    if ( xl__tcc.tcc_relocate(s) != 0 ) {
        if ( ctx != NULL && ctx->lastError == NULL ) {
            xlSetError(ctx, "TCC C relocate failed");
        }
        goto cleanup;
    }
    module = xlCreateModuleFromState(ctx, s);
    if ( module != NULL ) {
        s = NULL;
    }

cleanup:
    if ( s != NULL ) {
        xl__tcc.tcc_delete(s);
    }
    xlTccDynamicVfsClear();
    return module;
}

static int xlAddCSourceFilesToTcc(XL_Context* ctx, TCCState* s, const char** paths, int pathCount)
{
    int i;

    if ( s == NULL ) {
        xlSetError(ctx, "TCC state is empty");
        return 0;
    }
    if ( paths == NULL || pathCount <= 0 ) {
        xlSetError(ctx, "C source file list is empty");
        return 0;
    }
    if ( pathCount > XLC_MAX_C_SOURCE_FILES ) {
        xlSetError(ctx, "too many C source files");
        return 0;
    }
    for ( i = 0; i < pathCount; ++i ) {
        if ( paths[i] == NULL || paths[i][0] == '\0' ) {
            xlSetError(ctx, "C source file path is empty");
            return 0;
        }
        if ( xl__tcc.tcc_add_file(s, paths[i]) != 0 ) {
            if ( ctx != NULL && ctx->lastError == NULL ) {
                xlSetError2(ctx, "cannot compile C source file", paths[i]);
            }
            return 0;
        }
    }
    return 1;
}

static XL_Module* xlCompileCSourceFilesToModuleUnlocked(XL_Context* ctx, const char** paths, int pathCount)
{
    TCCState* s;
    XL_Module* module;

    s = NULL;
    module = NULL;
    xlTccDynamicVfsClear();
    s = xlCreateTccState(ctx, TCC_OUTPUT_MEMORY);
    if ( s == NULL ) {
        goto cleanup;
    }
    if ( !xlAddCSourceFilesToTcc(ctx, s, paths, pathCount) ) {
        goto cleanup;
    }
    if ( !xlAddDependenciesToTcc(ctx, s, NULL) ) {
        goto cleanup;
    }
    if ( xl__tcc.tcc_relocate(s) != 0 ) {
        if ( ctx != NULL && ctx->lastError == NULL ) {
            xlSetError(ctx, "TCC C relocate failed");
        }
        goto cleanup;
    }
    module = xlCreateModuleFromState(ctx, s);
    if ( module != NULL ) {
        s = NULL;
    }

cleanup:
    if ( s != NULL ) {
        xl__tcc.tcc_delete(s);
    }
    xlTccDynamicVfsClear();
    return module;
}

static XL_Module* xlCompileCSourceFileToModuleUnlocked(XL_Context* ctx, const char* path)
{
    const char* paths[1];

    paths[0] = path;
    return xlCompileCSourceFilesToModuleUnlocked(ctx, paths, 1);
}

static int xlRunModuleMainUnlocked(XL_Context* ctx, XL_Module* module, xvalue suppliedArg)
{
    XL_XlangMainProc entry;
    xvalue runArg;
    int rc;

    if ( ctx == NULL || module == NULL || module->state == NULL ) {
        xlSetError(ctx, "module is empty");
        return 0;
    }
    entry = (XL_XlangMainProc)xl__tcc.tcc_get_symbol(module->state, "xlang_main");
    if ( entry == NULL ) {
        xlSetError(ctx, "script entry xlang_main not found");
        return 0;
    }
    runArg = NULL;
    if ( !xlPrepareRuntimeArg(ctx, suppliedArg, &runArg) ) {
        return 0;
    }
    /*
     * module 已经完成编译和 relocate。执行用户入口时释放 API 全局锁，
     * 允许服务线程中的 XLangHandler 复用已编译模块。
     */
    xlApiUnlock();
    rc = entry(runArg);
    xlApiLock();
    if ( runArg != NULL ) {
        xvoUnref(runArg);
    }
    if ( xlCheckScriptActiveError(ctx, module->state) ) {
        return 0;
    }
    ctx->lastExitCode = rc;
    return 1;
}

static int xlRunCModuleMainUnlocked(XL_Context* ctx, XL_Module* module, int argc, char** argv)
{
    if ( ctx == NULL || module == NULL || module->state == NULL ) {
        xlSetError(ctx, "module is empty");
        return 0;
    }
    return xlRunMainInState(ctx, module->state, argc, argv, "C entry main not found", "C main returned");
}

#if XL_ENABLE_EXE_COMPILER
static int xlEmitUnitCToFileEx(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputPath, const XL6_CgenOptions* options)
{
    XL6_Output out;
    int ok;

    if ( outputPath == NULL || outputPath[0] == '\0' ) {
        xlSetError(ctx, "output C path is empty");
        return 0;
    }
    if ( unit == NULL || unit->parse.root == NULL ) {
        xlSetError(ctx, "compile unit is empty");
        return 0;
    }
    if ( !xl6OutputOpenFile(&out, outputPath) ) {
        xlSetError2(ctx, "cannot open C output", outputPath);
        return 0;
    }
    ok = xl6CgenEmitCEx(&out, unit->parse.root, &unit->parse.diags, options);
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot write C output", xl6OutputError(&out));
        ok = 0;
    } else if ( !ok ) {
        xlSetCompileDiagError(ctx, unit);
    }
    xl6OutputClose(&out);
    return ok;
}

static int xlEmitUnitCToFile(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputPath)
{
    return xlEmitUnitCToFileEx(ctx, unit, outputPath, NULL);
}

static int xlEmitUnitDebugCToFile(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputPath, const char* debugSourcePath)
{
    XL6_CgenOptions options;

    memset(&options, 0, sizeof(options));
    options.debugMode = 1;
    options.debugSourcePath = debugSourcePath;
    return xlEmitUnitCToFileEx(ctx, unit, outputPath, &options);
}

static int xlEmitUnitHeaderToFile(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputPath, const char* guardName)
{
    XL6_Output out;
    int ok;

    if ( outputPath == NULL || outputPath[0] == '\0' ) {
        xlSetError(ctx, "output header path is empty");
        return 0;
    }
    if ( unit == NULL || unit->parse.root == NULL ) {
        xlSetError(ctx, "compile unit is empty");
        return 0;
    }
    if ( !xl6OutputOpenFile(&out, outputPath) ) {
        xlSetError2(ctx, "cannot open header output", outputPath);
        return 0;
    }
    ok = xl6CgenEmitHeader(&out,
                           unit->parse.root,
                           guardName != NULL && guardName[0] != '\0' ? guardName : "XL6_GENERATED_HEADER_H",
                           &unit->parse.diags);
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot write header output", xl6OutputError(&out));
        ok = 0;
    } else if ( !ok ) {
        xlSetCompileDiagError(ctx, unit);
    }
    xl6OutputClose(&out);
    return ok;
}

static int xlcOutputOpen(XL_Context* ctx, XL6_Output* out, const char* outputPath, const char* desc)
{
    int ok;

    if ( outputPath == NULL || outputPath[0] == '\0' || strcmp(outputPath, "-") == 0 ) {
        xl6OutputInitStdFile(out, stdout);
        return !xl6OutputHasFailed(out);
    }
    ok = xl6OutputOpenFile(out, outputPath);
    if ( !ok ) {
        xlSetError2(ctx, desc != NULL ? desc : "cannot open output", outputPath);
    }
    return ok;
}

static int xlcOutputClose(XL_Context* ctx, XL6_Output* out, const char* desc)
{
    xl6OutputClose(out);
    if ( xl6OutputHasFailed(out) ) {
        xlSetError2(ctx, desc != NULL ? desc : "cannot write output", xl6OutputError(out));
        return 0;
    }
    return 1;
}

static const char* xlcSourceLabel(const XLC_Context* ctx)
{
    if ( ctx == NULL ) {
        return "<none>";
    }
    if ( ctx->sourceKind == XLC_SOURCE_FILE ) {
        return ctx->sourcePath != NULL ? ctx->sourcePath : "<file>";
    }
    if ( ctx->sourceKind == XLC_SOURCE_STRING ) {
        return ctx->virtualPath != NULL ? ctx->virtualPath : "<string>";
    }
    return "<none>";
}

static void xlcEmitDiagText(XL6_Output* out, const XL6_DiagList* diags, const char* path, const char* phase, int success)
{
    size_t i;

    if ( success ) {
        xl6OutputPuts("ok\n", out);
        return;
    }
    if ( diags == NULL || diags->count == 0 ) {
        xl6OutputPrintf(out, "%s: [%s] compile failed\n",
                        path != NULL ? path : "<memory>",
                        phase != NULL ? phase : "compile");
        return;
    }
    for ( i = 0; i < diags->count; ++i ) {
        const XL6_Diag* diag = &diags->items[i];
        xl6OutputPrintf(out,
                        "%s:%d:%d: [%s] %s\n",
                        path != NULL ? path : "<memory>",
                        diag->line,
                        diag->column,
                        phase != NULL ? phase : "compile",
                        diag->message != NULL ? diag->message : "compile error");
    }
}

static int xlcParseSourceUnlocked(XLC_Context* ctx, XL6_ParseResult* result)
{
    int ok;

    xl6ParseResultInit(result);
    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    if ( ctx->sourceKind == XLC_SOURCE_FILE ) {
        ok = xl6ParseFile(ctx->sourcePath, result);
    } else if ( ctx->sourceKind == XLC_SOURCE_STRING ) {
        ok = xl6ParseSource(ctx->sourceText, ctx->virtualPath, result);
    } else {
        xlSetError(ctx->runtime, "compiler source is empty");
        return 0;
    }
    if ( !ok ) {
        if ( result->diags.count > 0 ) {
            char text[1200];
            const XL6_Diag* diag = &result->diags.items[0];
            snprintf(text,
                     sizeof(text),
                     "%s:%d:%d: [parse] %s",
                     xlcSourceLabel(ctx),
                     diag->line,
                     diag->column,
                     diag->message != NULL ? diag->message : "parse error");
            xlSetError(ctx->runtime, text);
        } else {
            xlSetError(ctx->runtime, "xlang parse failed");
        }
    }
    return ok;
}

static xvalue xlcCreateArgArray(XL_Context* runtime, int argc, char** argv)
{
    xvalue args;
    int i;

    args = xvoCreateArray();
    if ( args == NULL ) {
        xlSetError(runtime, "cannot create script arg array");
        return NULL;
    }
    for ( i = 0; i < argc; ++i ) {
        const char* text = argv != NULL && argv[i] != NULL ? argv[i] : "";
        if ( !xvoArrayAppendText(args, (ptr)text, 0, FALSE) ) {
            xvoUnref(args);
            xlSetError(runtime, "cannot append script argument");
            return NULL;
        }
    }
    return args;
}
#endif

static void xlDestroyContextUnlocked(XL_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->baseDir);
    xrtFree(ctx->workDir);
    xrtFree(ctx->tempDir);
    xrtFree(ctx->tccDllPath);
    xrtFree(ctx->tccLibPath);
    xrtFree(ctx->tccIncludePath);
    xrtFree(ctx->tccWinIncludePath);
    xrtFree(ctx->tccWinapiIncludePath);
    xrtFree(ctx->tccLibSearchPath);
    xrtFree(ctx->tccLibtcc1Path);
    xrtFree(ctx->lastError);
    xlStringListFree(&ctx->includePaths);
    xlStringListFree(&ctx->libraryPaths);
    xlStringListFree(&ctx->linkLibraries);
    xlFreeSymbols(ctx);
    xlLoadedLibrariesFree(ctx);
    xrtFree(ctx);
    xlRuntimeReleaseContextUnlocked();
}

XL_Context* xlCreateContext(const XL_ContextOptions* options)
{
    XL_ContextOptions localOptions;
    XL_Context* ctx;

    xlApiLock();
    if ( !xlRuntimeAcquireContextUnlocked() ) {
        xlApiUnlock();
        return NULL;
    }
    localOptions = options != NULL ? *options : xlDefaultContextOptions();
    if ( localOptions.size != 0 && localOptions.size < sizeof(XL_ContextOptions) ) {
        xlRuntimeReleaseContextUnlocked();
        xlApiUnlock();
        return NULL;
    }
    if ( localOptions.version != 0 && localOptions.version != XL_API_VERSION ) {
        xlRuntimeReleaseContextUnlocked();
        xlApiUnlock();
        return NULL;
    }
    ctx = (XL_Context*)xrtCalloc(1, sizeof(XL_Context));
    if ( ctx == NULL ) {
        xlRuntimeReleaseContextUnlocked();
        xlApiUnlock();
        return NULL;
    }
    ctx->baseDir = xlGetCurrentDirText(NULL);
    ctx->workDir = xlPathSnapshot(ctx->baseDir, localOptions.workDir != NULL ? localOptions.workDir : ".");
    ctx->tempDir = localOptions.tempDir != NULL ?
                   xlPathSnapshot(ctx->baseDir, localOptions.tempDir) :
                   xlDefaultTempDirSnapshot(ctx->baseDir);
    ctx->tccDllPath = xlPathSnapshot(ctx->baseDir, localOptions.tccDllPath != NULL ? localOptions.tccDllPath : "");
    ctx->tccLibPath = localOptions.tccLibPath != NULL && localOptions.tccLibPath[0] != '\0' ?
                      xlPathSnapshot(ctx->baseDir, localOptions.tccLibPath) :
                      xlToolchainPathSnapshot(ctx->baseDir, xlDefaultTccRootPath());
    ctx->tccIncludePath = xlToolchainPathSnapshot(ctx->baseDir, xlDefaultTccIncludePath());
    ctx->tccWinIncludePath = xlToolchainPathSnapshot(ctx->baseDir, xlDefaultTccWinIncludePath());
    ctx->tccWinapiIncludePath = xlToolchainPathSnapshot(ctx->baseDir, xlDefaultTccWinapiIncludePath());
    ctx->tccLibSearchPath = xlToolchainPathSnapshot(ctx->baseDir, xlDefaultTccLibPath());
    ctx->tccLibtcc1Path = xlToolchainPathSnapshot(ctx->baseDir, xlDefaultTccLibtcc1Path());
    ctx->saveTemps = localOptions.saveTemps != 0;
    if ( ctx->baseDir == NULL || ctx->workDir == NULL || ctx->tempDir == NULL ||
         ctx->tccDllPath == NULL || ctx->tccLibPath == NULL ||
         ctx->tccIncludePath == NULL || ctx->tccWinIncludePath == NULL ||
         ctx->tccWinapiIncludePath == NULL || ctx->tccLibSearchPath == NULL ||
         ctx->tccLibtcc1Path == NULL ) {
        xlDestroyContextUnlocked(ctx);
        ctx = NULL;
    }
    xlApiUnlock();
    return ctx;
}

void xlDestroyContext(XL_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xlApiLock();
    xlDestroyContextUnlocked(ctx);
    xlApiUnlock();
}

int xlSetWorkDir(XL_Context* ctx, const char* path)
{
    char* copy;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "work dir path is empty");
        xlApiUnlock();
        return 0;
    }
    copy = xlPathSnapshotFromCurrentDir(ctx, path, "cannot allocate work dir");
    if ( copy == NULL ) {
        xlApiUnlock();
        return 0;
    }
    xrtFree(ctx->workDir);
    ctx->workDir = copy;
    xlApiUnlock();
    return 1;
}

int xlSetTempDir(XL_Context* ctx, const char* path)
{
    char* copy;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "temp dir path is empty");
        xlApiUnlock();
        return 0;
    }
    copy = xlPathSnapshotFromCurrentDir(ctx, path, "cannot allocate temp dir");
    if ( copy == NULL ) {
        xlApiUnlock();
        return 0;
    }
    xrtFree(ctx->tempDir);
    ctx->tempDir = copy;
    xlApiUnlock();
    return 1;
}

int xlSetSaveTemps(XL_Context* ctx, int enabled)
{
    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    ctx->saveTemps = enabled != 0;
    xlApiUnlock();
    return 1;
}

int xlAddIncludePath(XL_Context* ctx, const char* path)
{
    char* snapshot;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    snapshot = xlPathSnapshotFromCurrentDir(ctx, path, "cannot snapshot include path");
    ok = snapshot != NULL && xlStringListAppend(&ctx->includePaths, snapshot);
    xrtFree(snapshot);
    if ( !ok ) {
        xlSetError(ctx, "cannot add include path");
    }
    xlApiUnlock();
    return ok;
}

int xlAddLibraryPath(XL_Context* ctx, const char* path)
{
    char* snapshot;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    snapshot = xlPathSnapshotFromCurrentDir(ctx, path, "cannot snapshot library path");
    ok = snapshot != NULL && xlStringListAppend(&ctx->libraryPaths, snapshot);
    xrtFree(snapshot);
    if ( !ok ) {
        xlSetError(ctx, "cannot add library path");
    }
    xlApiUnlock();
    return ok;
}

int xlAddLinkLibrary(XL_Context* ctx, const char* nameOrPath)
{
    char* snapshot;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    snapshot = xlLinkValueSnapshotFromCurrentDir(ctx, nameOrPath, "cannot snapshot link library");
    ok = snapshot != NULL && xlStringListAppend(&ctx->linkLibraries, snapshot);
    xrtFree(snapshot);
    if ( !ok ) {
        xlSetError(ctx, "cannot add link library");
    }
    xlApiUnlock();
    return ok;
}

int xlLoadDependencyLibrary(XL_Context* ctx, const char* path)
{
    XL_LoadedLibrary loaded;
    char* snapshot;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "dependency library path is empty");
        xlApiUnlock();
        return 0;
    }
    memset(&loaded, 0, sizeof(loaded));
    snapshot = xlPathSnapshotFromCurrentDir(ctx, path, "cannot snapshot dependency library");
    if ( snapshot == NULL ) {
        xlApiUnlock();
        return 0;
    }
#ifdef _WIN32
    {
        u16str pathW = xrtUTF8to16((u8str)snapshot, 0, NULL);
        if ( pathW != NULL ) {
            loaded.module = LoadLibraryW((LPCWSTR)pathW);
            xrtFree(pathW);
        }
        if ( loaded.module == NULL ) {
            xlSetError2(ctx, "cannot load dependency library", snapshot);
            xrtFree(snapshot);
            xlApiUnlock();
            return 0;
        }
    }
#else
    loaded.module = dlopen(snapshot, RTLD_NOW | RTLD_GLOBAL);
    if ( loaded.module == NULL ) {
        xlSetError2(ctx, "cannot load dependency library", snapshot);
        xrtFree(snapshot);
        xlApiUnlock();
        return 0;
    }
#endif
    if ( !xlContextAddLoadedLibrary(ctx, snapshot, &loaded) ) {
        xlSetError(ctx, "cannot remember dependency library");
        xlCloseLoadedLibrary(&loaded);
        xrtFree(snapshot);
        xlApiUnlock();
        return 0;
    }
    xrtFree(snapshot);
    xlApiUnlock();
    return 1;
}

int xlRegisterCFunction(XL_Context* ctx, const char* symbolName, void* fn)
{
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    ok = xlContextAddSymbol(ctx, symbolName, fn);
    if ( !ok ) {
        xlSetError(ctx, "cannot register C function");
    }
    xlApiUnlock();
    return ok;
}

int xlRunScriptString(XL_Context* ctx, const char* source)
{
    return xlRunScriptStringArg(ctx, source, NULL);
}

int xlRunScriptStringArg(XL_Context* ctx, const char* source, xvalue arg)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlLoadUnitFromSource(ctx, &unit, source, "__xl_string.xl");
    if ( ok ) {
        ok = xlCompileAndRunUnit(ctx, &unit, arg);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

int xlRunScriptFile(XL_Context* ctx, const char* path)
{
    return xlRunScriptFileArg(ctx, path, NULL);
}

int xlRunScriptFileArg(XL_Context* ctx, const char* path, xvalue arg)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlLoadUnitFromFile(ctx, &unit, path);
    if ( ok ) {
        ok = xlCompileAndRunUnit(ctx, &unit, arg);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_Module* xlCompileScriptStringToModule(XL_Context* ctx, const char* source)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    XL_Module* module;
    int ok;

    if ( ctx == NULL ) {
        return NULL;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return NULL;
    }
    module = NULL;
    ok = xlLoadUnitFromSource(ctx, &unit, source, "__xl_string.xl");
    if ( ok ) {
        module = xlCompileUnitToModule(ctx, &unit);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return module;
}

XL_Module* xlCompileScriptFileToModule(XL_Context* ctx, const char* path)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    XL_Module* module;
    int ok;

    if ( ctx == NULL ) {
        return NULL;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return NULL;
    }
    module = NULL;
    ok = xlLoadUnitFromFile(ctx, &unit, path);
    if ( ok ) {
        module = xlCompileUnitToModule(ctx, &unit);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return module;
}

XL_Module* xlCompileCSourceStringToModule(XL_Context* ctx, const char* source)
{
    XL_CwdScope cwd;
    XL_Module* module;

    if ( ctx == NULL ) {
        return NULL;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return NULL;
    }
    module = xlCompileCSourceTextToModule(ctx, source, XL_TCC_VFS_C_MODULE_C);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return module;
}

XL_Module* xlCompileCSourceFileToModule(XL_Context* ctx, const char* path)
{
    XL_CwdScope cwd;
    XL_Module* module;

    if ( ctx == NULL ) {
        return NULL;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return NULL;
    }
    module = xlCompileCSourceFileToModuleUnlocked(ctx, path);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return module;
}

XL_API XL_Module* xlCompileCSourceFilesToModule(XL_Context* ctx, const char** paths, int pathCount)
{
    XL_CwdScope cwd;
    XL_Module* module;

    if ( ctx == NULL ) {
        return NULL;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return NULL;
    }
    module = xlCompileCSourceFilesToModuleUnlocked(ctx, paths, pathCount);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return module;
}

int xlRunCSourceString(XL_Context* ctx, const char* source, int argc, char** argv)
{
    XL_CwdScope cwd;
    XL_Module* module;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    module = xlCompileCSourceTextToModule(ctx, source, XL_TCC_VFS_C_MODULE_C);
    ok = module != NULL && xlRunCModuleMainUnlocked(ctx, module, argc, argv);
    xlDestroyModuleUnlocked(module);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

int xlRunCSourceFile(XL_Context* ctx, const char* path, int argc, char** argv)
{
    XL_CwdScope cwd;
    XL_Module* module;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    module = xlCompileCSourceFileToModuleUnlocked(ctx, path);
    ok = module != NULL && xlRunCModuleMainUnlocked(ctx, module, argc, argv);
    xlDestroyModuleUnlocked(module);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlRunCSourceFiles(XL_Context* ctx, const char** paths, int pathCount, int argc, char** argv)
{
    XL_CwdScope cwd;
    XL_Module* module;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    module = xlCompileCSourceFilesToModuleUnlocked(ctx, paths, pathCount);
    ok = module != NULL && xlRunCModuleMainUnlocked(ctx, module, argc, argv);
    xlDestroyModuleUnlocked(module);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

int xlRunModuleMain(XL_Context* ctx, XL_Module* module, xvalue arg)
{
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    ok = xlRunModuleMainUnlocked(ctx, module, arg);
    xlApiUnlock();
    return ok;
}

void* xlModuleGetSymbol(XL_Module* module, const char* symbolName)
{
    void* symbol;

    if ( module == NULL || module->state == NULL || symbolName == NULL || symbolName[0] == '\0' ) {
        return NULL;
    }
    xlApiLock();
    symbol = xl__tcc.tcc_get_symbol(module->state, symbolName);
    xlApiUnlock();
    return symbol;
}

void xlDestroyModule(XL_Module* module)
{
    if ( module == NULL ) {
        return;
    }
    xlApiLock();
    xlDestroyModuleUnlocked(module);
    xlApiUnlock();
}

XL_API int xlTccRegisterXrtEnvironment(TCCState* state)
{
    int ok;

    if ( state == NULL ) {
        return 0;
    }
    xlApiLock();
    ok = xlEnsureTcc(NULL) &&
         xlConfigureTccStateEnvironment(NULL, state, TCC_OUTPUT_MEMORY) &&
         xlAddDefaultRuntimeSymbols(state);
    xlApiUnlock();
    return ok;
}

XL_API int xlTccRegisterContextEnvironment(XL_Context* ctx, TCCState* state)
{
    int ok;

    if ( ctx == NULL || state == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    ok = xlEnsureTcc(ctx) &&
         xlConfigureTccStateEnvironment(ctx, state, TCC_OUTPUT_MEMORY) &&
         xlAddContextSymbols(ctx, state);
    xlApiUnlock();
    return ok;
}

XL_API int xlTccImpdef(const char* dllPath, const char* outputDef)
{
#ifdef _WIN32
    char* args[5];

    if ( dllPath == NULL || dllPath[0] == '\0' || outputDef == NULL || outputDef[0] == '\0' ) {
        return 0;
    }
    args[0] = (char*)"tcc";
    args[1] = (char*)dllPath;
    args[2] = (char*)"-o";
    args[3] = (char*)outputDef;
    args[4] = NULL;
    return tcc_tool_impdef(4, args) == 0;
#else
    (void)dllPath;
    (void)outputDef;
    return 0;
#endif
}

#if XL_ENABLE_EXE_COMPILER
int xlCompileScriptStringToExe(XL_Context* ctx, const char* source, const char* outputExe)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlLoadUnitFromSource(ctx, &unit, source, "__xl_string.xl");
    if ( ok ) {
        ok = xlCompileUnitToExe(ctx, &unit, outputExe);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

int xlCompileScriptFileToExe(XL_Context* ctx, const char* path, const char* outputExe)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlLoadUnitFromFile(ctx, &unit, path);
    if ( ok ) {
        ok = xlCompileUnitToExe(ctx, &unit, outputExe);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

static int xlCompileCSourceFilesToExternalOutput(
    XL_Context* ctx,
    const char** paths,
    int pathCount,
    const char* outputPath,
    int outputType,
    const char* outputKind)
{
    XL_CwdScope cwd;
    TCCState* s;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    if ( outputPath == NULL || outputPath[0] == '\0' ) {
        xlSetError(ctx, "C output path is empty");
        xlApiUnlock();
        return 0;
    }
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    s = xlCreateTccState(ctx, outputType);
    ok = s != NULL &&
         xlAddCSourceFilesToTcc(ctx, s, paths, pathCount) &&
         xlAddDependenciesToTcc(ctx, s, NULL) &&
         xlTccCompileSourceText(ctx, s, XL_TCC_VFS_XRT_C,
             "#define XRT_IMPLEMENTATION\n"
             "#include \"lib/xrt/xrt.h\"\n",
             "cannot compile XRT single header");
    if ( ok ) {
        ok = xl__tcc.tcc_output_file(s, outputPath) == 0;
    }
    if ( !ok && ctx->lastError == NULL ) {
        xlSetError2(ctx, outputKind != NULL ? outputKind : "cannot output C target", outputPath);
    }
    if ( s != NULL ) {
        xl__tcc.tcc_delete(s);
    }
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlCompileCSourceFilesToExe(XL_Context* ctx, const char** paths, int pathCount, const char* outputExe)
{
    return xlCompileCSourceFilesToExternalOutput(ctx, paths, pathCount, outputExe, TCC_OUTPUT_EXE, "cannot output C exe");
}

XL_API int xlCompileCSourceFileToExe(XL_Context* ctx, const char* path, const char* outputExe)
{
    const char* paths[1];

    paths[0] = path;
    return xlCompileCSourceFilesToExe(ctx, paths, 1, outputExe);
}

XL_API int xlCompileCSourceFilesToSharedLibrary(XL_Context* ctx, const char** paths, int pathCount, const char* outputDll)
{
    return xlCompileCSourceFilesToExternalOutput(ctx, paths, pathCount, outputDll, TCC_OUTPUT_DLL, "cannot output C shared library");
}

XL_API int xlCompileCSourceFileToSharedLibrary(XL_Context* ctx, const char* path, const char* outputDll)
{
    const char* paths[1];

    paths[0] = path;
    return xlCompileCSourceFilesToSharedLibrary(ctx, paths, 1, outputDll);
}

#endif

#if XL_ENABLE_EXE_COMPILER
static void xlcClearUnitUnlocked(XLC_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    if ( ctx->unitReady ) {
        xl6CompileUnitFree(&ctx->unit);
    }
    memset(&ctx->unit, 0, sizeof(ctx->unit));
    ctx->unitReady = 0;
}

static void xlcClearSourceUnlocked(XLC_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xlcClearUnitUnlocked(ctx);
    xrtFree(ctx->sourcePath);
    xrtFree(ctx->sourceText);
    xrtFree(ctx->virtualPath);
    ctx->sourcePath = NULL;
    ctx->sourceText = NULL;
    ctx->virtualPath = NULL;
    ctx->sourceKind = XLC_SOURCE_NONE;
}

static int xlcCompileUnlocked(XLC_Context* ctx)
{
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    if ( ctx->unitReady ) {
        return 1;
    }
    if ( ctx->sourceKind == XLC_SOURCE_FILE ) {
        ok = xlLoadUnitFromFile(ctx->runtime, &ctx->unit, ctx->sourcePath);
    } else if ( ctx->sourceKind == XLC_SOURCE_STRING ) {
        ok = xlLoadUnitFromSource(ctx->runtime, &ctx->unit, ctx->sourceText, ctx->virtualPath);
    } else {
        xlSetError(ctx->runtime, "compiler source is empty");
        return 0;
    }
    if ( !ok ) {
        xl6CompileUnitFree(&ctx->unit);
        memset(&ctx->unit, 0, sizeof(ctx->unit));
        return 0;
    }
    ctx->unitReady = 1;
    return 1;
}

XL_API XLC_ContextOptions xlcDefaultContextOptions(void)
{
    return xlDefaultContextOptions();
}

XL_API XLC_Context* xlcCreateContext(const XLC_ContextOptions* options)
{
    XL_Context* runtime;
    XLC_Context* ctx;

    runtime = xlCreateContext(options);
    if ( runtime == NULL ) {
        return NULL;
    }
    ctx = (XLC_Context*)xrtCalloc(1, sizeof(XLC_Context));
    if ( ctx == NULL ) {
        xlDestroyContext(runtime);
        return NULL;
    }
    ctx->runtime = runtime;
    return ctx;
}

XL_API void xlcDestroyContext(XLC_Context* ctx)
{
    XL_Context* runtime;

    if ( ctx == NULL ) {
        return;
    }
    xlApiLock();
    runtime = ctx->runtime;
    ctx->runtime = NULL;
    xlcClearSourceUnlocked(ctx);
    xlStringListFree(&ctx->cSourceFiles);
    xrtFree(ctx);
    xlApiUnlock();
    xlDestroyContext(runtime);
}

XL_API int xlcSetWorkDir(XLC_Context* ctx, const char* path)
{
    return ctx != NULL ? xlSetWorkDir(ctx->runtime, path) : 0;
}

XL_API int xlcSetTempDir(XLC_Context* ctx, const char* path)
{
    return ctx != NULL ? xlSetTempDir(ctx->runtime, path) : 0;
}

XL_API int xlcSetSaveTemps(XLC_Context* ctx, int enabled)
{
    return ctx != NULL ? xlSetSaveTemps(ctx->runtime, enabled) : 0;
}

XL_API int xlcAddIncludePath(XLC_Context* ctx, const char* path)
{
    return ctx != NULL ? xlAddIncludePath(ctx->runtime, path) : 0;
}

XL_API int xlcAddLibraryPath(XLC_Context* ctx, const char* path)
{
    return ctx != NULL ? xlAddLibraryPath(ctx->runtime, path) : 0;
}

XL_API int xlcAddLinkLibrary(XLC_Context* ctx, const char* nameOrPath)
{
    return ctx != NULL ? xlAddLinkLibrary(ctx->runtime, nameOrPath) : 0;
}

XL_API int xlcAddCSourceFile(XLC_Context* ctx, const char* path)
{
    char* snapshot;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx->runtime, "C source file path is empty");
        xlApiUnlock();
        return 0;
    }
    if ( ctx->cSourceFiles.count >= XLC_MAX_C_SOURCE_FILES ) {
        xlSetError(ctx->runtime, "too many C source files");
        xlApiUnlock();
        return 0;
    }
    snapshot = xlPathSnapshotFromCurrentDir(ctx->runtime, path, "cannot snapshot C source file");
    ok = snapshot != NULL && xlStringListAppend(&ctx->cSourceFiles, snapshot);
    xrtFree(snapshot);
    if ( !ok && ctx->runtime->lastError == NULL ) {
        xlSetError(ctx->runtime, "cannot add C source file");
    }
    xlApiUnlock();
    return ok;
}

XL_API int xlcLoadDependencyLibrary(XLC_Context* ctx, const char* path)
{
    return ctx != NULL ? xlLoadDependencyLibrary(ctx->runtime, path) : 0;
}

XL_API int xlcRegisterCFunction(XLC_Context* ctx, const char* symbolName, void* fn)
{
    return ctx != NULL ? xlRegisterCFunction(ctx->runtime, symbolName, fn) : 0;
}

XL_API int xlcAddSourceFile(XLC_Context* ctx, const char* path)
{
    char* copy;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx->runtime, "compiler source file path is empty");
        xlApiUnlock();
        return 0;
    }
    copy = xlDupText(path);
    if ( copy == NULL ) {
        xlSetError(ctx->runtime, "cannot copy compiler source file path");
        xlApiUnlock();
        return 0;
    }
    xlcClearSourceUnlocked(ctx);
    ctx->sourceKind = XLC_SOURCE_FILE;
    ctx->sourcePath = copy;
    xlApiUnlock();
    return 1;
}

XL_API int xlcAddSourceString(XLC_Context* ctx, const char* virtualPath, const char* source)
{
    char* pathCopy;
    char* sourceCopy;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( source == NULL ) {
        xlSetError(ctx->runtime, "compiler source is empty");
        xlApiUnlock();
        return 0;
    }
    pathCopy = xlDupText(virtualPath != NULL && virtualPath[0] != '\0' ? virtualPath : "__xlc_string.xl");
    sourceCopy = xlDupText(source);
    if ( pathCopy == NULL || sourceCopy == NULL ) {
        xrtFree(pathCopy);
        xrtFree(sourceCopy);
        xlSetError(ctx->runtime, "cannot copy compiler source");
        xlApiUnlock();
        return 0;
    }
    xlcClearSourceUnlocked(ctx);
    ctx->sourceKind = XLC_SOURCE_STRING;
    ctx->virtualPath = pathCopy;
    ctx->sourceText = sourceCopy;
    xlApiUnlock();
    return 1;
}

XL_API int xlcCompile(XLC_Context* ctx)
{
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcCheck(XLC_Context* ctx, const char* outputPath, int jsonOutput)
{
    XL_CwdScope cwd;
    XL6_CompileUnit unit;
    XL6_Output out;
    int success;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    xl6CompileUnitInit(&unit);
    if ( ctx->sourceKind == XLC_SOURCE_FILE ) {
        success = xlLoadUnitFromFile(ctx->runtime, &unit, ctx->sourcePath);
    } else if ( ctx->sourceKind == XLC_SOURCE_STRING ) {
        success = xlLoadUnitFromSource(ctx->runtime, &unit, ctx->sourceText, ctx->virtualPath);
    } else {
        xlSetError(ctx->runtime, "compiler source is empty");
        xlLeaveWorkDir(&cwd);
        xlApiUnlock();
        return 0;
    }
    ok = xlcOutputOpen(ctx->runtime, &out, outputPath, "cannot open check output");
    if ( ok ) {
        if ( jsonOutput ) {
            xl6DiagPrintJson(&out, &unit.parse.diags, xlcSourceLabel(ctx), unit.phase, success);
        } else {
            xlcEmitDiagText(&out, &unit.parse.diags, xlcSourceLabel(ctx), unit.phase, success);
        }
        ok = xlcOutputClose(ctx->runtime, &out, "cannot write check output");
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok && success;
}

XL_API int xlcEmitTokens(XLC_Context* ctx, const char* outputPath, int jsonOutput)
{
    XL_CwdScope cwd;
    XL6_Output out;
    int rc;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( ctx->sourceKind != XLC_SOURCE_FILE ) {
        xlSetError(ctx->runtime, "token dump currently requires source file input");
        xlApiUnlock();
        return 0;
    }
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcOutputOpen(ctx->runtime, &out, outputPath, "cannot open token output");
    rc = ok ? xl6LexerDumpFileTo(&out, ctx->sourcePath, jsonOutput) : 1;
    if ( ok ) {
        ok = xlcOutputClose(ctx->runtime, &out, "cannot write token output");
    }
    if ( rc != 0 && ctx->runtime->lastError == NULL ) {
        xlSetError2(ctx->runtime, "cannot dump tokens", ctx->sourcePath);
    }
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok && rc == 0;
}

XL_API int xlcEmitAst(XLC_Context* ctx, const char* outputPath, int jsonOutput)
{
    XL_CwdScope cwd;
    XL6_ParseResult result;
    XL6_Output out;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcParseSourceUnlocked(ctx, &result);
    if ( ok ) {
        ok = xlcOutputOpen(ctx->runtime, &out, outputPath, "cannot open AST output");
        if ( ok ) {
            if ( jsonOutput ) {
                xl6AstDumpJson(&out, result.root);
            } else {
                xl6AstDumpTextFile(&out, result.root, 0);
            }
            ok = xlcOutputClose(ctx->runtime, &out, "cannot write AST output");
        }
    }
    xl6ParseResultFree(&result);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitIr(XLC_Context* ctx, const char* outputPath, int jsonOutput)
{
    XL_CwdScope cwd;
    XL6_Output out;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx) && xlcOutputOpen(ctx->runtime, &out, outputPath, "cannot open IR output");
    if ( ok ) {
        if ( jsonOutput ) {
            xl6IrDumpJson(&out, ctx->unit.parse.root);
        } else {
            xl6IrDumpText(&out, ctx->unit.parse.root);
        }
        ok = xlcOutputClose(ctx->runtime, &out, "cannot write IR output");
    }
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitC(XLC_Context* ctx, const char* outputPath)
{
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx) && xlEmitUnitCToFile(ctx->runtime, &ctx->unit, outputPath);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitDebugC(XLC_Context* ctx, const char* outputPath, const char* debugSourcePath)
{
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx) && xlEmitUnitDebugCToFile(ctx->runtime, &ctx->unit, outputPath, debugSourcePath);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitHeader(XLC_Context* ctx, const char* outputPath, const char* guardName)
{
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx) && xlEmitUnitHeaderToFile(ctx->runtime, &ctx->unit, outputPath, guardName);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitAbi(XLC_Context* ctx, const char* outputPath, int jsonOutput)
{
    XL_CwdScope cwd;
    XL6_Output out;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx) && xlcOutputOpen(ctx->runtime, &out, outputPath, "cannot open ABI output");
    if ( ok ) {
        if ( jsonOutput ) {
            xl6AbiDumpJson(&out, ctx->unit.parse.root);
        } else {
            xl6AbiDumpText(&out, ctx->unit.parse.root);
        }
        ok = xlcOutputClose(ctx->runtime, &out, "cannot write ABI output");
    }
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitExe(XLC_Context* ctx, const char* outputExe)
{
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx) &&
         xlCompileUnitToExternalOutputWithC(ctx->runtime, &ctx->unit, outputExe, TCC_OUTPUT_EXE, "exe", NULL, &ctx->cSourceFiles);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitDebugExe(XLC_Context* ctx, const char* outputExe, const char* debugSourcePath)
{
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    if ( xlcCompileUnlocked(ctx) ) {
        XL6_CgenOptions options;
        memset(&options, 0, sizeof(options));
        options.debugMode = 1;
        options.debugSourcePath = debugSourcePath;
        ok = xlCompileUnitToExternalOutputWithC(ctx->runtime, &ctx->unit, outputExe, TCC_OUTPUT_EXE, "debug exe", &options, &ctx->cSourceFiles);
    } else {
        ok = 0;
    }
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitSharedLibrary(XLC_Context* ctx, const char* outputDll)
{
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    ok = xlcCompileUnlocked(ctx) && xlCompileUnitToSharedLibraryWithC(ctx->runtime, &ctx->unit, outputDll, &ctx->cSourceFiles);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API XL_Module* xlcRelocateModule(XLC_Context* ctx)
{
    XL_CwdScope cwd;
    XL_Module* module;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return NULL;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return NULL;
    }
    module = xlcCompileUnlocked(ctx) ? xlCompileUnitToModuleWithC(ctx->runtime, &ctx->unit, NULL, &ctx->cSourceFiles) : NULL;
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return module;
}

XL_API int xlcRun(XLC_Context* ctx, int argc, char** argv)
{
    XL_CwdScope cwd;
    XL6_CgenOptions cgenOptions;
    XL_Module* module;
    xvalue arg;
    int ok;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlApiUnlock();
        return 0;
    }
    memset(&cgenOptions, 0, sizeof(cgenOptions));
    cgenOptions.suppressMainWrapper = 1;
    arg = xlcCreateArgArray(ctx->runtime, argc > 0 ? argc : 0, argv);
    if ( arg == NULL ) {
        xlLeaveWorkDir(&cwd);
        xlApiUnlock();
        return 0;
    }
    module = xlcCompileUnlocked(ctx) ? xlCompileUnitToModuleWithC(ctx->runtime, &ctx->unit, &cgenOptions, &ctx->cSourceFiles) : NULL;
    ok = module != NULL && xlRunModuleMainUnlocked(ctx->runtime, module, arg);
    xlDestroyModuleUnlocked(module);
    xvoUnref(arg);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API XL_Context* xlcRuntimeContext(XLC_Context* ctx)
{
    return ctx != NULL ? ctx->runtime : NULL;
}

XL_API const char* xlcGetLastError(XLC_Context* ctx)
{
    return ctx != NULL ? xlGetLastError(ctx->runtime) : "";
}

XL_API void xlcClearLastError(XLC_Context* ctx)
{
    if ( ctx != NULL ) {
        xlClearLastError(ctx->runtime);
    }
}

XL_API int xlcGetLastExitCode(XLC_Context* ctx)
{
    return ctx != NULL ? xlGetLastExitCode(ctx->runtime) : 0;
}
#endif

const char* xlGetLastError(XL_Context* ctx)
{
    if ( ctx == NULL || ctx->lastError == NULL ) {
        return "";
    }
    return ctx->lastError;
}

void xlClearLastError(XL_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    xlApiUnlock();
}

int xlGetLastExitCode(XL_Context* ctx)
{
    return ctx != NULL ? ctx->lastExitCode : 0;
}
