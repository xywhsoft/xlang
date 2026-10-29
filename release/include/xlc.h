#ifndef XLC_PUBLIC_API_H
#define XLC_PUBLIC_API_H

#include "xl.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XLC_API
#define XLC_API XL_API
#endif

#ifndef XLC_API_VERSION
#define XLC_API_VERSION XL_API_VERSION
#endif

/* A context accepts at most this many independent C translation units. */
#ifndef XLC_MAX_C_SOURCE_FILES
#define XLC_MAX_C_SOURCE_FILES 256
#endif

typedef struct XLC_Context XLC_Context;
typedef XL_ContextOptions XLC_ContextOptions;

/* XLIR is the only compiler backend. */
typedef enum XLC_Backend {
    XLC_BACKEND_XLIR = 0
} XLC_Backend;

XLC_API XLC_ContextOptions xlcDefaultContextOptions(void);
XLC_API XLC_Context* xlcCreateContext(const XLC_ContextOptions* options);
XLC_API void xlcDestroyContext(XLC_Context* ctx);

XLC_API int xlcSetWorkDir(XLC_Context* ctx, const char* path);
XLC_API int xlcSetTempDir(XLC_Context* ctx, const char* path);
XLC_API int xlcSetSaveTemps(XLC_Context* ctx, int enabled);
XLC_API int xlcSetBackend(XLC_Context* ctx, XLC_Backend backend);
XLC_API int xlcAddIncludePath(XLC_Context* ctx, const char* path);
XLC_API int xlcAddLibraryPath(XLC_Context* ctx, const char* path);
XLC_API int xlcAddLinkLibrary(XLC_Context* ctx, const char* nameOrPath);
XLC_API int xlcAddResourceFile(XLC_Context* ctx, const char* path);
XLC_API int xlcAddCSourceFile(XLC_Context* ctx, const char* path);
XLC_API int xlcLoadDependencyLibrary(XLC_Context* ctx, const char* path);
XLC_API int xlcRegisterCFunction(XLC_Context* ctx, const char* symbolName, void* fn);

XLC_API int xlcAddSourceFile(XLC_Context* ctx, const char* path);
XLC_API int xlcAddSourceString(XLC_Context* ctx, const char* virtualPath, const char* source);
/* Context-owned UTF-8 snapshots for entry and recursive .xl imports. Input
 * is copied; no NUL, malformed UTF-8, >8 MiB/file, >32 MiB total or >64 files.
 * Relative paths resolve against workDir at registration, using lexical
 * absolute paths and Windows case-insensitive comparison. String entry text
 * remains authoritative; overlays apply to its imports. Failed updates retain
 * the previous snapshot and compiled unit. Remove/clear restore disk fallback.
 * These APIs do not override C sources, manifests or resource files. */
XLC_API int xlcSetSourceOverlay(XLC_Context* ctx, const char* path, const char* source, size_t size);
XLC_API int xlcRemoveSourceOverlay(XLC_Context* ctx, const char* path);
XLC_API int xlcClearSourceOverlays(XLC_Context* ctx);
XLC_API int xlcCompile(XLC_Context* ctx);
XLC_API int xlcCheck(XLC_Context* ctx, const char* outputPath, int jsonOutput);
/* JSON diagnostics plus schema-1 snapshots of loaded source/manifest files.
 * This opt-in check is bounded and rejects unstable or non-UTF-8 inputs.
 * It does not observe resolution candidates, native ABI files, or C inputs. */
XLC_API int xlcCheckWithSources(XLC_Context* ctx, const char* outputPath);
/* Same validated check + source/resolution reports, with opt-in semantic facts. */
XLC_API int xlcCheckWithIndex(XLC_Context* ctx, const char* outputPath);
/* Fresh verified/lowered native C declarations with the same source/resolution
 * batch. Schema-1 cBridge; failed queries contain no stale declarations.
 * Query header is capped at 1 MiB. Does not observe the C preprocessor graph
 * or export a complete TCC environment. Inputs/outputs follow check APIs. */
XLC_API int xlcCBridge(XLC_Context* ctx, const char* outputPath);
/* Opt-in configuration operations shared with TCC and exact builtin header
 * bytes. shared is 0 or 1. Does not prove a complete preprocessing graph or
 * Clang/TCC equivalence; failure clears both declarations and environment. */
XLC_API int xlcCBridgeWithEnvironment(XLC_Context* ctx, const char* outputPath, int shared);
/* Query-only completion at a UTF-8 byte offset after the optional BOM.
 * JSON includes query completeness and exact source/resolution reports.
 * Success does not claim the input compilation is valid. */
XLC_API int xlcComplete(XLC_Context* ctx,const char* queryPath,size_t byteOffset,const char* outputPath);
XLC_API int xlcEmitTokens(XLC_Context* ctx, const char* outputPath, int jsonOutput);
XLC_API int xlcEmitAst(XLC_Context* ctx, const char* outputPath, int jsonOutput);
/* Bounded schema-1 JSON for the entry syntax tree, with real grammar/token
 * ranges. This is a parser API, not a semantic symbol/dependency query. */
XLC_API int xlcEmitAstWithRanges(XLC_Context* ctx, const char* outputPath);
XLC_API int xlcEmitIr(XLC_Context* ctx, const char* outputPath, int jsonOutput);
/* Emits only verified CFG-lowered XLIR; useful for inspecting the Cgen input. */
XLC_API int xlcEmitLoweredIr(XLC_Context* ctx, const char* outputPath, int jsonOutput);
/* Emits with the selected backend; a new context defaults to verified XLIR. */
XLC_API int xlcEmitC(XLC_Context* ctx, const char* outputPath);
/* Explicit module-form XLIR output (no process main wrapper). */
XLC_API int xlcEmitXlirC(XLC_Context* ctx, const char* outputPath);
/* Emits export declarations derived only from lowered XLIR. */
XLC_API int xlcEmitXlirHeader(XLC_Context* ctx, const char* outputPath, const char* guardName);
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
