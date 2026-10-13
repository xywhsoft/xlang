#include "include/xl.h"

#include "src/common/xl6_xrt.h"
#include "src/runtime/xl6_list.h"
#include "src/runtime/xl6_set.h"
#include "src/runtime/xl6_dict.h"
#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_array_native_value_admission.h"
#include "libtcc.h"
#include "src/api/xl_api_internal.h"
#include "src/api/xl_api_path.h"
#include "src/api/xl_foreign_error.h"
#include "src/runtime/xl6_code_owner.h"
#include "src/runtime/xl6_closure.h"
#include "src/runtime/xl6_cell.h"
#include "src/runtime/xl6_stack.h"
#include "src/runtime/xl6_sequence.h"
#include "src/runtime/xl6_tree.h"
#include "src/runtime/xl6_semaphore.h"
#include "src/runtime/xl6_semaphore_admission.h"
#include "src/runtime/xl6_sync.h"
#include "src/runtime/xl6_sync_admission.h"
#include "src/runtime/xl6_thread.h"
#include "src/runtime/xl6_thread_admission.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_concurrent_queue.h"
#include "src/runtime/xl6_channel.h"
#include "src/runtime/xl6_wait_queue.h"
#include "src/runtime/xl6_value_iterator.h"
#include "src/runtime/xl6_generator.h"
#include "src/runtime/xl6_ownership_plan.h"
#include "src/runtime/xl6_ownership_read.h"
#include "src/api/xl_module_state.h"
#include "src/api/xl_api_lock.h"
#include "src/runtime/xl6_native_handle.h"
#include "src/runtime/xl6_dynamic_fields.h"
#include "src/runtime/xl6_error_value.h"
#include "src/runtime/xl6_future_payload.h"
#include "src/runtime/xl6_future_callback.h"
#include "src/runtime/xl6_future_aggregate.h"
#include "src/runtime/xl6_task.h"
#include "src/runtime/xl6_continuation_future.h"
#include "src/runtime/xl6_continuation_executor.h"
#include "src/abi/xl6_abi_dump.h"
#include "src/ast/xl6_ast_dump.h"
#include "src/backend/xl6_toolchain.h"
#include "src/cgen/xl6_ir_cgen.h"
#include "src/common/xl6_common.h"
#include "src/common/xl6_console.h"
#include "src/compile/xl6_compile_unit.h"
#include "src/compile/xl6_artifact_runtime.h"
#include "src/compile/xl6_xrt_trim.h"
#include "src/diag/xl6_diag.h"
#include "src/ir/xl6_ir_dump.h"
#include "src/lexer/xl6_lexer_api.h"
#include "src/resource/xl6_win_resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int (*XL_XlangMainProc)(xvalue* pArg);
/* Checked artifact ABI keeps language failure separate from process status
 * and from optional diagnostics. */
typedef int (*XL_ScriptMainCheckedProc)(xvalue* pArg, int* exitCode);
typedef int (*XL_ScriptUnitCheckedProc)(void);
typedef int (*XL_CMainProc)(int argc, char** argv);
typedef int (*XL_ScriptHasErrorProc)(void);
typedef int (*XL_ScriptModuleInitProc)(xvalue* pArg);
typedef void (*XL_ScriptVoidProc)(void);

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
    XL_StringList resourceFiles;
    XL_CSymbol* symbols;
    size_t symbolCount;
    XL_LoadedLibrary* loadedLibraries;
    size_t loadedLibraryCount;
    unsigned int activeCalls;
    int destroyRequested;
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
    unsigned int activeCalls;
    int destroyRequested;
} XLC_Context;

XL_API XLC_ContextOptions xlcDefaultContextOptions(void);
XL_API XLC_Context* xlcCreateContext(const XLC_ContextOptions* options);
XL_API void xlcDestroyContext(XLC_Context* ctx);
XL_API int xlcSetWorkDir(XLC_Context* ctx, const char* path);
XL_API int xlcSetTempDir(XLC_Context* ctx, const char* path);
XL_API int xlcSetSaveTemps(XLC_Context* ctx, int enabled);
XL_API int xlcSetBackend(XLC_Context* ctx, XLC_Backend backend);
XL_API int xlcAddIncludePath(XLC_Context* ctx, const char* path);
XL_API int xlcAddLibraryPath(XLC_Context* ctx, const char* path);
XL_API int xlcAddLinkLibrary(XLC_Context* ctx, const char* nameOrPath);
XL_API int xlcAddResourceFile(XLC_Context* ctx, const char* path);
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
XL_API int xlcEmitLoweredIr(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XL_API int xlcEmitC(XLC_Context* ctx, const char* outputPath);
XL_API int xlcEmitXlirC(XLC_Context* ctx, const char* outputPath);
XL_API int xlcEmitXlirHeader(XLC_Context* ctx, const char* outputPath, const char* guardName);
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
    XL_Context* context;
    char* oldDir;
} XL_CwdScope;

XL_TccApi xl__tcc;
static int xl__runtimeManualCount = 0;
static int xl__runtimeContextCount = 0;

#define XL_TCC_VFS_LIBTCC1 "lib/libtcc1.a"
#define XL_TCC_VFS_MODULE_C "__xlvfs__/module.c"
#define XL_TCC_VFS_C_MODULE_C "__xlvfs__/c_module.c"
#define XL_TCC_VFS_RUNTIME_C "__xlvfs__/runtime.c"
#define XL_TCC_VFS_XRT_C "__xlvfs__/xl6_xrt_impl.c"
#define XL_TCC_VFS_COMPILER_SOURCE_ROOT "__xl_compiler"
/* 官方大型 C 库通常包含上百个独立编译单元，上限只负责阻止异常输入。 */
#ifndef XLC_MAX_C_SOURCE_FILES
#define XLC_MAX_C_SOURCE_FILES 256
#endif
#define XLC_MAX_RESOURCE_FILES 32

/* DLL API 会修改全局 TCC 状态和进程 cwd，公开入口必须串行化。 */
#ifdef _WIN32

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
    text = xrtUtf16To8((const uint16*)wide, NULL);
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
    scope->context = ctx;
    scope->oldDir = xrtPathCwd();
    if ( scope->oldDir == NULL || !xrtPathSetCwd(workDir) ) {
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

static void xlModuleLoadedLibrariesFree(XL_Module* module)
{
    size_t i;

    if ( module == NULL ) {
        return;
    }
    for ( i = 0; i < module->loadedLibraryCount; ++i ) {
        xlCloseLoadedLibrary(&module->loadedLibraries[i]);
    }
    xrtFree(module->loadedLibraries);
    module->loadedLibraries = NULL;
    module->loadedLibraryCount = 0;
}

/*
    已重定位模块可能直接调用上下文加载的动态库。模块必须单独保留加载引用，
    否则先销毁上下文会让模块中的函数地址悬空。
*/
static int xlModuleRetainLoadedLibraries(XL_Module* module, XL_Context* ctx)
{
    size_t i;

    if ( module == NULL || ctx == NULL || ctx->loadedLibraryCount == 0 ) {
        return 1;
    }
    module->loadedLibraries = (XL_LoadedLibrary*)xrtCalloc(
        ctx->loadedLibraryCount,
        sizeof(XL_LoadedLibrary));
    if ( module->loadedLibraries == NULL ) {
        xlSetError(ctx, "cannot retain module dependency libraries");
        return 0;
    }
    for ( i = 0; i < ctx->loadedLibraryCount; ++i ) {
        XL_LoadedLibrary* dst = &module->loadedLibraries[i];
        const char* path = ctx->loadedLibraries[i].path;

        dst->path = xlDupText(path);
        if ( dst->path == NULL ) {
            xlSetError(ctx, "cannot retain module dependency path");
            xlModuleLoadedLibrariesFree(module);
            return 0;
        }
        module->loadedLibraryCount += 1;
#ifdef _WIN32
        {
            uint16* pathW = xrtUtf8To16(path, NULL);
            if ( pathW != NULL ) {
                dst->module = LoadLibraryW((LPCWSTR)pathW);
                xrtFree(pathW);
            }
        }
#else
        dst->module = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
#endif
        if ( dst->module == NULL ) {
            xlSetError2(ctx, "cannot retain module dependency library", path);
            xlModuleLoadedLibrariesFree(module);
            return 0;
        }
    }
    return 1;
}

static void xlLeaveWorkDir(XL_CwdScope* scope)
{
    if ( scope == NULL ) {
        return;
    }
    if ( scope->active ) {
        if ( scope->oldDir != NULL ) {
            if ( !xrtPathSetCwd(scope->oldDir) && scope->context != NULL ) {
                xlSetError2(
                    scope->context,
                    "cannot restore work dir",
                    scope->oldDir
                );
            }
            xrtFree(scope->oldDir);
        }
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
        uint16* pathW = xrtUtf8To16(path, NULL);
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
#ifdef _WIN32
    if ( xlLoadTccFromPath(ctx, "libtcc.dll") ) {
        return 1;
    }
    if ( xlLoadTccFromBasePath(ctx, "release\\libtcc.dll") ) {
        return 1;
    }
#else
    if ( xlLoadTccFromPath(ctx, "libtcc.so") ) {
        return 1;
    }
    if ( xlLoadTccFromBasePath(ctx, "release/libtcc.so") ) {
        return 1;
    }
#endif
    if ( xlLoadTccFromBasePath(ctx, XL6_TCC_DLL_PATH) ) {
        return 1;
    }
#ifdef _WIN32
    xlSetError(ctx, "cannot load libtcc.dll");
#else
    xlSetError(ctx, "cannot load libtcc.so");
#endif
    return 0;
}
#endif

static int xlAddContextSymbols(XL_Context* ctx, TCCState* s)
{
    size_t i;

    if ( !xlApiAddDefaultRuntimeSymbols(s) ) {
        xlSetError(ctx, "cannot add XRT symbols to TCC state");
        return 0;
    }
    for ( i = 0; ctx != NULL && i < ctx->symbolCount; ++i ) {
        if ( !xlApiAddTccSymbol(s, ctx->symbols[i].name, ctx->symbols[i].value) ) {
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
    if ( xlJoinPathBuffer(path, sizeof(path), root, "lib/xrt") ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
    if ( xlJoinPathBuffer(path, sizeof(path), root, "lib/xruntime") ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
#ifdef _WIN32
    if ( xlJoinPathBuffer(path, sizeof(path), root, "release\\tcc\\include_win") ) {
        (void)xl__tcc.tcc_add_include_path(s, path);
    }
#endif
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
#ifdef XL_STATIC_EMBED
    (void)xl__tcc.tcc_set_options(s, "-DXL_STATIC_EMBED");
#endif
    (void)xl__tcc.tcc_add_include_path(s, ctx != NULL && ctx->baseDir != NULL ? ctx->baseDir : ".");
    xlAddDefaultProjectToolchainPaths(s);
    (void)xl__tcc.tcc_add_include_path(s, "src");
    (void)xl__tcc.tcc_add_include_path(s, "lib/xrt");
    (void)xl__tcc.tcc_add_include_path(s, "lib/xruntime");
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
    if ( outputType == TCC_OUTPUT_DLL ) {
        xl__tcc.tcc_define_symbol(s, "XL6_BUILD_SHARED", "1");
    }
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

static int xlEnsureUnitXlirLowered(XL_Context* ctx, XL6_CompileUnit* unit)
{
    if ( unit == NULL || !unit->irReady ) {
        xlSetError(ctx, "verified XLIR is unavailable");
        return 0;
    }
    if ( unit->ir.stage == XL6_IR_STAGE_TYPED ) {
        if ( !xl6IrLowerModule(&unit->ir, &unit->parse.diags) ) {
            unit->irReady = 0;
            xlSetCompileDiagError(ctx, unit);
            return 0;
        }
    }
    if ( unit->ir.stage != XL6_IR_STAGE_LOWERED ||
         !xl6IrVerifyModule(&unit->ir, &unit->parse.diags) ) {
        unit->irReady = 0;
        xlSetCompileDiagError(ctx, unit);
        return 0;
    }
    return 1;
}

static int xlCgenToMemory(XL_Context* ctx, XL6_CompileUnit* unit,
                          char** outText,
                          const XL6_IrCgenOptions* requestedOptions)
{
    XL6_IrCgenOptions options = {0};
    XL6_Output out;
    int ok;

    if ( outText == NULL ) return 0;
    *outText = NULL;
    if ( !xlEnsureUnitXlirLowered(ctx, unit) ||
         !xl6IrCgenVerifyEmittable(&unit->ir, &unit->parse.diags) ) {
        if ( ctx != NULL && ctx->lastError == NULL )
            xlSetCompileDiagError(ctx, unit);
        return 0;
    }
    if ( !xl6OutputOpenMemory(&out) ) {
        xlSetError(ctx, "cannot create XLIR Cgen memory output");
        return 0;
    }
    if ( requestedOptions != NULL ) options = *requestedOptions;
    ok = xl6IrCgenEmitCEx(&out, &unit->ir, &unit->parse.diags, &options);
    if ( ok && !xl6OutputHasFailed(&out) )
        *outText = xl6OutputTakeMemoryText(&out, NULL);
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot generate XLIR C code", xl6OutputError(&out));
        ok = 0;
    } else if ( !ok ) {
        xlSetCompileDiagError(ctx, unit);
    } else if ( *outText == NULL ) {
        xlSetError(ctx, "cannot copy generated XLIR C code");
        ok = 0;
    }
    xl6OutputClose(&out);
    return ok && *outText != NULL;
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

static int xlTccCompileGeneratedModule(XL_Context* ctx, TCCState* s, const char* source)
{
    int ok;
    if ( s == NULL || source == NULL ) {
        xlSetError(ctx, "generated C module is empty");
        return 0;
    }
    /* Lowered XLIR owns every native signature. Apply the same ABI gate to
     * JIT and external outputs, but not to user/vendor C translation units. */
    if ( xl__tcc.tcc_set_options(s, "-Werror=implicit-function-declaration") < 0 ) {
        xlSetError(ctx, "cannot enforce generated C declaration ABI");
        return 0;
    }
    ok = xlTccCompileSourceText(ctx, s, XL_TCC_VFS_MODULE_C, source, "TCC compile failed");
    if ( xl__tcc.tcc_set_options(s, "-Wno-error=implicit-function-declaration") < 0 ) {
        if ( ctx != NULL && ctx->lastError == NULL )
            xlSetError(ctx, "cannot restore C declaration diagnostic policy");
        return 0;
    }
    return ok;
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
static int xlEmitUnitHeaderToFile(XL_Context* ctx, XL6_CompileUnit* unit,
                                  const char* outputPath, const char* guardName);
#endif

static char* xlJoinTempFilePath(XL_Context* ctx, const char* fileName)
{
    str path;

    if ( ctx == NULL || ctx->tempDir == NULL || ctx->tempDir[0] == '\0' ) {
        xlSetError(ctx, "compiler temp dir is empty");
        return NULL;
    }
    path = xrtPathJoin(ctx->tempDir, fileName);
    if ( path == NULL ) {
        xlSetError2(ctx, "cannot create temp path", fileName);
    }
    return (char*)path;
}

static int xlWriteTempTextFile(XL_Context* ctx, const char* path, const char* text)
{
    xbytesview data;
    size_t size;

    if ( path == NULL || path[0] == '\0' || text == NULL ) {
        xlSetError(ctx, "temp file write input is empty");
        return 0;
    }
    size = strlen(text);
    data.Data = (const unsigned char*)text;
    data.Size = size;
    if ( !xrtFileWriteAll(path, data) ) {
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

static int xlEmitUnitHeaderToMemory(XL_Context* ctx, XL6_CompileUnit* unit,
                                    const char* guardName, char** outText)
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
    if ( !xlEnsureUnitXlirLowered(ctx, unit) )
        return 0;
    if ( !xl6OutputOpenMemory(&out) ) {
        xlSetError(ctx, "cannot create header memory output");
        return 0;
    }
    ok = xl6IrCgenEmitHeader(&out, &unit->ir,
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

static int xlMountDynamicTccBinary(XL_Context* ctx, const char* virtualPath, const void* data, size_t size)
{
#ifdef XL_STATIC_TCC_VFS
    if ( virtualPath == NULL || virtualPath[0] == '\0' || (data == NULL && size != 0) ) {
        xlSetError(ctx, "dynamic TCC VFS binary input is empty");
        return 0;
    }
    if ( !tcc_vfs_mount_memory(virtualPath, data, size) ) {
        xlSetError2(ctx, "cannot mount dynamic TCC VFS binary", virtualPath);
        return 0;
    }
    return 1;
#else
    (void)virtualPath;
    (void)data;
    (void)size;
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

static int xlBuildCSourceWrapperText(XL_Context* ctx, const char* sourcePath,
                                     const char* headerText, char** outText)
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
    /* Native implementations need declarations, not an implementation header
     * whose feature guards were fixed before the sidecar can select modules.
     * Use the same canonical, packaged declaration ABI as generated XLIR C. */
    ok = xl6OutputPuts("#include <__xl_runtime/xruntime_decl.h>\n", &out) != EOF &&
         xl6OutputPuts(headerText, &out) != EOF &&
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

/* All C implementations of language declarations share the same verified ABI
 * header, regardless of whether they came from a package or the public API.
 * TCC owns disk/VFS transport; no second wrapper or ABI inference path exists. */
static int xlCompileUnitCSourceFiles(
    XL_Context* ctx, TCCState* s, XL6_CompileUnit* unit,
    const XL_StringList* cSourceFiles)
{
    char* headerText = NULL;
    size_t group, i, ordinal = 0;
    size_t importedCount = unit != NULL ? unit->importDeps.cSources.count : 0;
    size_t additionalCount = cSourceFiles != NULL ? cSourceFiles->count : 0;
    int ok = 0;
    if ( importedCount == 0 && additionalCount == 0 ) return 1;
    if ( ctx == NULL || s == NULL || unit == NULL ) {
        xlSetError(ctx, "compiler context is empty");
        return 0;
    }
    if ( additionalCount > XLC_MAX_C_SOURCE_FILES ||
         importedCount > SIZE_MAX - additionalCount ) {
        xlSetError(ctx, "too many C source files");
        return 0;
    }
    if ( !xlEmitUnitHeaderToMemory(ctx, unit, "XL6_MODULE_H", &headerText) )
        return 0;
    if ( !xlSaveTempTextByName(ctx, "module.h", headerText) ) goto cleanup;
    for ( group = 0; group < 2; ++group ) {
        size_t count = group == 0 ? importedCount : additionalCount;
        for ( i = 0; i < count; ++i, ++ordinal ) {
            const char* path = group == 0 ? unit->importDeps.cSources.items[i] : cSourceFiles->items[i];
            char* wrapperText = NULL;
            char wrapperName[64], virtualPath[96];
            int compiled;
            snprintf(wrapperName, sizeof(wrapperName), "csrc_%zu.c", ordinal);
            snprintf(virtualPath, sizeof(virtualPath), "__xlvfs__/%s", wrapperName);
            if ( !xlBuildCSourceWrapperText(ctx, path, headerText, &wrapperText) )
                goto cleanup;
            compiled = xlSaveTempTextByName(ctx, wrapperName, wrapperText) &&
                 xlTccCompileSourceText(ctx, s, virtualPath, wrapperText, "cannot compile C source file");
            xrtFree(wrapperText);
            if ( !compiled ) goto cleanup;
        }
    }
    ok = 1;
cleanup:
    xrtFree(headerText);
    return ok;
}

#if XL_ENABLE_EXE_COMPILER
static int xlAddResourceFilesToTcc(XL_Context* ctx, TCCState* s)
{
    if ( ctx == NULL || ctx->resourceFiles.count == 0 ) {
        return 1;
    }
    if ( s == NULL ) {
        xlSetError(ctx, "TCC state is empty");
        return 0;
    }
#if defined(_WIN32)
    size_t i;
    if ( ctx->resourceFiles.count > XLC_MAX_RESOURCE_FILES ) {
        xlSetError(ctx, "too many resource config files");
        return 0;
    }
    for ( i = 0; i < ctx->resourceFiles.count; ++i ) {
        void* objectData;
        size_t objectSize;
        char virtualPath[96];
        char error[512];

        objectData = NULL;
        objectSize = 0;
        error[0] = '\0';
        if ( !xl6WinResourceConfigToCoffMemory(ctx->resourceFiles.items[i], &objectData, &objectSize, error, sizeof(error)) ) {
            xlSetError2(ctx, "cannot build Windows resource", error[0] != '\0' ? error : ctx->resourceFiles.items[i]);
            xrtFree(objectData);
            return 0;
        }
        snprintf(virtualPath, sizeof(virtualPath), "__xlvfs__/resource_%u.obj", (unsigned int)i);
        if ( !xlMountDynamicTccBinary(ctx, virtualPath, objectData, objectSize) ) {
            xrtFree(objectData);
            return 0;
        }
        xrtFree(objectData);
        if ( xl__tcc.tcc_add_file(s, virtualPath) != 0 ) {
            xlSetError2(ctx, "cannot add Windows resource object", virtualPath);
            return 0;
        }
    }
    return 1;
#else
    xlSetError(ctx, "windows resources are only supported on Windows");
    return 0;
#endif
}

static void xlAddEmbeddedSourceIncludePaths(TCCState* s)
{
    if ( s == NULL ) {
        return;
    }
    (void)xl__tcc.tcc_add_include_path(s, XL_TCC_VFS_COMPILER_SOURCE_ROOT);
    (void)xl__tcc.tcc_add_include_path(s, XL_TCC_VFS_COMPILER_SOURCE_ROOT "/src");
    (void)xl__tcc.tcc_add_include_path(s, XL_TCC_VFS_COMPILER_SOURCE_ROOT "/lib/tcc");
    (void)xl__tcc.tcc_add_include_path(s, XL_TCC_VFS_COMPILER_SOURCE_ROOT "/lib/lzma");
    (void)xl__tcc.tcc_add_include_path(s, XL_TCC_VFS_COMPILER_SOURCE_ROOT "/release");
    /* Filesystem SDKs use the same reserved package root as the VFS. Resolve
     * it only beside the compiler module, never from user include directories
     * or source spelling. tcc_add_file does not search its include paths. */
    char root[512], package[1536], path[2048];
    if (xlGetProjectRoot(root,sizeof(root)) &&
        xlJoinPathBuffer(package,sizeof(package),root,"include/" XL_TCC_VFS_COMPILER_SOURCE_ROOT)) {
        const char* suffixes[]={"","src","lib/tcc","lib/lzma","release"};
        for(size_t i=0;i<sizeof(suffixes)/sizeof(*suffixes);++i)
            if (xlJoinPathBuffer(path,sizeof(path),package,suffixes[i]))
                (void)xl__tcc.tcc_add_include_path(s,path);
    }
}

static int xlEmbeddedSourcePath(char* out, size_t outSize, const char* relativePath)
{
    char root[512];
    char externalPath[1536];
    size_t i;
    int written;

    if ( out == NULL || outSize == 0 || relativePath == NULL || relativePath[0] == '\0' ) {
        return 0;
    }

    /* 开发目录存在源码时优先使用外部最新版；发布环境回落到只读静态 VFS。 */
    if ( xlGetProjectRoot(root, sizeof(root)) &&
         xlJoinPathBuffer(externalPath, sizeof(externalPath), root, relativePath) &&
         xrtFileExists((str)externalPath) ) {
        return xlCopyToBuffer(out, outSize, externalPath);
    }

    char package[1536];
    if (xlGetProjectRoot(root,sizeof(root)) &&
        xlJoinPathBuffer(package,sizeof(package),root,"include/" XL_TCC_VFS_COMPILER_SOURCE_ROOT) &&
        xlJoinPathBuffer(externalPath,sizeof(externalPath),package,relativePath) &&
        xrtFileExists((str)externalPath))
        return xlCopyToBuffer(out,outSize,externalPath);

    written = snprintf(out, outSize, "%s/%s", XL_TCC_VFS_COMPILER_SOURCE_ROOT, relativePath);
    if ( written <= 0 || (size_t)written >= outSize ) {
        return 0;
    }
    for ( i = 0; out[i] != '\0'; ++i ) {
        if ( out[i] == '\\' ) {
            out[i] = '/';
        }
    }
    return 1;
}

static int xlAddTccSourceFile(XL_Context* ctx, TCCState* s, const char* path)
{
    char resolvedPath[2048];

    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "embedded TCC source path is empty");
        return 0;
    }
    if ( !xlEmbeddedSourcePath(resolvedPath, sizeof(resolvedPath), path) ) {
        xlSetError2(ctx, "cannot resolve embedded source", path);
        return 0;
    }
    if ( xl__tcc.tcc_add_file(s, resolvedPath) != 0 ) {
        if ( ctx->lastError == NULL ) {
            xlSetError2(ctx, "cannot compile embedded TCC source", path);
        }
        return 0;
    }
    return 1;
}



/* XLIR output consumes the complete precomputed artifact contract. No source
 * text, AST spelling or missing-symbol fallback is consulted at this boundary. */
static int xlAddPrivateLanguageRuntimeSources(XL_Context* ctx, TCCState* s,
    const XL6_CompileUnit* unit, const XL6_ArtifactRuntimePlan* plan)
{
    if (!ctx || !s || !unit || !plan) return 0;
    if (unit->importDeps.features &
        (XL6_IMPORT_FEATURE_XLANG_RUNTIME | XL6_IMPORT_FEATURE_XLANG_COMPILER)) return 1;
    (void)xl__tcc.tcc_set_options(s, "-DXL_STATIC_EMBED");
    (void)xl__tcc.tcc_add_include_path(s, ".");
    (void)xl__tcc.tcc_add_include_path(s, "src");
    (void)xl__tcc.tcc_add_include_path(s, "release");
    (void)xl__tcc.tcc_add_include_path(s, "release/include");
    xlAddEmbeddedSourceIncludePaths(s);
    for (size_t i=0;i<plan->sourceCount;++i)
        if (!xlAddTccSourceFile(ctx,s,plan->sources[i])) return 0;
    return 1;
}

static int xlAddEmbeddedTccRuntimeSources(
    XL_Context* ctx,
    TCCState* s,
    const XL6_CompileUnit* unit,
    int outputType)
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
#ifdef _WIN32
    (void)xl__tcc.tcc_set_options(s, "-DXL_STATIC_TCC -DCONFIG_TCC_STATIC");
#else
    /* Static inclusion of libtcc is not CONFIG_TCC_STATIC: that vendor option
     * replaces POSIX dlopen/dlsym with profiling stubs, disabling nested JIT. */
    (void)xl__tcc.tcc_set_options(s, "-DXL_STATIC_TCC");
#endif
#if !defined(_WIN32) && defined(XL6_TCC_CONFIGURED_TRIPLET)
    /* Preserve both libtcc's configured include/CRT layout and this host's
     * ability to propagate that same contract to a nested embedded host. */
    xl__tcc.tcc_define_symbol(s, "CONFIG_TRIPLET", "\"" XL6_TCC_CONFIGURED_TRIPLET "\"");
    xl__tcc.tcc_define_symbol(s, "XL6_TCC_CONFIGURED_TRIPLET", "\"" XL6_TCC_CONFIGURED_TRIPLET "\"");
#endif
    if ( outputType == TCC_OUTPUT_DLL ) {
        (void)xl__tcc.tcc_set_options(s, "-DLIBTCC_AS_DLL");
    }
    (void)xl__tcc.tcc_add_include_path(s, "lib/tcc");
    (void)xl__tcc.tcc_add_include_path(s, "lib/lzma");
    (void)xl__tcc.tcc_add_include_path(s, "release");
    (void)xl__tcc.tcc_add_include_path(s, "release/include");
    xlAddEmbeddedSourceIncludePaths(s);

	if ( !xlAddTccSourceFile(ctx, s, "lib/tcc/libtcc.c") ||
		 !xlAddTccSourceFile(ctx, s, "lib/tcc/tcctools.c") ||
		 !xlAddTccSourceFile(ctx, s, "lib/tcc/tcc_builtin_vfs.c") ||
		 !xlAddTccSourceFile(ctx, s, "lib/lzma/LzmaDec.c") ||
		 !xlAddTccSourceFile(ctx, s, XL6_TCC_RUNTIME_RESOURCE_SOURCE) ) {
		return 0;
	}
	if ( (features & XL6_IMPORT_FEATURE_TCC_RUNTIME) != 0 &&
		 (features & (XL6_IMPORT_FEATURE_XLANG_RUNTIME |
					  XL6_IMPORT_FEATURE_XLANG_COMPILER)) == 0 &&
		 !xlAddTccSourceFile(ctx, s, "src/api/xl_tcc_standalone_env.c") ) {
		return 0;
	}
#ifdef _WIN32
    if ( !xlAddTccSourceFile(ctx, s, "lib/tcc/tcc_utf8_io.c") ) {
        return 0;
    }
#endif
    return 1;
}

static int xlAddEmbeddedXlangRuntimeSources(
    XL_Context* ctx,
    TCCState* s,
    const XL6_CompileUnit* unit,
    int outputType)
{
    static const char* runtimeSources[] = {
        "src/api/xl_api.c",
        "src/api/xl_api_symbols.c",
        "src/api/xl_api_path.c",
        "src/abi/xl6_abi_json.c",
        "src/abi/xl6_callable.c",
        "src/backend/xl6_build_backend.c",
        "src/compile/xl6_compile_unit.c",
        "src/compile/xl6_imports.c",
        "src/compile/xl6_xrt_trim.c",
        "src/common/xl6_console.c",
        "src/common/xl6_output.c",
        "src/diag/xl6_diag.c",
        "src/ast/xl6_ast.c",
        "src/cgen/xl6_cgen_shared_runtime.c",
        "src/cgen/xl6_ir_cgen.c",
        "src/lexer/xl6_token.c",
        "src/lexer/xl6_lexer_api.c",
        "src/lexer/xl6_lexer_gen.c",
        "src/parser/xl6_parser.c",
        "src/parser/xl6_parser_api.c",
        "src/ir/xl6_ir.c",
        "src/ir/xl6_ir_lower.c",
        "src/package/xl6_package.c",
        "src/stdlib/xl6_stdlib.c",
        "src/stdlib/xl6_stdlib_data.c",
        "src/type/xl6_type_model.c",
        "src/sem/xl6_protocol.c",
        "src/sem/xl6_type_registry.c",
        "src/sem/xl6_sem_docs.c",
        "src/sem/xl6_sem_field_attrs.c",
        "src/sem/xl6_sem.c"
    };
    static const char* compilerSources[] = {
        "src/resource/xl6_win_resource.c",
        "src/abi/xl6_abi_dump.c",
        "src/ast/xl6_ast_dump.c",
        "src/ir/xl6_ir_dump.c"
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
    (void)xl__tcc.tcc_set_options(s,
        "-DXL_STATIC_TCC -DCONFIG_TCC_STATIC -DXL6_COMPILER_SOURCE_VFS");
    if ( outputType == TCC_OUTPUT_DLL ) {
        (void)xl__tcc.tcc_set_options(s, "-DXL_BUILD_DLL -DLIBTCC_AS_DLL");
    } else {
        (void)xl__tcc.tcc_set_options(s, "-DXL_STATIC_EMBED");
    }
    if ( (features & XL6_IMPORT_FEATURE_XLANG_COMPILER) == 0 ) {
        (void)xl__tcc.tcc_set_options(s, "-DXL_RUNTIME_ONLY_DLL -DXL_RUNTIME_ONLY_API");
    }
    (void)xl__tcc.tcc_add_include_path(s, ".");
    (void)xl__tcc.tcc_add_include_path(s, "src");
    (void)xl__tcc.tcc_add_include_path(s, "release");
    (void)xl__tcc.tcc_add_include_path(s, "release/include");
    (void)xl__tcc.tcc_add_include_path(s, "lib/tcc");
    (void)xl__tcc.tcc_add_include_path(s, "lib/lzma");
    xlAddEmbeddedSourceIncludePaths(s);

    for ( i = 0; i < sizeof(runtimeSources) / sizeof(runtimeSources[0]); ++i ) {
        if ( !xlAddTccSourceFile(ctx, s, runtimeSources[i]) ) {
            return 0;
        }
    }
    /* Explicit import xlang embeds a full language host, including the
     * callable standard-library environment registered by xl_api_symbols.
     * Use its canonical resident catalog once instead of a second, stale
     * hand-maintained source list. Ordinary artifacts still consume only
     * their verified, pruned XLIR plan in xlAddPrivateLanguageRuntimeSources. */
    for ( i = 0; i < XL6_ARTIFACT_RUNTIME_COUNT; ++i ) {
        const XL6_ArtifactRuntimeComponent* component =
            xl6ArtifactRuntimeComponentGet((unsigned)i);
        if (component == NULL || component->source == NULL ||
            !xlAddTccSourceFile(ctx, s, component->source)) return 0;
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

static int xlPrepareRuntimeArg(
    XL_Context* ctx,
    xvalue* pSuppliedArg,
    xvalue** pArg
)
{
    if ( pArg == NULL ) {
        return 0;
    }
    *pArg = NULL;
    if ( pSuppliedArg != NULL ) {
        if ( xrtValueType(pSuppliedArg) != XVALUE_ARRAY ) {
            xlSetError(ctx, "script arg must be array");
            return 0;
        }
        *pArg = xrtValueRetain(pSuppliedArg);
        return *pArg != NULL;
    }
    *pArg = xrtValueArray();
    if ( *pArg == NULL ) {
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

    /*
     * TCC 编译和重定位必须串行，但用户 C 入口不能持有全局 API 锁。
     * 上层执行租约会保证 ctx 在重新加锁前始终有效，并允许用户代码
     * 回调 xl.dll、并发请求销毁上下文或运行另一段脚本。
     */
    xlApiUnlock();
    rc = entry(argc > 0 ? argc : 0, argv);
    xlApiLock();
    ctx->lastExitCode = rc;
    (void)returnPrefix;
    return 1;
}

static void xlAddDefaultTccLinkLibraries(TCCState* s)
{
#ifdef _WIN32
    /*
     * XRT 是 xlang 的基础运行库。Windows 版本的 XRT 会使用 socket、
     * IP Helper 和 shell 等系统能力；C99 数学函数需要 UCRT 中超出
     * 旧 msvcrt 的导出。生成外部 exe/dll 和内存运行使用同一默认链接集。
     * 这里不强制检查返回值，让最终链接阶段给出精确诊断。
     */
# ifdef XL_STATIC_TCC
    (void)tcc_add_library(s, "Ws2_32");
    (void)tcc_add_library(s, "IPHLPAPI");
    (void)tcc_add_library(s, "Shell32");
    (void)tcc_add_library(s, "ucrtbase");
# else
    if ( xl__tcc.tcc_add_library != NULL ) {
        (void)xl__tcc.tcc_add_library(s, "Ws2_32");
        (void)xl__tcc.tcc_add_library(s, "IPHLPAPI");
        (void)xl__tcc.tcc_add_library(s, "Shell32");
        (void)xl__tcc.tcc_add_library(s, "ucrtbase");
    }
# endif
#else
    /*
        Linux 版 XRT 依赖 pthread、dl 和 libm。内存运行时这些符号来自
        宿主进程；生成 ELF 产物时由 TCC 按同一规则链接。
    */
# ifdef XL_STATIC_TCC
    (void)tcc_add_library(s, "pthread");
    (void)tcc_add_library(s, "dl");
    (void)tcc_add_library(s, "m");
# else
    if ( xl__tcc.tcc_add_library != NULL ) {
        (void)xl__tcc.tcc_add_library(s, "pthread");
        (void)xl__tcc.tcc_add_library(s, "dl");
        (void)xl__tcc.tcc_add_library(s, "m");
    }
# endif
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
    /* Printing is not an API diagnostic. Capture owned text while the
     * original error/module is alive, before the artifact clears its TLS.
     * Allocation failure must not replace the error being reported. */
    if (ctx != NULL && ctx->lastError == NULL) {
        xerror* original = xrtTakeError();
        xlSetError2(ctx, "uncaught xlang error", original != NULL ? xrtErrorMessage(original) : NULL);
        xrtClearError(); xrtSetErrorTake(original);
    }
    if ( printUncaught != NULL ) {
        printUncaught();
    }
    if ( clearError != NULL ) {
        clearError();
    }
    return 1;
}


/* 纯 C 不经过语言调用图裁剪；声明的能力必须与宿主/独立产物的完整运行库一致。 */
static void xlTccSelectFullRuntime(TCCState* state)
{
    xl__tcc.tcc_define_symbol(state, "XRT_MODULE_ALL", "1");
    xl__tcc.tcc_define_symbol(state, "XRUNTIME_MODULE_ALL", "1");
}

/* 包与显式混编 C 共用能力判定，JIT 声明可见性和 AOT 实现选择保持一致。 */
static int xlTccPrepareNativeSources(
    XL_Context* ctx,
    TCCState* state,
    const XL6_CompileUnit* unit,
    const XL_StringList* cSourceFiles
)
{
    size_t i;

    if ( state == NULL || unit == NULL ) {
        return 0;
    }
    if ( xl6XrtTrimRequiresFull(&unit->importDeps, cSourceFiles != NULL ? cSourceFiles->count : 0) ) {
        xlTccSelectFullRuntime(state);
    } else {
        for ( i = 0; i < unit->importDeps.xrtModules.count; i += 1 ) {
            xl__tcc.tcc_define_symbol(state, unit->importDeps.xrtModules.items[i], "1");
        }
    }
    for ( i = 0; i < unit->importDeps.includeDirs.count; i += 1 ) {
        if ( xl__tcc.tcc_add_include_path(state, unit->importDeps.includeDirs.items[i]) != 0 ) {
            if ( ctx != NULL && ctx->lastError == NULL ) {
                xlSetError2(ctx, "cannot add import include directory",
                    unit->importDeps.includeDirs.items[i]);
            }
            return 0;
        }
    }
    return 1;
}



#if XL_ENABLE_EXE_COMPILER
static int xlCompileUnitHasExecutableEntry(XL_Context* ctx, XL6_CompileUnit* unit);

static int xlCompileUnitToExternalOutputWithC(
    XL_Context* ctx,
    XL6_CompileUnit* unit,
    const char* outputPath,
    int outputType,
    const char* outputKind,
    const XL6_IrCgenOptions* requestedOptions,
    const XL_StringList* cSourceFiles)
{
    TCCState* s;
    char* cText;
    char* runtimeSource;
    XL6_ArtifactRuntimePlan artifactRuntime;
    XL6_IrCgenOptions options = {0};
    int ok;

    s = NULL;
    if ( outputPath == NULL || outputPath[0] == '\0' ) {
        char text[128];
        snprintf(text, sizeof(text), "output %s path is empty", outputKind != NULL ? outputKind : "file");
        xlSetError(ctx, text);
        return 0;
    }
    cText = NULL;
    runtimeSource = NULL;
    memset(&artifactRuntime, 0, sizeof(artifactRuntime));
    if ( requestedOptions != NULL ) options = *requestedOptions;
    options.emitScriptEntry = 1;
    /* 显式混编 C 且没有语言入口时，由 C 提供 main；纯脚本仍可只执行全局初始化。 */
    options.emitMainWrapper = outputType == TCC_OUTPUT_EXE &&
        (cSourceFiles == NULL || cSourceFiles->count == 0 ||
         xlCompileUnitHasExecutableEntry(ctx, unit) > 0);
    ok = 0;
    xlTccDynamicVfsClear();
    if ( !xlCgenToMemory(ctx, unit, &cText, &options) ) {
        goto cleanup;
    }
    if (!xl6ArtifactRuntimePlanBuild(&unit->ir, options.emitMainWrapper
            ? XL6_ARTIFACT_ENTRY_EXECUTABLE_V1 : XL6_ARTIFACT_ENTRY_CHECKED_MODULE_V1,
            &artifactRuntime, &unit->parse.diags)) {
        xlSetCompileDiagError(ctx, unit);
        goto cleanup;
    }
    if ( !xlSaveTempTextByName(ctx, "module.c", cText) ) {
        goto cleanup;
    }
    s = xlCreateTccState(ctx, outputType);
    if ( s == NULL ) {
        goto cleanup;
    }
    if ( !xlTccPrepareNativeSources(ctx, s, unit, cSourceFiles) ) {
        goto cleanup;
    }
    if ( !xlTccCompileSourceText(ctx, s, XL_TCC_VFS_RUNTIME_C,
            "__attribute__((weak)) unsigned int __mzerosf = 0x80000000u;\n"
            "__attribute__((weak)) unsigned long long __mzerodf = 0x8000000000000000ull;\n",
            "cannot compile TCC runtime constants") ) {
        goto cleanup;
    }
    if ( !xlTccCompileGeneratedModule(ctx, s, cText) ) {
        goto cleanup;
    }
    xlAddDefaultTccRuntime(s, ctx, outputType);
    if ( !xlCompileUnitCSourceFiles(ctx, s, unit, cSourceFiles) ) {
        goto cleanup;
    }
    if ( !xlAddDependenciesToTcc(ctx, s, unit) ) {
        goto cleanup;
    }
    if ( !xlAddResourceFilesToTcc(ctx, s) ) {
        goto cleanup;
    }
    if ( !xlAddEmbeddedTccRuntimeSources(ctx, s, unit, outputType) ) {
        goto cleanup;
    }
    if ( !xlAddPrivateLanguageRuntimeSources(ctx, s, unit, &artifactRuntime) ) {
        goto cleanup;
    }
    if ( !xlAddEmbeddedXlangRuntimeSources(ctx, s, unit, outputType) ) {
        goto cleanup;
    }
    if ( !xl6XrtTrimBuildImplementationSourceFromArtifact(&unit->importDeps,
            cSourceFiles != NULL ? cSourceFiles->count : 0,
            artifactRuntime.nativeModules, artifactRuntime.nativeModuleCount, &runtimeSource) ) {
        xlSetError(ctx, "cannot generate XRT implementation source");
        goto cleanup;
    }
    if ( !xlSaveTempTextByName(ctx, "xl6_xrt_impl.c", runtimeSource) ) {
        goto cleanup;
    }
	if ( !xlTccCompileSourceText(ctx, s, XL_TCC_VFS_XRT_C, runtimeSource, "cannot compile selected XRT/xruntime product") ) {
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
    xrtFree(runtimeSource);
    xlTccDynamicVfsClear();
    return ok;
}

static int xlCompileUnitToExternalOutputEx(
    XL_Context* ctx,
    XL6_CompileUnit* unit,
    const char* outputPath,
    int outputType,
    const char* outputKind,
    const XL6_IrCgenOptions* options)
{
    return xlCompileUnitToExternalOutputWithC(ctx, unit, outputPath,
        outputType, outputKind, options, NULL);
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
    XL6_IrCgenOptions options;

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
    case XL6_AST_PROTOCOL:
    case XL6_AST_ENUM:
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

static int xlCompileUnitToSharedLibraryWithC(
    XL_Context* ctx, XL6_CompileUnit* unit, const char* outputDll,
    const XL_StringList* cSourceFiles)
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
    return xlCompileUnitToExternalOutputWithC(ctx, unit, outputDll,
        TCC_OUTPUT_DLL, "shared library", NULL, cSourceFiles);
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
#ifdef _WIN32
    options.tempDir = "build\\tmp";
#else
    options.tempDir = "build/tmp";
#endif
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
        xl6ConsoleSessionBegin();
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
        xl6ConsoleSessionEnd();
    }
}

static int xlRuntimeAcquireContextUnlocked(void)
{
    if ( xlRuntimeRefTotal() == 0 ) {
        xl6ConsoleSessionBegin();
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
        xl6ConsoleSessionEnd();
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

static void xlCollectRetiringModulesUnlocked(void);
static int xlModuleAcquireCode(void*);
static void xlModuleReleaseCode(void*);
#include "src/api/xl_module_read.inc"


void xlRuntimeUnit(void)
{
    xlApiLock();
    xlCollectRetiringModulesUnlocked();
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
    if ( !xlModuleRetainLoadedLibraries(module, ctx) ) {
        xrtFree(module);
        xlRuntimeReleaseManualUnlocked();
        return NULL;
    }
    module->state = s;
    module->ownsRuntimeRef = 1;
    module->ownershipTrace = (XL6_ModuleOwnershipTrace)xl__tcc.tcc_get_symbol(s, "xl_script_module_ownership_trace");
    {
        XL6_ModuleReadContractGetV1 getRead = (XL6_ModuleReadContractGetV1)
            xl__tcc.tcc_get_symbol(s, "xl_script_module_ownership_read_v1");
        const XL6_ModuleReadContractV1* read = getRead ? getRead() : NULL;
        if (getRead) {
            if (!read || read->size != sizeof(*read) || read->abiTag != 0x584C364D524431ULL ||
                !read->trace || read->trace != module->ownershipTrace) {
                xlModuleLoadedLibrariesFree(module); xrtFree(module);
                xlRuntimeReleaseManualUnlocked(); xlSetError(ctx, "invalid module ownership read V1 contract");
                return NULL;
            }
            module->ownershipReadContract = read;
        }
    }
    {
        XL6_ModuleRetirementGet get = (XL6_ModuleRetirementGet)xl__tcc.tcc_get_symbol(s, "xl_script_module_retirement_v1");
        XL6_ModuleRetirementGetV2 getV2 = (XL6_ModuleRetirementGetV2)xl__tcc.tcc_get_symbol(s, "xl_script_module_retirement_v2");
        XL6_ModuleRetirementGetV3 getV3 = (XL6_ModuleRetirementGetV3)xl__tcc.tcc_get_symbol(s, "xl_script_module_retirement_v3");
        XL6_ModuleRetirementGetV4 getV4 = (XL6_ModuleRetirementGetV4)xl__tcc.tcc_get_symbol(s, "xl_script_module_retirement_v4");
        const XL6_ModuleRetirementV1* contract = get ? get() : NULL;
        const XL6_ModuleRetirementV2* preparation = getV2 ? getV2() : NULL;
        const XL6_ModuleRetirementV3* preflight = getV3 ? getV3() : NULL;
        const XL6_ModuleRetirementV4* step = getV4 ? getV4() : NULL;
        if (contract && contract->size == sizeof(*contract) && contract->abiTag == 0x584C364D525431ULL &&
            contract->trace == module->ownershipTrace && contract->trace && contract->finish &&
            ((getV2 != NULL) + (getV3 != NULL) + (getV4 != NULL) <= 1) &&
            (!getV2 || (preparation && preparation->size == sizeof(*preparation) &&
                preparation->abiTag == 0x584C364D525432ULL && preparation->graph == contract && preparation->prepare)) &&
            (!getV3 || (preflight && preflight->size == sizeof(*preflight) &&
                preflight->abiTag == 0x584C364D525433ULL && preflight->graph == contract && preflight->preflight)) &&
            (!getV4 || (step && step->size == sizeof(*step) && step->abiTag == 0x584C364D525434ULL &&
                step->graph == contract && step->preflight && step->step))) {
            module->retirementContract = contract;
            module->retirementPreparation = preparation;
            module->retirementPreflight = preflight;
            module->retirementStep = step;
        }
        /* An advertised typed contract cannot silently fall back to bool:
         * that would reinterpret BUSY as a semantic failure or safe unload. */
        if (getV4 && !module->retirementStep) {
            xlModuleLoadedLibrariesFree(module); xrtFree(module);
            xlRuntimeReleaseManualUnlocked(); xlSetError(ctx, "invalid module retirement V4 contract");
            return NULL;
        }
    }
    module->codeOwnerGraph = (XL6_CodeOwnerGraph){
        {/* Only a graph-aware artifact may receive the tagged tail. A legacy
          * binder can copy just the prefix without normalizing its size. */
         module->ownershipTrace != NULL ? sizeof(XL6_CodeOwnerGraph) : sizeof(XL6_CodeOwner),
         module, xlModuleAcquireCode, xlModuleReleaseCode},
        0x584C36434F574E31ULL, &xlModuleOwnershipOps};
    {
        XL6_BindCodeOwnerProc bind = (XL6_BindCodeOwnerProc)xl__tcc.tcc_get_symbol(s, "xl_script_bind_code_owner");
        XL6_BindOwnershipReadV1 bindRead = (XL6_BindOwnershipReadV1)
            xl__tcc.tcc_get_symbol(s, "xl_script_bind_ownership_read_v1");
        if (bindRead && (!bind || !module->ownershipReadContract || !module->retirementContract)) {
            xlModuleLoadedLibrariesFree(module); xrtFree(module);
            xlRuntimeReleaseManualUnlocked(); xlSetError(ctx, "module read binding requires certified owning slots and lifecycle");
            return NULL;
        }
        if (bind != NULL && !bind(&module->codeOwnerGraph.base)) {
            /* Binding only stores the capability. No acquisition is accepted
             * until it succeeds; the caller still owns TCC state on failure. */
            xlModuleLoadedLibrariesFree(module); xrtFree(module);
            xlRuntimeReleaseManualUnlocked(); xlSetError(ctx, "module code-owner binding failed");
            return NULL;
        }
        if (bindRead && !bindRead(xlModuleOwnershipReadBoundaryV1())) {
            /* Only the resident API supplies this independent capability.
             * No execution/acquisition has been admitted before both binds.
             * Failed binding leaves the caller owning the TCC state. */
            xlModuleLoadedLibrariesFree(module); xrtFree(module);
            xlRuntimeReleaseManualUnlocked(); xlSetError(ctx, "module ownership read binding failed");
            return NULL;
        }
    }
    module->codeOwnerReady = 1;
    return module;
}

/* Intrusive ownership: failed one-shot retirement must not lose the only
 * pointer to a module. No allocation is needed on this failure path. */
static XL_Module* xl__retiringModules;
static size_t xl__retirementPass;
static int xl__collectingRetirements;
/* API lifecycle exclusion serializes all host modules, including plans paused
 * for nonblocking Freeze admission. During callbacks the mutex is released,
 * but this guard prevents another driver from deleting any pinned module. */
static int xl__ownershipDriverRunning;

/* Caller owns an entry/retirement pin and invokes outside the API mutex.
 * Semantic failure is captured by the surrounding error scope, never inferred
 * from the presence or absence of a diagnostic allocation. */
static XL6_ModuleRetireResult xlModuleRetirementStep(XL_Module* module, bool graph)
{
    if (module->retirementStep) {
        XL6_ModuleRetireResult result = module->retirementStep->step();
        if (result == XL6_MODULE_RETIRE_READY || result == XL6_MODULE_RETIRE_BUSY ||
            result == XL6_MODULE_RETIRE_FAILED) return result;
        xrtSetErrorKind(XERR_INTERNAL); return XL6_MODULE_RETIRE_FAILED;
    }
    if (graph) return module->retirementContract->finish() ? XL6_MODULE_RETIRE_READY : XL6_MODULE_RETIRE_FAILED;
    XL_ScriptUnitCheckedProc checked = (XL_ScriptUnitCheckedProc)xl__tcc.tcc_get_symbol(module->state, "xl_script_module_unit_checked");
    if (checked) return checked() ? XL6_MODULE_RETIRE_READY : XL6_MODULE_RETIRE_FAILED;
    XL_ScriptVoidProc unit = (XL_ScriptVoidProc)xl__tcc.tcc_get_symbol(module->state, "xl_script_module_unit");
    if (unit) unit();
    return XL6_MODULE_RETIRE_READY;
}

static bool xlModuleSnapshotHold(const void* data)
{
    XL_Module* module = (XL_Module*)data; size_t count;
    if (!xlModuleOwnershipCount(module, &count) || count == SIZE_MAX) return false;
    ++module->snapshotRefs; return true;
}
static void xlModuleSnapshotDrop(const void* data)
{
    XL_Module* module = (XL_Module*)data;
    xlApiLock();
    if (!module->snapshotRefs || !xl__ownershipDriverRunning) abort();
    --module->snapshotRefs;
    xlApiUnlock();
}
static bool xlModuleClaim(const void* data, const void* token)
{
    XL_Module* module = (XL_Module*)data;
    if (!module->destroyRequested || module->ownershipCleared ||
        (module->ownershipClaim && module->ownershipClaim != token)) return false;
    module->ownershipClaim = token; return true;
}
static void xlModuleRestore(const void* data, const void* token)
{
    XL_Module* module = (XL_Module*)data;
    if (module->ownershipClaim != token || module->ownershipCleared) abort();
    module->ownershipClaim = NULL;
}
static void xlModuleClear(const void* data, const void* token)
{
    XL_Module* module = (XL_Module*)data;
    if (module->ownershipClaim != token || module->ownershipCleared) abort();
    module->ownershipCleared = 1;
    /* Globals are quarantined retired slots until checked Finish. All owning
     * descendants have actual plan pins; no generated cleanup under Freeze. */
}
static XL6_OwnershipStepResult xlModuleFinishOwnershipV2(const void* data, const void* token)
{
    XL_Module* module = (XL_Module*)data;
    xl6_error_scope cleanup = {0}; xl6_error_result outcome;
    if (module->ownershipClaim != token || !module->ownershipCleared) abort();
    if (!xl6ErrorScopeBegin(&cleanup)) abort();
    XL6_ModuleRetireResult result = xlModuleRetirementStep(module, true);
    if (result == XL6_MODULE_RETIRE_FAILED) xl6ErrorScopeCaptureFailure(&cleanup);
    outcome = xl6ErrorScopeEnd(&cleanup);
    xlApiLock();
    module->activation = outcome.ok && result == XL6_MODULE_RETIRE_READY
        ? XL_MODULE_INACTIVE : XL_MODULE_DEACTIVATION_PENDING;
    xlApiUnlock();
    if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome);
    return !outcome.ok ? XL6_OWNERSHIP_STEP_FAILED : result == XL6_MODULE_RETIRE_READY
        ? XL6_OWNERSHIP_STEP_READY : XL6_OWNERSHIP_STEP_BUSY;
}
static bool xlModuleFinishOwnership(const void* data, const void* token)
{ return xlModuleFinishOwnershipV2(data, token) == XL6_OWNERSHIP_STEP_READY; }
static const XL6_OwnershipAdapterV1 xlModuleOwnershipAdapter = {sizeof(xlModuleOwnershipAdapter),
    xlModuleSnapshotHold, xlModuleSnapshotDrop, xlModuleClaim, xlModuleRestore,
    NULL, xlModuleClear, xlModuleFinishOwnership};
static const XL6_OwnershipAdapterV1* xlModuleResolveOwnership(xrtownershipref ref, void* context)
{
    const XL6_OwnershipAdapterV1* adapter; (void)context;
    if (ref.Ops == &xlModuleOwnershipOps && ref.Data) {
        const XL_Module* module = ref.Data; size_t count;
        return module->retirementContract && xlModuleOwnershipCount(module, &count) ? &xlModuleOwnershipAdapter : NULL;
    }
    if ((adapter = xl6ClosureOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xl6CellOwnershipAdapterV1(ref)) != NULL) return adapter;
    if((adapter=xl6SlotMapKeyValueOwnershipAdapterV1(ref))!=NULL) return adapter;
    if((adapter=xl6ChannelOwnershipAdapterV1(ref))!=NULL) return adapter;
    {
        const XL6_SequenceAdmissionOpsV2* policies[]={xl6ArrayFutureAdmissionOpsV2Get(),
            xl6ArraySemAdmissionOpsV2Get(),xl6ArraySyncAdmissionOpsV2Get(),xl6ArrayThreadAdmissionOpsV2Get(),
            xl6ArraySlotMapKeyAdmissionOpsV2Get()};
        size_t policyCount=sizeof(policies)/sizeof(*policies);
        if ((adapter=xl6SpscQueueOwnershipAdapterV1(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6MpscQueueOwnershipAdapterV1(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6MpmcQueueOwnershipAdapterV1(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6WaitQueueOwnershipAdapterV1(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6SequenceOwnershipAdapterV2(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6ListOwnershipAdapterV2(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6SetOwnershipAdapterV2(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6DictOwnershipAdapterV2(ref,policies,policyCount)) != NULL) return adapter;
        if ((adapter=xl6SlotMapOwnershipAdapterV2(ref,policies,policyCount)) != NULL) return adapter;
    }
    if ((adapter=xl6TreeOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter=xl6SemOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter=xl6SyncOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter=xl6ThreadOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xl6ValueIteratorOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xrtValueCursorOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xl6CodeLeaseOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xl6ClassOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xrtValueCallableOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xrtValueFutureOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xl6PayloadOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xl6FutureAggregateOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xrtCancelOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xrtErrorOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xl6ErrorValueOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xrtNetAddrListOwnershipAdapterV1(ref)) != NULL) return adapter;
    if ((adapter = xrtDynamicFieldsOwnershipAdapterV1(ref)) != NULL) return adapter;
    {
        const XL6_NativeHandlePolicyV1* policies[] = {
            xl6ProcessCommandHandlePolicyV1Get(), xl6ProcessHandlePolicyV1Get(),
            xl6ProcessStatusHandlePolicyV1Get(), xl6BufferIteratorHandlePolicyV1Get(),
            xl6DynamicFieldsHandlePolicyV1Get()
        };
        for (size_t i = 0; i < sizeof(policies)/sizeof(*policies); ++i)
            if ((adapter = xrtValueHandleOwnershipAdapterV1(ref,
                    policies[i]->Ops, policies[i]->Trace)) != NULL) return adapter;
    }
    /* Unknown class/native policies and Future nodes still REFUSE before
     * Count/Trace. Known Handle bridges do not authorize opaque children. */
    return xrtValueOwnershipAdapterV1(ref);
}
static bool xlModuleResolveOwnershipV2(xrtownershipref ref, void* context,
    const XL6_OwnershipAdapterV1** lifecycle, const xrtownershippreparationv1** preparation)
{
    enum {
        XL6_RESIDENT_PAYLOAD_COUNT =
            XL6_FUTURE_PAYLOAD_COUNT - XL6_FUTURE_PAYLOAD_VALUE
    };
    const XL6_OwnershipAdapterV1* adapter;
    const xfuturepayloadownershipv1* payloads[XL6_RESIDENT_PAYLOAD_COUNT + 3];
    const xfutureproducerownershipv1* producers[]={xl6FutureCallbackProducerPolicyV1Get(),xl6TaskProducerPolicyV1Get(),xrtTaskProducerPolicyV1Get(),xl6ContinuationProducerPolicyV1Get(),xrtFutureCombineProducerPolicyV1Get(),xrtNetResolverFutureProducerPolicyV1Get()};
    const xfuturewatchownershipv1* watches[]={xl6FutureCallbackWatchPolicyV1Get(),xl6TaskWatchPolicyV1Get(),xl6ContinuationWatchPolicyV1Get()};
    const xfuturewatchownershipv2* projectedWatches[]={xrtFutureCombineWatchPolicyV2Get()};
    const xcancelwatchownershipv1* cancelWatches[]={xrtFutureCombineCancelPolicyV1Get(),xl6FutureCallbackCancelPolicyV1Get(),xrtNetResolverFutureCancelPolicyV1Get()};
    const xfuturecombineownershipv1* aggregateMaps[]={xl6FutureAggregatePolicyV1Get()};
    const xtaskdataownershipv1* tasks[]={xl6TaskNativeDataPolicyV1Get()};
    const XL6_ContinuationStorageV2* queuedStorage[]={xl6ContinuationProducedStorageV2Get()};
    const xnetresolveownershipv1* resolverRequests[]={xrtNetResolverFutureRequestPolicyV1Get()};
    if((adapter=xl6ContinuationExecutorOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xl6TaskPoolOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtTaskPoolOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xl6ContinuationQueueOwnershipAdapterV1(ref,queuedStorage,1,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xl6FutureCallbackOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xl6TaskOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xl6GeneratorOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xl6ContinuationFrameOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtTaskOwnershipAdapterV1(ref,tasks,1,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtFutureCombineOwnershipAdapterV1(ref,aggregateMaps,1,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtCancelWatchOwnershipAdapterV1(ref,cancelWatches,sizeof(cancelWatches)/sizeof(*cancelWatches),preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtCancelOwnershipAdapterV2(ref,cancelWatches,sizeof(cancelWatches)/sizeof(*cancelWatches),preparation))!=NULL){*lifecycle=adapter;return true;}
    for(unsigned i=0;i<XL6_RESIDENT_PAYLOAD_COUNT;++i)
        payloads[i]=xl6PayloadPolicyGet((XL6_FuturePayloadKind)(XL6_FUTURE_PAYLOAD_VALUE+i));
    payloads[XL6_RESIDENT_PAYLOAD_COUNT]=xrtFutureCombineAllPayloadPolicyV1Get();
    payloads[XL6_RESIDENT_PAYLOAD_COUNT+1]=xrtFutureCombinePickPayloadPolicyV1Get();
    payloads[XL6_RESIDENT_PAYLOAD_COUNT+2]=xrtNetResolverFuturePayloadPolicyV1Get();
    const xfutureownershipadmissionv1 admission={sizeof(admission),payloads,
        sizeof(payloads)/sizeof(*payloads),
        producers,sizeof(producers)/sizeof(*producers),watches,sizeof(watches)/sizeof(*watches),
        projectedWatches,1,cancelWatches,sizeof(cancelWatches)/sizeof(*cancelWatches)};
    if((adapter=xrtFutureOwnershipAdapterV4(ref,&admission,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtNetResolverFutureOwnershipAdapterV1(ref,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtNetResolveOpOwnershipAdapterV1(ref,resolverRequests,1,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtNetResolverOwnershipAdapterV1(ref,NULL,0,preparation))!=NULL){*lifecycle=adapter;return true;}
    if((adapter=xrtNetEngineOwnershipAdapterV1(ref,NULL,0,NULL,0,preparation))!=NULL){*lifecycle=adapter;return true;}
    adapter=xlModuleResolveOwnership(ref,context);
    if(!adapter)return false;
    *lifecycle=adapter;*preparation=NULL;return true;
}
static bool xlModuleResolveOwnershipV3(xrtownershipref ref, void* context,
    const XL6_OwnershipAdapterV1** lifecycle, const xrtownershippreparationv1** preparation,
    const XL6_OwnershipFinishV1** completion)
{
    static const XL6_OwnershipFinishV1 moduleFinish = {sizeof(moduleFinish),
        &xlModuleOwnershipAdapter, xlModuleFinishOwnershipV2};
    if (!xlModuleResolveOwnershipV2(ref, context, lifecycle, preparation)) return false;
    *completion = *lifecycle == &xlModuleOwnershipAdapter && ((const XL_Module*)ref.Data)->retirementStep
        ? &moduleFinish : NULL;
    return true;
}
static XL6_OwnershipStepResult xlModuleOwnershipStepV2(XL6_OwnershipPlan* plan,
    XL6_OwnershipStepResult (*step)(XL6_OwnershipPlan*),xl6_error_result* outcome)
{
    xl6_error_scope scope={0};
    if(!xl6ErrorScopeBegin(&scope))abort();
    XL6_OwnershipStepResult status=step(plan);
    if(status==XL6_OWNERSHIP_STEP_FAILED)xl6ErrorScopeCaptureFailure(&scope);
    bool semanticFailure=scope.failed;
    *outcome=xl6ErrorScopeEnd(&scope);
    /* Snapshot allocation/admission failure only defers collection. Preserve
     * an independently reported callback failure even if a later rebuild
     * also fails; diagnostic allocation is never the semantic failure bit. */
    if(status==XL6_OWNERSHIP_STEP_ERROR && !semanticFailure){
        xrtErrorFree(outcome->error);*outcome=(xl6_error_result){true,NULL};
    }
    return status;
}
static bool xlModuleDiscoverOwnership(XL6_ModuleOwnershipVisit visit,void* visitorContext,void* context)
{
    const XL_Module* module=context;size_t count;
    /* Reject active generated code, construction/retirement transitions and
     * unsupported module contracts BEFORE inspecting the class registry.
     * Freeze and the existing host lifecycle driver exclusion span this
     * borrowed enumeration and its subsequent snapshot/pin transaction. */
    if(!module || !module->retirementContract || !xlModuleOwnershipCount(module,&count))return false;
    return xl6ClassOwnershipDiscoverV1(&module->codeOwnerGraph.base,visit,visitorContext);
}
static void xlModuleAdvanceOwnershipUnlocked(XL_Module* module)
{
    XL6_OwnershipPlanResult result; xerror* prior;
    xl6_error_result outcome = {true, NULL};
    if (xl__ownershipDriverRunning) return;
    xl__ownershipDriverRunning = 1; prior = xrtTakeError();
    if (!module->ownershipPlan) {
        result = xl6OwnershipPlanTryCreateV4((xrtownershipref){module, &xlModuleOwnershipOps},
            xlModuleResolveOwnershipV3, xlModuleDiscoverOwnership, module, &module->ownershipPlan);
        if (result <= XL6_OWNERSHIP_PLAN_BUSY) goto done;
        module->ownershipPhase = result == XL6_OWNERSHIP_PLAN_ROOTED ? 5 :
            result == XL6_OWNERSHIP_PLAN_PREPARATIONS ? 6 : 1;
    }
    /* A bounded driver turn. New semantic duties can request another pass;
     * BUSY/failure keeps the exact plan and real pins in the registry. */
    if (module->ownershipPhase == 6) {
        XL6_OwnershipStepResult prepared;
        xlApiUnlock();
        prepared=xlModuleOwnershipStepV2(module->ownershipPlan,xl6OwnershipPlanPrepare,&outcome);xlApiLock();
        if(!outcome.ok || prepared==XL6_OWNERSHIP_STEP_ERROR)module->ownershipPhase=5;
        else if(prepared==XL6_OWNERSHIP_STEP_READY)module->ownershipPhase=1;
        else goto done;
    }
    if (module->ownershipPhase == 1) {
        XL6_OwnershipStepResult finalized;
        xlApiUnlock();finalized=xlModuleOwnershipStepV2(module->ownershipPlan,xl6OwnershipPlanFinalizeV2,&outcome);xlApiLock();
        if(!outcome.ok || finalized==XL6_OWNERSHIP_STEP_ERROR)module->ownershipPhase=5;
        else if(finalized==XL6_OWNERSHIP_STEP_BUSY)goto done;
        else module->ownershipPhase=2;
    }
    if (module->ownershipPhase == 2) {
        result = xl6OwnershipPlanTryCommit(module->ownershipPlan);
        if (result == XL6_OWNERSHIP_PLAN_READY) module->ownershipPhase = 3;
        else if (result == XL6_OWNERSHIP_PLAN_FINALIZERS) module->ownershipPhase = 1;
        else if (result == XL6_OWNERSHIP_PLAN_PREPARATIONS) module->ownershipPhase = 6;
        else if (result != XL6_OWNERSHIP_PLAN_BUSY) module->ownershipPhase = 5;
    }
    if (module->ownershipPhase == 3) {
        XL6_OwnershipStepResult finished;
        xlApiUnlock(); finished = xlModuleOwnershipStepV2(module->ownershipPlan, xl6OwnershipPlanFinishV2, &outcome); xlApiLock();
        if (outcome.ok && finished == XL6_OWNERSHIP_STEP_READY) module->ownershipPhase = 4;
    }
    if (module->ownershipPhase == 4 || module->ownershipPhase == 5) {
        /* Abort restores quarantine under its Freeze. Drop may invoke host
         * release hooks, so the API mutex must not span this operation. */
        xlApiUnlock(); result = xl6OwnershipPlanTryDestroy(module->ownershipPlan); xlApiLock();
        if (result == XL6_OWNERSHIP_PLAN_READY) {
            module->ownershipPlan = NULL; module->ownershipPhase = 0;
        }
    }
done:
    /* Snapshot refusal/OOM merely defers collection. A semantic finalizer's
     * explicit failure is different: latch it independently of allocation,
     * publish it at this void API boundary, and never repeat its physical duty.
     * The immutable diagnostic DAG is admitted on the next fresh plan. */
    if (!outcome.ok && !module->retirementFailed) {
        module->retirementFailed = 1;
        module->retirementError = xrtErrorRef(outcome.error);
    }
    xrtClearError(); xrtSetErrorTake(prior); xl__ownershipDriverRunning = 0;
    if (!outcome.ok) {
        if (prior != NULL) { xrtErrorFree(outcome.error); outcome.error = xrtErrorRef(prior); }
        xl6ErrorScopePropagateFailure(outcome);
    }
}

static void xlModuleUnqueueRetirementUnlocked(XL_Module* module)
{
    XL_Module** link = &xl__retiringModules;
    while (*link != NULL && *link != module) link = &(*link)->retirementNext;
    if (*link == module) *link = module->retirementNext;
    module->retirementQueued = 0; module->retirementNext = NULL;
}

static int xlModulePrepareRetirementUnlocked(XL_Module* module)
{
    xrtownershipscope admission = {0}; xl6_error_scope cleanup = {0};
    xl6_error_result outcome; XL6_ModulePrepareResult result;
    if ((!module->retirementPreparation && !module->retirementPreflight && !module->retirementStep) || module->retirementPrepared) return 1;
    /* A resident release may arrive inside its caller's mutation (or freeze).
     * Never join workers from that boundary. This short, nonblocking probe is
     * NOT a snapshot or permission to trace; release it before preparation. */
    if (!xrtOwnershipFreezeTryBegin(&admission)) return 0;
    if (!xrtOwnershipScopeEnd(&admission)) abort();
    if (module->preparationRefs || module->ownershipPlan || xl__ownershipDriverRunning) return 0;
    ++module->preparationRefs; module->retirementRunning = 1; xl__ownershipDriverRunning = 1;
    xlApiUnlock();
    if (!xl6ErrorScopeBegin(&cleanup)) abort();
    result = module->retirementStep ? module->retirementStep->preflight()
        : module->retirementPreflight ? module->retirementPreflight->preflight()
        : module->retirementPreparation->prepare();
    if (result != XL6_MODULE_PREPARE_READY && result != XL6_MODULE_PREPARE_BUSY)
        xl6ErrorScopeCaptureFailure(&cleanup);
    outcome = xl6ErrorScopeEnd(&cleanup);
    xlApiLock();
    if (!module->preparationRefs || !module->retirementRunning || !xl__ownershipDriverRunning) abort();
    --module->preparationRefs; module->retirementRunning = 0; xl__ownershipDriverRunning = 0;
    if (outcome.ok && result == XL6_MODULE_PREPARE_READY) module->retirementPrepared = 1;
    if (!outcome.ok) {
        if (!module->retirementFailed) {
            module->retirementFailed = 1; module->retirementError = xrtErrorRef(outcome.error);
        }
        if (xrtGetError() != NULL) {
            xerror* primary = xrtErrorRef(xrtGetError());
            xrtErrorFree(outcome.error); outcome.error = primary;
        }
        xl6ErrorScopePropagateFailure(outcome);
    }
    return module->retirementPrepared;
}

static int xlModuleTryRetireUnlocked(XL_Module* module)
{
    XL_ScriptVoidProc errorUnit;
    XL_ScriptUnitCheckedProc checkedUnit;
    XL6_ModuleRetireResult retired;
    xl6_error_scope cleanup = {0}; xl6_error_result outcome;
    int ownsRuntimeRef;

    if (module == NULL) return 1;
    if (module->activeCalls != 0 || module->retirementRunning || xl__ownershipDriverRunning) return 0;
    if (module->destroyRequested && !xlModulePrepareRetirementUnlocked(module)) return 0;
    if (module->ownershipPlan || (module->codeLeases != 0 && module->retirementContract && module->destroyRequested))
        xlModuleAdvanceOwnershipUnlocked(module);
    if (module->ownershipPlan || module->snapshotRefs != 0 || module->preparationRefs != 0 || module->codeLeases != 0) return 0;
    module->retirementRunning = 1;
    if ( module->state != NULL ) {
        checkedUnit = (XL_ScriptUnitCheckedProc)xl__tcc.tcc_get_symbol(module->state, "xl_script_module_unit_checked");
        /* Checked artifacts own an idempotent phase guard. One-shot execution
         * never activates the host module, but its cleanup may still be busy;
         * host INACTIVE alone is therefore not proof that code can be freed. */
        if ( module->activation != XL_MODULE_INACTIVE || checkedUnit != NULL || module->retirementStep ) {
            module->activation = XL_MODULE_DEACTIVATING;
            /* A failed caller's ambient diagnostic must not be mistaken for
             * failure to retire this module. Observe this operation's own
             * outcome in isolation, restoring the earlier diagnostic. */
            xlApiUnlock();
            if (!xl6ErrorScopeBegin(&cleanup)) abort();
            retired = xlModuleRetirementStep(module, false);
            if (retired == XL6_MODULE_RETIRE_FAILED) xl6ErrorScopeCaptureFailure(&cleanup);
            outcome = xl6ErrorScopeEnd(&cleanup);
            xlApiLock();
            if (!outcome.ok) {
                module->activation = XL_MODULE_DEACTIVATION_PENDING;
                module->retirementRunning = 0;
                if (!module->retirementFailed) {
                    module->retirementFailed = 1; module->retirementError = xrtErrorRef(outcome.error);
                }
                if (xrtGetError() != NULL) {
                    xerror* primary = xrtErrorRef(xrtGetError());
                    xrtErrorFree(outcome.error); outcome.error = primary;
                }
                xl6ErrorScopePropagateFailure(outcome);
                return 0;
            }
            if (retired == XL6_MODULE_RETIRE_BUSY) {
                module->activation = XL_MODULE_DEACTIVATION_PENDING;
                module->retirementRunning = 0;
                return 0;
            }
            module->activation = XL_MODULE_INACTIVE;
        }
        errorUnit = (XL_ScriptVoidProc)xl__tcc.tcc_get_symbol(module->state, "xl_script_error_unit");
        if ( errorUnit != NULL ) {
            xerror* saved = xrtTakeError();
            errorUnit();
            xrtClearError(); xrtSetErrorTake(saved);
        }
        xl__tcc.tcc_delete(module->state);
        module->state = NULL;
    }
    xlModuleLoadedLibrariesFree(module);
    xlModuleUnqueueRetirementUnlocked(module);
    xrtErrorFree(module->retirementError);
    ownsRuntimeRef = module->ownsRuntimeRef;
    module->ownsRuntimeRef = 0;
    /* 最后一个运行时引用会关闭 XRT 内存池，模块外壳必须在此之前释放。 */
    xrtFree(module);
    if ( ownsRuntimeRef ) {
        xlRuntimeReleaseManualUnlocked();
    }
    return 1;
}

static void xlDestroyModuleUnlocked(XL_Module* module)
{
    if (module == NULL) return;
    module->destroyRequested = 1;
    if (!module->retirementQueued) {
        module->retirementNext = xl__retiringModules; xl__retiringModules = module;
        module->retirementQueued = 1;
    }
    (void)xlModuleTryRetireUnlocked(module);
}

static void xlCollectRetiringModulesUnlocked(void)
{
    XL_Module* module; size_t remaining = 0, pass;
    if (xl__collectingRetirements) return;
    xl__collectingRetirements = 1;
    if (++xl__retirementPass == 0) {
        ++xl__retirementPass;
        for (module = xl__retiringModules; module != NULL; module = module->retirementNext) module->retirementPass = 0;
    }
    pass = xl__retirementPass;
    for (module = xl__retiringModules; module != NULL; module = module->retirementNext) ++remaining;
    while (remaining-- != 0) {
        /* Never retain a next pointer across callbacks: reentry can retire
         * another module or enqueue a new one. Each pass is bounded. */
        for (module = xl__retiringModules; module != NULL; module = module->retirementNext)
            if (module->retirementPass != pass) break;
        if (module == NULL) break;
        module->retirementPass = pass; (void)xlModuleTryRetireUnlocked(module);
    }
    xl__collectingRetirements = 0;
}

static int xlModuleAcquireCode(void* data)
{
    XL_Module* module = (XL_Module*)data; int ok = 0;
    xlApiLock();
    if (module != NULL && module->codeOwnerReady && !module->destroyRequested && !module->retirementRunning &&
        module->activation != XL_MODULE_DEACTIVATING && module->activation != XL_MODULE_DEACTIVATION_PENDING &&
        module->codeLeases != SIZE_MAX) {
        ++module->codeLeases; ok = 1;
    }
    xlApiUnlock(); return ok;
}

static void xlModuleReleaseCode(void* data)
{
    XL_Module* module = (XL_Module*)data;
    xerror* saved = xrtTakeError();
    xlApiLock();
    if (module == NULL || module->codeLeases == 0) abort();
    --module->codeLeases;
    if (module->destroyRequested) (void)xlModuleTryRetireUnlocked(module);
    xlApiUnlock();
    xrtClearError(); xrtSetErrorTake(saved);
}

static void xlModuleEndCallUnlocked(XL_Module* module)
{
    if (module == NULL || module->activeCalls == 0) return;
    module->activeCalls -= 1;
    if ( module->activeCalls == 0 && module->destroyRequested ) {
        xlDestroyModuleUnlocked(module);
    }
}

static void xlModuleRequestDestroyUnlocked(XL_Module* module)
{
    xlDestroyModuleUnlocked(module);
}

static XL_Module* xlCompileUnitToModuleWithC(
    XL_Context* ctx, XL6_CompileUnit* unit,
    const XL6_IrCgenOptions* requestedOptions,
    const XL_StringList* cSourceFiles)
{
    TCCState* s;
    XL_Module* module;
    char* cText;
    XL6_IrCgenOptions options = {0};

    s = NULL;
    module = NULL;
    cText = NULL;
    if ( requestedOptions != NULL ) options = *requestedOptions;
    options.emitScriptEntry = 1;
    options.emitMainWrapper = 0;
    xlTccDynamicVfsClear();
    if ( !xlCgenToMemory(ctx, unit, &cText, &options) ) {
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
    if ( !xlTccPrepareNativeSources(ctx, s, unit, cSourceFiles) ) {
        goto cleanup;
    }
    if ( !xlTccCompileGeneratedModule(ctx, s, cText) ) {
        goto cleanup;
    }
    if ( !xlCompileUnitCSourceFiles(ctx, s, unit, cSourceFiles) ) {
        goto cleanup;
    }
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

static XL_Module* xlCompileUnitToModuleEx(XL_Context* ctx, XL6_CompileUnit* unit, const XL6_IrCgenOptions* options)
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
    xlTccSelectFullRuntime(s);
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
    xlTccSelectFullRuntime(s);
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

static int xlRunModuleMainUnlocked(
    XL_Context* ctx,
    XL_Module* module,
    xvalue* pSuppliedArg
)
{
    XL_XlangMainProc entry;
    XL_ScriptMainCheckedProc checkedEntry;
    XL6_ModuleRetireResult retired;
    xl6_error_scope cleanup = {0};
    xl6_error_result outcome;
    xvalue* pRunArg;
    int rc, ok;

    if ( ctx == NULL || module == NULL || module->state == NULL ) {
        xlSetError(ctx, "module is empty");
        return 0;
    }
    if ( module->activation != XL_MODULE_INACTIVE ) {
        xlSetError(ctx, "active module cannot run as a one-shot script");
        return 0;
    }
    entry = (XL_XlangMainProc)xl__tcc.tcc_get_symbol(module->state, "xlang_main");
    checkedEntry = (XL_ScriptMainCheckedProc)xl__tcc.tcc_get_symbol(module->state, "xl_script_main_checked");
    if ( entry == NULL && checkedEntry == NULL ) {
        xlSetError(ctx, "script entry xlang_main not found");
        return 0;
    }
    pRunArg = NULL;
    if ( !xlPrepareRuntimeArg(ctx, pSuppliedArg, &pRunArg) ) {
        return 0;
    }
    /*
     * module 已经完成编译和 relocate。执行用户入口时释放 API 全局锁，
     * 允许服务线程中的 XLangHandler 复用已编译模块。
     */
    xlApiUnlock();
    if ( checkedEntry != NULL ) ok = checkedEntry(pRunArg, &rc);
    else { rc = entry(pRunArg); ok = 1; }
    if ( !xl6ErrorScopeBeginCleanup(&cleanup, !ok) ) abort();
    xrtValueRelease(pRunArg);
    if ( xrtGetError() != NULL ) xl6ErrorScopeCaptureFailure(&cleanup);
    /* A one-shot run never becomes an activated module. Close its language
     * lifetime here, before TCC destruction and while callbacks may use the
     * unlocked API. Verified XLIR cleanup preserves the original exception;
     * older entries that already unload have an idempotent module unit. */
    retired = xlModuleRetirementStep(module, false);
    if (retired == XL6_MODULE_RETIRE_FAILED) xl6ErrorScopeCaptureFailure(&cleanup);
    outcome = xl6ErrorScopeEnd(&cleanup);
    xrtSetErrorTake(outcome.error);
    xlApiLock();
    if (retired != XL6_MODULE_RETIRE_READY || !outcome.ok)
        module->activation = XL_MODULE_DEACTIVATION_PENDING;
    if ( xlCheckScriptActiveError(ctx, module->state) ) {
        return 0;
    }
    if ( !outcome.ok ) {
        xlSetError(ctx, "script execution or cleanup failed without a diagnostic");
        return 0;
    }
    ctx->lastExitCode = rc;
    return 1;
}

/* Consumes the caller's sole module handle. The entry lease stays held until
 * Destroy has transferred ownership to the resident retirement queue. Never
 * touch module after EndCall: cleanup or the last code lease may free it. */
static int xlRunAndRetireModuleUnlocked(XL_Context* ctx, XL_Module* module, xvalue* arg)
{
    int ok;
    XL6_ModuleCall call = {0};
    if (!xlModuleBeginCallUnlocked(module)) {
        xlDestroyModuleUnlocked(module);
        return 0;
    }
    xlModuleCallAttachUnlocked(module,&call);
    ok = xlRunModuleMainUnlocked(ctx, module, arg);
    xlDestroyModuleUnlocked(module);
    xlModuleCallDetachUnlocked(module,&call);
    xlModuleEndCallUnlocked(module);
    return ok;
}

static int xlCompileAndRunUnit(XL_Context* ctx, XL6_CompileUnit* unit, xvalue* arg)
{
    XL_Module* module;
    /* One compiler/binder and one checked execution/retirement path. Neither
     * a busy finalizer nor an accepted continuation permits raw tcc_delete. */
    module = xlCompileUnitToModuleWithC(ctx, unit, NULL, NULL);
    if (module == NULL) return 0;
    return xlRunAndRetireModuleUnlocked(ctx, module, arg);
}

static int xlActivateModuleUnlocked(
    XL_Context* ctx,
    XL_Module* module,
    xvalue* pSuppliedArg
)
{
    XL_ScriptModuleInitProc entry;
    XL6_ModuleRetireResult retired = XL6_MODULE_RETIRE_READY;
    xl6_error_scope cleanup = {0};
    xl6_error_result outcome;
    xvalue* pRunArg;
    int result;

    if ( ctx == NULL || module == NULL || module->state == NULL ) {
        xlSetError(ctx, "module is empty");
        return 0;
    }
    if (module->activation == XL_MODULE_DEACTIVATING || module->activation == XL_MODULE_DEACTIVATION_PENDING) {
        xlSetError(ctx, "module deactivation must complete before activation");
        return 0;
    }
    if ( module->activation == XL_MODULE_ACTIVE ) {
        return 1;
    }
    entry = (XL_ScriptModuleInitProc)xl__tcc.tcc_get_symbol(module->state, "xl_script_module_init");
    if ( entry == NULL ) {
        module->activation = XL_MODULE_ACTIVE;
        return 1;
    }
    pRunArg = NULL;
    if ( !xlPrepareRuntimeArg(ctx, pSuppliedArg, &pRunArg) ) {
        return 0;
    }
    xlApiUnlock();
    result = entry(pRunArg);
    if ( !xl6ErrorScopeBeginCleanup(&cleanup, !result) ) abort();
    xrtValueRelease(pRunArg);
    if ( xrtGetError() != NULL ) xl6ErrorScopeCaptureFailure(&cleanup);
    /* V4 initialization may fail with partially created services even after
     * IR rollback has cleared every language global. Retain their physical
     * cleanup state and preserve the original initialization failure. */
    if (cleanup.failed && (result || module->retirementStep)) {
        retired = xlModuleRetirementStep(module, false);
        if (retired == XL6_MODULE_RETIRE_FAILED) xl6ErrorScopeCaptureFailure(&cleanup);
    }
    outcome = xl6ErrorScopeEnd(&cleanup);
    xrtSetErrorTake(outcome.error);
    xlApiLock();
    if (retired != XL6_MODULE_RETIRE_READY) module->activation = XL_MODULE_DEACTIVATION_PENDING;
    if ( xlCheckScriptActiveError(ctx, module->state) ) {
        return 0;
    }
    if ( !outcome.ok ) {
        xlSetError(ctx, "module initialization failed");
        return 0;
    }
    module->activation = XL_MODULE_ACTIVE;
    return 1;
}

static int xlDeactivateModuleUnlocked(XL_Context* ctx, XL_Module* module)
{
    XL6_ModuleRetireResult retired;
    xl6_error_scope cleanup = {0};
    xl6_error_result outcome;

    if ( ctx == NULL || module == NULL || module->state == NULL ) {
        xlSetError(ctx, "module is empty");
        return 0;
    }
    if ( module->activation == XL_MODULE_INACTIVE ) {
        return 1;
    }
    /* A residency lease is not an execution lease. Module-owned globals with
     * generated finalizers necessarily hold code leases until this retirement
     * step releases them, and external leases keep the image mapped after
     * deactivation. Destruction, not semantic deactivation, resolves that
     * ownership graph. Only concurrent API execution/deactivation is busy. */
    if (module->activation == XL_MODULE_DEACTIVATING || module->activeCalls > 1) {
        xlSetError(ctx, "module is busy executing an entry or deactivation");
        return 0;
    }
    module->activation = XL_MODULE_DEACTIVATING;
    xlApiUnlock();
    if ( !xl6ErrorScopeBeginCleanup(&cleanup, false) ) abort();
    retired = xlModuleRetirementStep(module, false);
    if (retired == XL6_MODULE_RETIRE_FAILED) xl6ErrorScopeCaptureFailure(&cleanup);
    outcome = xl6ErrorScopeEnd(&cleanup);
    xrtSetErrorTake(outcome.error);
    xlApiLock();
    /* Commit only after the checked outcome is acknowledged. Preserve the
     * retry state even if script error reporting consumes the diagnostic. */
    module->activation = XL_MODULE_DEACTIVATION_PENDING;
    if ( xlCheckScriptActiveError(ctx, module->state) ) return 0;
    if ( !outcome.ok ) {
        xlSetError(ctx, "module cleanup failed without a diagnostic");
        return 0;
    }
    if (retired == XL6_MODULE_RETIRE_BUSY) {
        xlSetError(ctx, "module cleanup is pending; retry after outstanding work completes");
        return 0;
    }
    module->activation = XL_MODULE_INACTIVE;
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
static int xlEmitUnitCToFile(XL_Context* ctx, XL6_CompileUnit* unit,
                             const char* outputPath,
                             const XL6_IrCgenOptions* requestedOptions)
{
    XL6_IrCgenOptions options = {0};
    XL6_Output out;
    int ok;
    if ( outputPath == NULL || outputPath[0] == '\0' ) { xlSetError(ctx, "output C path is empty"); return 0; }
    if ( !xlEnsureUnitXlirLowered(ctx, unit) ) return 0;
    if ( !xl6IrCgenVerifyEmittable(&unit->ir, &unit->parse.diags) ) { xlSetCompileDiagError(ctx, unit); return 0; }
    if ( !xl6OutputOpenFile(&out, outputPath) ) { xlSetError2(ctx, "cannot open XLIR C output", outputPath); return 0; }
    if ( requestedOptions != NULL ) options = *requestedOptions;
    ok = xl6IrCgenEmitCEx(&out, &unit->ir, &unit->parse.diags, &options);
    if ( xl6OutputHasFailed(&out) ) { xlSetError2(ctx, "cannot write XLIR C output", xl6OutputError(&out)); ok = 0; }
    else if ( !ok ) xlSetCompileDiagError(ctx, unit);
    if ( !ok ) {
        xl6OutputDiscard(&out);
        return 0;
    }
    xl6OutputClose(&out);
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot write XLIR C output", xl6OutputError(&out));
        return 0;
    }
    return 1;
}

static int xlEmitUnitDebugCToFile(XL_Context* ctx, XL6_CompileUnit* unit, const char* outputPath, const char* debugSourcePath)
{
    XL6_IrCgenOptions options;

    memset(&options, 0, sizeof(options));
    options.debugMode = 1;
    options.debugSourcePath = debugSourcePath;
    return xlEmitUnitCToFile(ctx, unit, outputPath, &options);
}

static int xlEmitUnitHeaderToFile(XL_Context* ctx, XL6_CompileUnit* unit,
                                  const char* outputPath, const char* guardName)
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
    if ( !xlEnsureUnitXlirLowered(ctx, unit) )
        return 0;
    if ( !xl6OutputOpenFile(&out, outputPath) ) {
        xlSetError2(ctx, "cannot open header output", outputPath);
        return 0;
    }
    ok = xl6IrCgenEmitHeader(&out, &unit->ir,
                             guardName != NULL && guardName[0] != '\0' ? guardName : "XL6_GENERATED_HEADER_H",
                             &unit->parse.diags);
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot write header output", xl6OutputError(&out));
        ok = 0;
    } else if ( !ok ) {
        xlSetCompileDiagError(ctx, unit);
    }
    if ( !ok ) {
        xl6OutputDiscard(&out);
        return 0;
    }
    xl6OutputClose(&out);
    if ( xl6OutputHasFailed(&out) ) {
        xlSetError2(ctx, "cannot write header output", xl6OutputError(&out));
        return 0;
    }
    return 1;
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

static xvalue* xlcCreateArgArray(XL_Context* runtime, int argc, char** argv)
{
    xvalue* pArgs;
    int i;

    pArgs = xrtValueArray();
    if ( pArgs == NULL ) {
        xlSetError(runtime, "cannot create script arg array");
        return NULL;
    }
    for ( i = 0; i < argc; ++i ) {
        const char* text = argv != NULL && argv[i] != NULL ? argv[i] : "";
        if ( !xrtValueArrayAppendNew(
            pArgs,
            xrtValueString(xrtStrView(text))
        ) ) {
            xrtValueRelease(pArgs);
            xlSetError(runtime, "cannot append script argument");
            return NULL;
        }
    }
    return pArgs;
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
    xlStringListFree(&ctx->resourceFiles);
    xlFreeSymbols(ctx);
    xlLoadedLibrariesFree(ctx);
    xrtFree(ctx);
    xlRuntimeReleaseContextUnlocked();
}

/*
 * 用户入口执行期间会暂时释放全局 API 锁。执行租约保证并发销毁只会
 * 标记上下文关闭，真正释放延迟到最后一个入口返回之后。
 */
static int xlContextBeginCallUnlocked(XL_Context* ctx)
{
    if ( ctx == NULL || ctx->destroyRequested ) {
        return 0;
    }
    ctx->activeCalls += 1;
    return 1;
}

static void xlContextEndCallUnlocked(XL_Context* ctx)
{
    if ( ctx == NULL || ctx->activeCalls == 0 ) {
        return;
    }
    ctx->activeCalls -= 1;
    if ( ctx->activeCalls == 0 && ctx->destroyRequested ) {
        xlDestroyContextUnlocked(ctx);
    }
}

static void xlContextRequestDestroyUnlocked(XL_Context* ctx)
{
    if ( ctx == NULL || ctx->destroyRequested ) {
        return;
    }
    ctx->destroyRequested = 1;
    if ( ctx->activeCalls == 0 ) {
        xlDestroyContextUnlocked(ctx);
    }
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
    xlContextRequestDestroyUnlocked(ctx);
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

int xlAddResourceFile(XL_Context* ctx, const char* path)
{
    char* snapshot;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    if ( path == NULL || path[0] == '\0' ) {
        xlSetError(ctx, "resource config path is empty");
        xlApiUnlock();
        return 0;
    }
    if ( ctx->resourceFiles.count >= XLC_MAX_RESOURCE_FILES ) {
        xlSetError(ctx, "too many resource config files");
        xlApiUnlock();
        return 0;
    }
    snapshot = xlPathSnapshotFromCurrentDir(ctx, path, "cannot snapshot resource config");
    ok = snapshot != NULL && xlStringListAppend(&ctx->resourceFiles, snapshot);
    xrtFree(snapshot);
    if ( !ok ) {
        xlSetError(ctx, "cannot add resource config");
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
        uint16* pathW = xrtUtf8To16(snapshot, NULL);
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

int xlRunScriptStringArg(XL_Context* ctx, const char* source, xvalue* pArg)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    if ( !xlContextBeginCallUnlocked(ctx) ) {
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    ok = xlLoadUnitFromSource(ctx, &unit, source, "__xl_string.xl");
    if ( ok ) {
        ok = xlCompileAndRunUnit(ctx, &unit, pArg);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlContextEndCallUnlocked(ctx);
    xlApiUnlock();
    return ok;
}

int xlRunScriptFile(XL_Context* ctx, const char* path)
{
    return xlRunScriptFileArg(ctx, path, NULL);
}

int xlRunScriptFileArg(XL_Context* ctx, const char* path, xvalue* pArg)
{
    XL6_CompileUnit unit;
    XL_CwdScope cwd;
    int ok;

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    if ( !xlContextBeginCallUnlocked(ctx) ) {
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    ok = xlLoadUnitFromFile(ctx, &unit, path);
    if ( ok ) {
        ok = xlCompileAndRunUnit(ctx, &unit, pArg);
    }
    xl6CompileUnitFree(&unit);
    xlLeaveWorkDir(&cwd);
    xlContextEndCallUnlocked(ctx);
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
    if ( !xlContextBeginCallUnlocked(ctx) ) {
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    module = xlCompileCSourceTextToModule(ctx, source, XL_TCC_VFS_C_MODULE_C);
    ok = module != NULL && xlRunCModuleMainUnlocked(ctx, module, argc, argv);
    xlDestroyModuleUnlocked(module);
    xlLeaveWorkDir(&cwd);
    xlContextEndCallUnlocked(ctx);
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
    if ( !xlContextBeginCallUnlocked(ctx) ) {
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    module = xlCompileCSourceFileToModuleUnlocked(ctx, path);
    ok = module != NULL && xlRunCModuleMainUnlocked(ctx, module, argc, argv);
    xlDestroyModuleUnlocked(module);
    xlLeaveWorkDir(&cwd);
    xlContextEndCallUnlocked(ctx);
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
    if ( !xlContextBeginCallUnlocked(ctx) ) {
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    if ( !xlEnterWorkDir(ctx, &cwd) ) {
        xlContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    module = xlCompileCSourceFilesToModuleUnlocked(ctx, paths, pathCount);
    ok = module != NULL && xlRunCModuleMainUnlocked(ctx, module, argc, argv);
    xlDestroyModuleUnlocked(module);
    xlLeaveWorkDir(&cwd);
    xlContextEndCallUnlocked(ctx);
    xlApiUnlock();
    return ok;
}

int xlRunModuleMain(XL_Context* ctx, XL_Module* module, xvalue* pArg)
{
    int ok;
    int hasContextLease;
    XL6_ModuleCall call = {0};

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    hasContextLease = xlContextBeginCallUnlocked(ctx);
    if ( !hasContextLease || !xlModuleBeginCallUnlocked(module) ) {
        if ( hasContextLease ) {
            xlContextEndCallUnlocked(ctx);
        }
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    xlModuleCallAttachUnlocked(module,&call);
    ok = xlRunModuleMainUnlocked(ctx, module, pArg);
    xlModuleCallDetachUnlocked(module,&call);
    xlModuleEndCallUnlocked(module);
    xlContextEndCallUnlocked(ctx);
    xlApiUnlock();
    return ok;
}

int xlActivateModule(XL_Context* ctx, XL_Module* module, xvalue* pArg)
{
    int ok;
    int hasContextLease;
    XL6_ModuleCall call = {0};

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    hasContextLease = xlContextBeginCallUnlocked(ctx);
    if ( !hasContextLease || !xlModuleBeginCallUnlocked(module) ) {
        if ( hasContextLease ) {
            xlContextEndCallUnlocked(ctx);
        }
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    xlModuleCallAttachUnlocked(module,&call);
    ok = xlActivateModuleUnlocked(ctx, module, pArg);
    xlModuleCallDetachUnlocked(module,&call);
    xlModuleEndCallUnlocked(module);
    xlContextEndCallUnlocked(ctx);
    xlApiUnlock();
    return ok;
}

int xlDeactivateModule(XL_Context* ctx, XL_Module* module)
{
    int ok;
    int hasContextLease;
    XL6_ModuleCall call = {0};

    if ( ctx == NULL ) {
        return 0;
    }
    xlApiLock();
    hasContextLease = xlContextBeginCallUnlocked(ctx);
    if ( !hasContextLease || !xlModuleBeginCallUnlocked(module) ) {
        if ( hasContextLease ) {
            xlContextEndCallUnlocked(ctx);
        }
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx);
    xlModuleCallAttachUnlocked(module,&call);
    ok = xlDeactivateModuleUnlocked(ctx, module);
    xlModuleCallDetachUnlocked(module,&call);
    xlModuleEndCallUnlocked(module);
    xlContextEndCallUnlocked(ctx);
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
    xlModuleRequestDestroyUnlocked(module);
    xlApiUnlock();
}

XL_API int xlTccRegisterXrtEnvironment(TCCState* state, int outputType)
{
    int ok;

    if ( state == NULL ||
         outputType < TCC_OUTPUT_MEMORY ||
         outputType > TCC_OUTPUT_PREPROCESS ) {
        return 0;
    }
    xlApiLock();
    ok = xlEnsureTcc(NULL) &&
         xlConfigureTccStateEnvironment(NULL, state, outputType) &&
         xlApiAddDefaultRuntimeSymbols(state);
    xlApiUnlock();
    return ok;
}

XL_API int xlTccRegisterContextEnvironment(XL_Context* ctx, TCCState* state, int outputType)
{
    int ok;

    if ( ctx == NULL || state == NULL ||
         outputType < TCC_OUTPUT_MEMORY ||
         outputType > TCC_OUTPUT_PREPROCESS ) {
        return 0;
    }
    xlApiLock();
    xlClearLastErrorUnlocked(ctx);
    ok = xlEnsureTcc(ctx) &&
         xlConfigureTccStateEnvironment(ctx, state, outputType) &&
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
         xlAddResourceFilesToTcc(ctx, s) &&
         xlTccCompileSourceText(ctx, s, XL_TCC_VFS_XRT_C,
             "#define XRUNTIME_IMPLEMENTATION\n"
             "#include \"xruntime.h\"\n",
             "cannot compile XRT/xruntime single header");
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

static void xlcDestroyContextUnlocked(XLC_Context* ctx)
{
    XL_Context* runtime;

    if ( ctx == NULL ) {
        return;
    }
    runtime = ctx->runtime;
    ctx->runtime = NULL;
    xlcClearSourceUnlocked(ctx);
    xlStringListFree(&ctx->cSourceFiles);
    xrtFree(ctx);
    xlContextRequestDestroyUnlocked(runtime);
}

static int xlcContextBeginCallUnlocked(XLC_Context* ctx)
{
    if ( ctx == NULL || ctx->destroyRequested ) {
        return 0;
    }
    ctx->activeCalls += 1;
    return 1;
}

static void xlcContextEndCallUnlocked(XLC_Context* ctx)
{
    if ( ctx == NULL || ctx->activeCalls == 0 ) {
        return;
    }
    ctx->activeCalls -= 1;
    if ( ctx->activeCalls == 0 && ctx->destroyRequested ) {
        xlcDestroyContextUnlocked(ctx);
    }
}

XL_API void xlcDestroyContext(XLC_Context* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xlApiLock();
    if ( !ctx->destroyRequested ) {
        ctx->destroyRequested = 1;
        if ( ctx->activeCalls == 0 ) {
            xlcDestroyContextUnlocked(ctx);
        }
    }
    xlApiUnlock();
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

XL_API int xlcSetBackend(XLC_Context* ctx, XLC_Backend backend)
{
    if ( ctx == NULL || ctx->runtime == NULL ) return 0;
    if ( backend != XLC_BACKEND_XLIR ) {
        xlSetError(ctx->runtime, "only the XLIR compiler backend is available");
        return 0;
    }
    return 1;
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

XL_API int xlcAddResourceFile(XLC_Context* ctx, const char* path)
{
    return ctx != NULL ? xlAddResourceFile(ctx->runtime, path) : 0;
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
            xl6IrDumpModuleJson(&out, &ctx->unit.ir);
        } else {
            xl6IrDumpModuleText(&out, &ctx->unit.ir);
        }
        ok = xlcOutputClose(ctx->runtime, &out, "cannot write IR output");
    }
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitLoweredIr(XLC_Context* ctx, const char* outputPath, int jsonOutput)
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
    ok = xlcCompileUnlocked(ctx) && xlEnsureUnitXlirLowered(ctx->runtime, &ctx->unit) &&
         xlcOutputOpen(ctx->runtime, &out, outputPath, "cannot open lowered IR output");
    if ( ok ) {
        if ( jsonOutput ) {
            xl6IrDumpModuleJson(&out, &ctx->unit.ir);
        } else {
            xl6IrDumpModuleText(&out, &ctx->unit.ir);
        }
        ok = xlcOutputClose(ctx->runtime, &out, "cannot write lowered IR output");
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
    ok = xlcCompileUnlocked(ctx) &&
         xlEmitUnitCToFile(ctx->runtime, &ctx->unit, outputPath, NULL);
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
    ok = xlcCompileUnlocked(ctx) && xlEmitUnitHeaderToFile(
        ctx->runtime, &ctx->unit, outputPath, guardName);
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
         xlCompileUnitToExternalOutputWithC(ctx->runtime, &ctx->unit,
             outputExe, TCC_OUTPUT_EXE, "exe", NULL,
             &ctx->cSourceFiles);
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
        XL6_IrCgenOptions options;
        memset(&options, 0, sizeof(options));
        options.debugMode = 1;
        options.debugSourcePath = debugSourcePath;
        ok = xlCompileUnitToExternalOutputWithC(ctx->runtime, &ctx->unit,
            outputExe, TCC_OUTPUT_EXE, "debug exe", &options,
            &ctx->cSourceFiles);
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
    ok = xlcCompileUnlocked(ctx) && xlCompileUnitToSharedLibraryWithC(
        ctx->runtime, &ctx->unit, outputDll, &ctx->cSourceFiles);
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
    module = xlcCompileUnlocked(ctx) ? xlCompileUnitToModuleWithC(
        ctx->runtime, &ctx->unit, NULL, &ctx->cSourceFiles) : NULL;
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return module;
}

XL_API int xlcRun(XLC_Context* ctx, int argc, char** argv)
{
    XL_CwdScope cwd;
    XL_Module* module;
    xvalue* pArg;
    int ok;
    int hasRuntimeLease;

    if ( ctx == NULL || ctx->runtime == NULL ) {
        return 0;
    }
    xlApiLock();
    if ( !xlcContextBeginCallUnlocked(ctx) ) {
        xlApiUnlock();
        return 0;
    }
    hasRuntimeLease = xlContextBeginCallUnlocked(ctx->runtime);
    if ( !hasRuntimeLease ) {
        xlcContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) {
        xlContextEndCallUnlocked(ctx->runtime);
        xlcContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    pArg = xlcCreateArgArray(ctx->runtime, argc > 0 ? argc : 0, argv);
    if ( pArg == NULL ) {
        xlLeaveWorkDir(&cwd);
        xlContextEndCallUnlocked(ctx->runtime);
        xlcContextEndCallUnlocked(ctx);
        xlApiUnlock();
        return 0;
    }
    module = xlcCompileUnlocked(ctx) ? xlCompileUnitToModuleWithC(
        ctx->runtime, &ctx->unit, NULL, &ctx->cSourceFiles) : NULL;
    ok = module != NULL && xlRunModuleMainUnlocked(ctx->runtime, module, pArg);
    xlDestroyModuleUnlocked(module);
    xrtValueRelease(pArg);
    xlLeaveWorkDir(&cwd);
    xlContextEndCallUnlocked(ctx->runtime);
    xlcContextEndCallUnlocked(ctx);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitXlirC(XLC_Context* ctx, const char* outputPath)
{
    XL_CwdScope cwd;
    int ok;
    if ( ctx == NULL || ctx->runtime == NULL ) return 0;
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) { xlApiUnlock(); return 0; }
    ok = xlcCompileUnlocked(ctx) && xlEmitUnitCToFile(
        ctx->runtime, &ctx->unit, outputPath, NULL);
    xlLeaveWorkDir(&cwd);
    xlApiUnlock();
    return ok;
}

XL_API int xlcEmitXlirHeader(XLC_Context* ctx, const char* outputPath, const char* guardName)
{
    XL_CwdScope cwd;
    XL6_Output out;
    int ok;
    if ( ctx == NULL || ctx->runtime == NULL || guardName == NULL || guardName[0] == '\0' ) return 0;
    xlApiLock();
    xlClearLastErrorUnlocked(ctx->runtime);
    if ( !xlEnterWorkDir(ctx->runtime, &cwd) ) { xlApiUnlock(); return 0; }
    ok = xlcCompileUnlocked(ctx) && xlEnsureUnitXlirLowered(ctx->runtime, &ctx->unit);
    if ( ok && !xl6OutputOpenFile(&out, outputPath) ) {
        xlSetError2(ctx->runtime, "cannot open XLIR C header output", outputPath);
        ok = 0;
    }
    if ( ok ) {
        ok = xl6IrCgenEmitHeader(&out, &ctx->unit.ir, guardName, &ctx->unit.parse.diags);
        if ( xl6OutputHasFailed(&out) ) {
            xlSetError2(ctx->runtime, "cannot write XLIR C header output", xl6OutputError(&out));
            ok = 0;
        } else if ( !ok ) xlSetCompileDiagError(ctx->runtime, &ctx->unit);
        if ( ok ) {
            xl6OutputClose(&out);
            if ( xl6OutputHasFailed(&out) ) {
                xlSetError2(ctx->runtime, "cannot write XLIR C header output", xl6OutputError(&out));
                ok = 0;
            }
        } else {
            xl6OutputDiscard(&out);
        }
    }
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
