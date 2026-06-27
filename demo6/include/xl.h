#ifndef XL_PUBLIC_API_H
#define XL_PUBLIC_API_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
# if defined(XL_BUILD_DLL)
#  define XL_API __declspec(dllexport)
# else
#  define XL_API __declspec(dllimport)
# endif
#else
# define XL_API
#endif

#ifndef XL_API_VERSION
#define XL_API_VERSION 1u
#endif

typedef struct XL_Context XL_Context;
typedef struct xvalue_struct* xvalue;

typedef struct XL_ContextOptions {
    unsigned int size;
    unsigned int version;
    const char* workDir;
    const char* tempDir;
    const char* tccDllPath;
    const char* tccLibPath;
    int saveTemps;
} XL_ContextOptions;

XL_API XL_ContextOptions xlDefaultContextOptions(void);
XL_API int xlRuntimeInit(void);
XL_API void xlRuntimeUnit(void);

XL_API XL_Context* xlCreateContext(const XL_ContextOptions* options);
XL_API void xlDestroyContext(XL_Context* ctx);

XL_API int xlSetWorkDir(XL_Context* ctx, const char* path);
XL_API int xlSetTempDir(XL_Context* ctx, const char* path);
XL_API int xlSetSaveTemps(XL_Context* ctx, int enabled);

XL_API int xlAddIncludePath(XL_Context* ctx, const char* path);
XL_API int xlAddLibraryPath(XL_Context* ctx, const char* path);
XL_API int xlAddLinkLibrary(XL_Context* ctx, const char* nameOrPath);
XL_API int xlLoadDependencyLibrary(XL_Context* ctx, const char* path);
XL_API int xlRegisterCFunction(XL_Context* ctx, const char* symbolName, void* fn);

XL_API int xlRunScriptString(XL_Context* ctx, const char* source);
XL_API int xlRunScriptFile(XL_Context* ctx, const char* path);
XL_API int xlRunScriptStringArg(XL_Context* ctx, const char* source, xvalue arg);
XL_API int xlRunScriptFileArg(XL_Context* ctx, const char* path, xvalue arg);
#if !defined(XL_RUNTIME_ONLY_API)
XL_API int xlCompileScriptStringToExe(XL_Context* ctx, const char* source, const char* outputExe);
XL_API int xlCompileScriptFileToExe(XL_Context* ctx, const char* path, const char* outputExe);
#endif

XL_API const char* xlGetLastError(XL_Context* ctx);
XL_API void xlClearLastError(XL_Context* ctx);

#ifdef __cplusplus
}
#endif

#endif
