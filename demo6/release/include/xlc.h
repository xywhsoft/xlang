#ifndef XLC_PUBLIC_API_H
#define XLC_PUBLIC_API_H

#include "xl.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XLC_API
#define XLC_API XL_API
#endif

#ifndef XLC_API_VERSION
#define XLC_API_VERSION XL_API_VERSION
#endif

typedef struct XLC_Context XLC_Context;
typedef XL_ContextOptions XLC_ContextOptions;

XLC_API XLC_ContextOptions xlcDefaultContextOptions(void);
XLC_API XLC_Context* xlcCreateContext(const XLC_ContextOptions* options);
XLC_API void xlcDestroyContext(XLC_Context* ctx);

XLC_API int xlcSetWorkDir(XLC_Context* ctx, const char* path);
XLC_API int xlcSetTempDir(XLC_Context* ctx, const char* path);
XLC_API int xlcSetSaveTemps(XLC_Context* ctx, int enabled);
XLC_API int xlcAddIncludePath(XLC_Context* ctx, const char* path);
XLC_API int xlcAddLibraryPath(XLC_Context* ctx, const char* path);
XLC_API int xlcAddLinkLibrary(XLC_Context* ctx, const char* nameOrPath);
XLC_API int xlcAddCSourceFile(XLC_Context* ctx, const char* path);
XLC_API int xlcLoadDependencyLibrary(XLC_Context* ctx, const char* path);
XLC_API int xlcRegisterCFunction(XLC_Context* ctx, const char* symbolName, void* fn);

XLC_API int xlcAddSourceFile(XLC_Context* ctx, const char* path);
XLC_API int xlcAddSourceString(XLC_Context* ctx, const char* virtualPath, const char* source);
XLC_API int xlcCompile(XLC_Context* ctx);
XLC_API int xlcCheck(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XLC_API int xlcEmitTokens(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XLC_API int xlcEmitAst(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XLC_API int xlcEmitIr(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XLC_API int xlcEmitC(XLC_Context* ctx, const char* outputPath);
XLC_API int xlcEmitDebugC(XLC_Context* ctx, const char* outputPath, const char* debugSourcePath);
XLC_API int xlcEmitHeader(XLC_Context* ctx, const char* outputPath, const char* guardName);
XLC_API int xlcEmitAbi(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XLC_API int xlcEmitExe(XLC_Context* ctx, const char* outputExe);
XLC_API int xlcEmitDebugExe(XLC_Context* ctx, const char* outputExe, const char* debugSourcePath);
XLC_API int xlcEmitSharedLibrary(XLC_Context* ctx, const char* outputDll);
XLC_API XL_Module* xlcRelocateModule(XLC_Context* ctx);
XLC_API int xlcRun(XLC_Context* ctx, int argc, char** argv);

XLC_API XL_Context* xlcRuntimeContext(XLC_Context* ctx);
XLC_API const char* xlcGetLastError(XLC_Context* ctx);
XLC_API void xlcClearLastError(XLC_Context* ctx);
XLC_API int xlcGetLastExitCode(XLC_Context* ctx);

#ifdef __cplusplus
}
#endif

#endif
