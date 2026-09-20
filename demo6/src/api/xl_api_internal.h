#ifndef XL_API_INTERNAL_H
#define XL_API_INTERNAL_H

#include <stdbool.h>

#include "include/xl.h"
#include "libtcc.h"
#include "xl_channel_bridge.h"
#include "xl_future_bridge.h"
#include "xl_foreign_error.h"
#include "xl_task_group_bridge.h"

typedef struct xl6_protocol_witness xl6_protocol_witness;
typedef struct xl6_protocol_entry xl6_protocol_entry;

XL_API bool xl6ProtocolWitnessValidate(const xl6_protocol_witness* witness);
XL_API const xl6_protocol_entry* xl6ProtocolWitnessFind(
    const xl6_protocol_witness* witness,
    const char* name,
    unsigned int nameSize,
    unsigned long long signatureId);
XL_API bool xl6ProtocolRegister(const xl6_protocol_witness* witness);
XL_API bool xl6ProtocolUnregister(const xl6_protocol_witness* witness);
XL_API const xl6_protocol_witness* xl6ProtocolLookup(
    unsigned int protocolTypeId,
    unsigned int concreteTypeId);

struct XLC_Context;

typedef enum XLC_Backend {
    XLC_BACKEND_XLIR = 0,
    XLC_BACKEND_LEGACY = 1
} XLC_Backend;

XL_ContextOptions xlcDefaultContextOptions(void);
struct XLC_Context* xlcCreateContext(const XL_ContextOptions* options);
void xlcDestroyContext(struct XLC_Context* ctx);
int xlcSetWorkDir(struct XLC_Context* ctx, const char* path);
int xlcSetTempDir(struct XLC_Context* ctx, const char* path);
int xlcSetSaveTemps(struct XLC_Context* ctx, int enabled);
int xlcSetBackend(struct XLC_Context* ctx, XLC_Backend backend);
int xlcAddIncludePath(struct XLC_Context* ctx, const char* path);
int xlcAddLibraryPath(struct XLC_Context* ctx, const char* path);
int xlcAddLinkLibrary(struct XLC_Context* ctx, const char* nameOrPath);
int xlcAddResourceFile(struct XLC_Context* ctx, const char* path);
int xlcAddCSourceFile(struct XLC_Context* ctx, const char* path);
int xlcLoadDependencyLibrary(struct XLC_Context* ctx, const char* path);
int xlcRegisterCFunction(struct XLC_Context* ctx, const char* symbolName, void* fn);
int xlcAddSourceFile(struct XLC_Context* ctx, const char* path);
int xlcAddSourceString(struct XLC_Context* ctx, const char* virtualPath, const char* source);
int xlcCompile(struct XLC_Context* ctx);
int xlcCheck(struct XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitTokens(struct XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitAst(struct XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitIr(struct XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitLoweredIr(struct XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitC(struct XLC_Context* ctx, const char* outputPath);
int xlcEmitXlirC(struct XLC_Context* ctx, const char* outputPath);
int xlcEmitDebugC(struct XLC_Context* ctx, const char* outputPath, const char* debugSourcePath);
int xlcEmitHeader(struct XLC_Context* ctx, const char* outputPath, const char* guardName);
int xlcEmitAbi(struct XLC_Context* ctx, const char* outputPath, int jsonOutput);
int xlcEmitExe(struct XLC_Context* ctx, const char* outputExe);
int xlcEmitDebugExe(struct XLC_Context* ctx, const char* outputExe, const char* debugSourcePath);
XL_Context* xlcRuntimeContext(struct XLC_Context* ctx);
XL_Module* xlcRelocateModule(struct XLC_Context* ctx);
int xlcRun(struct XLC_Context* ctx, int argc, char** argv);
const char* xlcGetLastError(struct XLC_Context* ctx);
void xlcClearLastError(struct XLC_Context* ctx);
int xlcGetLastExitCode(struct XLC_Context* ctx);
int xlcEmitSharedLibrary(struct XLC_Context* ctx, const char* outputDll);

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
typedef void (*XL_TccListSymbolsProc)(TCCState* s, void* ctx, void (*symbolCb)(void* ctx, const char* name, const void* value));
typedef void (*XL_TccSetBacktraceFuncProc)(TCCState* s, void* userdata, TCCBtFunc* func);

typedef struct XL_TccApi {
    int ready;
    char* path;
#ifdef _WIN32
    void* module;
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

extern XL_TccApi xl__tcc;

int xlApiAddTccSymbol(TCCState* state, const char* name, const void* value);
int xlApiAddDefaultRuntimeSymbols(TCCState* state);

#endif
