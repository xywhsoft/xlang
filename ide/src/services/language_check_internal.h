#ifndef XIDE_LANGUAGE_CHECK_INTERNAL_H
#define XIDE_LANGUAGE_CHECK_INTERNAL_H
#include "services/language_check.h"
#include "services/semantic_definition.h"
#include <windows.h>
struct xide_language_check {
 CRITICAL_SECTION lock; HANDLE worker; volatile LONG cancel;
 xide_toolchains* cache; char *compiler,*directory,*path,*text;
 int version; enum xide_check_state state; char error[512]; xvalue* result;
 xide_language_snapshot* snapshots; size_t snapshotCount;
 char* queryPath; int queryOffset,queryVersion;
 int nativeSemantic; void (*wake)(void*); void* wakeUser;
 int nativeBridge; const char* const* bridgeArgs; size_t bridgeArgCount;
 int (*externalCancel)(void*); void* externalUser;
 char* packagePath; int packagePathCaptured;
 HANDLE stopEvent; enum xide_guard_state guardState; char guardError[512];
 xvalue *guardFiles,*guardResolution; xide_toolchain_stamp guardToolchain;
 xide_semantic_definitions* definitions;
#ifdef XLS_COMPILER_XRT
 xls_context* context;
 int configuredEntry;
 xls_text* snapshotTexts[64]; char* snapshotKeys[64]; uint64_t snapshotIdentities[64];
 int retainedSnapshots; char* semanticTarget; xls_worker* semanticActor;
 xls_check_stats semanticStats; xls_completion_stats completionStats; uint64_t semanticThread,captureThread;
#endif
};
int xideLanguageCheckRunCompiler(xide_language_check* check,const char* const* args,size_t count,size_t limit,char** output,int* exitCode,xide_model_error* error);
int xideLanguageSnapshotGuard(xide_language_check* check,xide_model_error* error);
int xideLanguageSnapshotRun(xide_language_check* check,xvalue** result,xide_model_error* error);
#endif
