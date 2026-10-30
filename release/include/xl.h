#ifndef XL_PUBLIC_API_H
#define XL_PUBLIC_API_H

#include "xl_version.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
# if defined(XL_STATIC_EMBED)
#  define XL_API
# elif defined(XL_BUILD_DLL)
#  define XL_API __declspec(dllexport)
# else
#  define XL_API __declspec(dllimport)
# endif
#elif defined(__GNUC__) || defined(__clang__)
# define XL_API __attribute__((visibility("default")))
#else
# define XL_API
#endif

#ifndef XL_API_VERSION
#define XL_API_VERSION 1u
#endif

typedef struct XL_Context XL_Context;
typedef struct XL_Module XL_Module;
typedef struct TCCState TCCState;
typedef struct xvalue xvalue;

typedef struct XL_ContextOptions {
    unsigned int size;
    unsigned int version;
    const char* workDir;
    const char* tempDir;
    const char* tccDllPath;
    const char* tccLibPath;
    int saveTemps;
} XL_ContextOptions;

/* Safe before runtime initialization. The text is immutable static storage. */
XL_API const char* xlGetVersionString(void);
XL_API unsigned int xlGetVersionCode(void);

XL_API XL_ContextOptions xlDefaultContextOptions(void);
XL_API int xlRuntimeInit(void);
XL_API void xlRuntimeUnit(void);
typedef struct XL_RuntimeDrainResult { int ready; int ok; } XL_RuntimeDrainResult;
/* Retire queued modules without consuming runtime/context references or
 * closing live module handles. Preserve failure independently of readiness.
 * -1 waits; 0 is one bounded turn; positive milliseconds bound retries only.
 * Stop creating/using contexts while performing process shutdown. External
 * owners remain genuine roots; same-driver callback reentry returns BUSY. */
XL_API XL_RuntimeDrainResult xlRuntimeDrainRetiringChecked(long long timeoutMilliseconds);

XL_API XL_Context* xlCreateContext(const XL_ContextOptions* options);
XL_API void xlDestroyContext(XL_Context* ctx);

XL_API int xlSetWorkDir(XL_Context* ctx, const char* path);
XL_API int xlSetTempDir(XL_Context* ctx, const char* path);
XL_API int xlSetSaveTemps(XL_Context* ctx, int enabled);

XL_API int xlAddIncludePath(XL_Context* ctx, const char* path);
XL_API int xlAddLibraryPath(XL_Context* ctx, const char* path);
XL_API int xlAddLinkLibrary(XL_Context* ctx, const char* nameOrPath);
XL_API int xlAddResourceFile(XL_Context* ctx, const char* path);
XL_API int xlLoadDependencyLibrary(XL_Context* ctx, const char* path);
XL_API int xlRegisterCFunction(XL_Context* ctx, const char* symbolName, void* fn);

XL_API int xlRunScriptString(XL_Context* ctx, const char* source);
XL_API int xlRunScriptFile(XL_Context* ctx, const char* path);
XL_API int xlRunScriptStringArg(XL_Context* ctx, const char* source, xvalue* arg);
XL_API int xlRunScriptFileArg(XL_Context* ctx, const char* path, xvalue* arg);
XL_API XL_Module* xlCompileScriptStringToModule(XL_Context* ctx, const char* source);
XL_API XL_Module* xlCompileScriptFileToModule(XL_Context* ctx, const char* path);
XL_API XL_Module* xlCompileCSourceStringToModule(XL_Context* ctx, const char* source);
XL_API XL_Module* xlCompileCSourceFileToModule(XL_Context* ctx, const char* path);
XL_API XL_Module* xlCompileCSourceFilesToModule(XL_Context* ctx, const char** paths, int pathCount);
XL_API int xlRunCSourceString(XL_Context* ctx, const char* source, int argc, char** argv);
XL_API int xlRunCSourceFile(XL_Context* ctx, const char* path, int argc, char** argv);
XL_API int xlRunCSourceFiles(XL_Context* ctx, const char** paths, int pathCount, int argc, char** argv);
XL_API int xlActivateModule(XL_Context* ctx, XL_Module* module, xvalue* arg);
XL_API int xlDeactivateModule(XL_Context* ctx, XL_Module* module);
XL_API int xlRunModuleMain(XL_Context* ctx, XL_Module* module, xvalue* arg);
XL_API void* xlModuleGetSymbol(XL_Module* module, const char* symbolName);
XL_API void xlDestroyModule(XL_Module* module);
XL_API int xlTccRegisterXrtEnvironment(TCCState* state, int outputType);
XL_API int xlTccRegisterContextEnvironment(XL_Context* ctx, TCCState* state, int outputType);
XL_API int xlTccImpdef(const char* dllPath, const char* outputDef);
#if !defined(XL_RUNTIME_ONLY_API)
XL_API int xlCompileScriptStringToExe(XL_Context* ctx, const char* source, const char* outputExe);
XL_API int xlCompileScriptFileToExe(XL_Context* ctx, const char* path, const char* outputExe);
XL_API int xlCompileCSourceFileToExe(XL_Context* ctx, const char* path, const char* outputExe);
XL_API int xlCompileCSourceFilesToExe(XL_Context* ctx, const char** paths, int pathCount, const char* outputExe);
XL_API int xlCompileCSourceFileToSharedLibrary(XL_Context* ctx, const char* path, const char* outputDll);
XL_API int xlCompileCSourceFilesToSharedLibrary(XL_Context* ctx, const char** paths, int pathCount, const char* outputDll);
#endif

XL_API const char* xlGetLastError(XL_Context* ctx);
XL_API void xlClearLastError(XL_Context* ctx);
XL_API int xlGetLastExitCode(XL_Context* ctx);

#ifdef __cplusplus
}
#endif

#endif
