#ifndef XLANG_IDE_LANGUAGE_CHECK_H
#define XLANG_IDE_LANGUAGE_CHECK_H
#include "services/toolchain.h"
typedef struct xide_language_check xide_language_check;
typedef struct xide_language_snapshot { const char *path,*text; int version; } xide_language_snapshot;
/* Immutable 64-file, 8 MiB/file, 32 MiB batch; entry must be present. Never
   writes original files. Result documents carry per-file UTF-16 diagnostics
   and versions for open files. Disk inputs are verified before publication and while the result is retained. */
xide_language_check* xideLanguageCheckStartSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,xide_model_error* error);

/* Independent cursor query over the same immutable, guarded input contract.
   UTF-16 position must identify a scalar boundary in the query overlay.
   Result is a CompletionList with whole-token textEdit, never diagnostics. */
xide_language_check* xideLanguageCheckStartCompletionSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const char* queryPath,int line,int character,xide_model_error* error);

/* Worker-only C ABI query: disk entry is allowed, open .xl overlays remain
   copied and guarded. Options are real compiler include/C-source inputs. */
typedef struct xide_language_bridge_options {
 const char* const* includes; size_t includeCount;
 const char* const* cSources; size_t cSourceCount; int shared;
 int (*cancelled)(void*); void* user;
} xide_language_bridge_options;
xide_language_check* xideLanguageBridgeRun(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const xide_language_bridge_options* options,xide_model_error* error);
/* Transfer once: real cBridge, verified sourceFiles and optional compact
   sourceBindings/sourceBindingsValid. Caller releases the owned resource DOM. */
xvalue* xideLanguageBridgeTake(xide_language_check* check);
/* Worker-only; permanent invalidation, no compiler rerun. */
int xideLanguageBridgeGuard(xide_language_check* check,xide_model_error* error);

enum xide_check_state { XIDE_CHECK_RUNNING,XIDE_CHECK_READY,XIDE_CHECK_FAILED,XIDE_CHECK_CANCELLED };
xide_language_check* xideLanguageCheckStart(xide_toolchains* cache,const char* compiler,const char* directory,const char* path,const char* savedText,int version,xide_model_error* error);
enum xide_check_state xideLanguageCheckState(xide_language_check* check,char* message,size_t capacity);
/* Caller owns the returned XRT value. Both modes contain UTF-16 ranges and
   entry version; snapshot mode also groups diagnostics and versions per file.
   Neither mode is a project-wide multi-entry analysis. */
xvalue* xideLanguageCheckResult(xide_language_check* check);
/* Same guarded batch; Location or JSON null, NULL if unavailable/invalidated. */
xvalue* xideLanguageCheckDefinition(xide_language_check* check,const char* path,int version,int line,int character);
xvalue* xideLanguageCheckHover(xide_language_check* check,const char* path,int version,int line,int character);
xvalue* xideLanguageCheckDocumentSymbols(xide_language_check* check,const char* path,int version);
/* Snapshot results remain guarded by the worker until cancellation/destruction.
   Polling interval is 1 s plus sweep duration; invalidation is permanent.
   No compiler process is rerun by the guard. Saved checks have no guard. */
enum xide_guard_state { XIDE_GUARD_NONE,XIDE_GUARD_RUNNING,XIDE_GUARD_INVALID };
enum xide_guard_state xideLanguageCheckGuardState(xide_language_check* check,char* message,size_t capacity);
/* Nonblocking retirement: Destroy can be called after WorkerFinished without
   waiting for disk work on the owner thread. Cache must outlive the worker. */
int xideLanguageCheckWorkerFinished(xide_language_check* check);
/* Caller frees the copied process-local package environment; NULL is unset. */
int xideLanguageCheckPackagePath(char** out,xide_model_error* error);
void xideLanguageCheckCancel(xide_language_check* check);
void xideLanguageCheckDestroy(xide_language_check* check);
#endif
