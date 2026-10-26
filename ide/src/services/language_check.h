#ifndef XLANG_IDE_LANGUAGE_CHECK_H
#define XLANG_IDE_LANGUAGE_CHECK_H
#include "services/toolchain.h"
#include "analysis/context.h"
typedef struct xide_language_check xide_language_check;
typedef struct xide_language_snapshot { const char *path,*text; int version; } xide_language_snapshot;
typedef struct xide_language_analysis_options {
 int nativeBackend; void (*wake)(void*); void* user;
} xide_language_analysis_options;
typedef xide_language_analysis_options xide_language_completion_options;
/* Worker-only shared disk proof reader. Validates handle/name identity,
 * timestamps, raw/post-BOM hashes and UTF-8; returns malloc-owned LF text.
 * Optional original receives malloc-owned post-BOM bytes with original EOLs.
 * Does not establish semantic/dependency freshness. */
char* xideLanguageReadDiskProof(const char* path,const xvalue* input,
 int (*cancelled)(void*),void* user,char** original,xide_model_error* error);
/* Immutable 64-file, 8 MiB/file, 32 MiB batch; entry must be present. Never
   writes original files. Result documents carry per-file UTF-16 diagnostics
   and versions for open files. Disk inputs are verified before publication and while the result is retained. */
xide_language_check* xideLanguageCheckStartSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,xide_model_error* error);
xide_language_check* xideLanguageCheckStartSnapshotsEx(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const xide_language_analysis_options* options,xide_model_error* error);

/* Independent cursor query over the same immutable, guarded input contract.
   UTF-16 position must identify a scalar boundary in the query overlay.
   Result is a CompletionList with whole-token textEdit, never diagnostics. */
xide_language_check* xideLanguageCheckStartCompletionSnapshots(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const char* queryPath,int line,int character,xide_model_error* error);
/* Native backend requires the standalone compiler-XRT build. The callback
   only wakes an owner, may run on the worker, and must not reenter/destroy the
   check. Its user data outlives Destroy. CLI remains an explicit oracle. */
xide_language_check* xideLanguageCheckStartCompletionSnapshotsEx(xide_toolchains* cache,const char* compiler,const char* directory,const char* entry,const xide_language_snapshot* documents,size_t count,const char* queryPath,int line,int character,const xide_language_completion_options* options,xide_model_error* error);
#ifdef XLS_COMPILER_XRT
#include "analysis/worker.h"
/* Standalone native host only. Captures immutable text references, labels and
   wire versions without copying/revalidating entire bodies or doing physical
   alias I/O on the caller. target is the entry's document key. A static check
   needs actor; cursor mode uses a dedicated foreground actor when supplied,
   retaining exact projections across queries. NULL keeps one-shot cursor
   compatibility. queryPath/position identify its query overlay.
   actor and callback data must outlive Destroy. Disk/alias checks run off-owner. */
xide_language_check* xideLanguageCheckStartRetained(xide_toolchains* cache,const char* compiler,
 const char* directory,xls_worker* actor,const xls_analysis_document* documents,size_t count,
 const char* target,const char* queryPath,int line,int character,
 const xide_language_analysis_options* options,xide_model_error* error);
/* Same retained host boundary with an independently retained native context.
   The open target must match context.entry, and directory must match the
   context's build directory. Unsupported frontend options reject before
   starting a worker. Detached input proofs retain the context as well;
   explicit package inputs never fall back to the process environment.
   NULL preserves the existing host API, including global environment guards. */
xide_language_check* xideLanguageCheckStartRetainedInContext(xls_context* context,
 xide_toolchains* cache,const char* compiler,const char* directory,xls_worker* actor,
 const xls_analysis_document* documents,size_t count,const char* target,
 const char* queryPath,int line,int character,const xide_language_analysis_options* options,
 xide_model_error* error);
/* Configured project root with an open triggering document. context.entry may
   be another overlay or an unopened disk entry; disk reads/proofs stay on the
   worker. The reply version belongs to trigger. An open entry uses the actor;
   a disk entry currently uses the original one-shot native frontend. */
xide_language_check* xideLanguageCheckStartConfigured(xls_context* context,
 xide_toolchains* cache,const char* compiler,xls_worker* actor,
 const xls_analysis_document* documents,size_t count,const char* trigger,
 const char* queryPath,int line,int character,const xide_language_analysis_options* options,
 xide_model_error* error);
/* Immutable context match, without disk work; caller keeps check alive. */
int xideLanguageCheckMatchesContext(const xide_language_check* check,const xls_context* context);
#include "services/semantic_reference_shard.h"
/* Dedicated project-query capture,never completion or ordinary diagnostics.
 * A NULL keyPath captures only a shard. trigger remains an open document even
 * when context.entry is an unopened member. Uses the original checked source
 * and publication guards. NULL key with a successful capture is no symbol. */
xide_language_check* xideLanguageCheckStartReferenceCapture(xls_context* context,
 xide_toolchains* cache,const char* compiler,xls_worker* actor,
 const xls_analysis_document* documents,size_t count,const char* trigger,
 const char* keyPath,int line,int character,const xide_language_analysis_options* options,xide_model_error* error);
int xideLanguageCheckCaptureReferences(xide_language_check* check,xide_reference_shard** shard,xide_reference_key** key);
/* Independent native signature query. Mode/options are fixed before starting
   the adapter thread. Configured roots may be unopened; trigger stays open. */
xide_language_check* xideLanguageCheckStartSignature(xls_context* context,int configured,
 xide_toolchains* cache,const char* compiler,const char* directory,xls_worker* actor,
 const xls_analysis_document* documents,size_t count,const char* trigger,
 const char* queryPath,int line,int character,unsigned flags,
 const xide_language_analysis_options* options,xide_model_error* error);
#endif

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
/* Same atomic guarded lookup, with Location.xlang={schema:1,document:...}.
   Only the actual target file's verified metadata is copied; no diagnostic
   arrays, source bodies, whole report clone or filesystem access. Owned result
   outlives check/cache. Missing actual target proof fails rather than emitting
   an unverified Location. JSON null remains a valid no-definition result. */
xvalue* xideLanguageCheckDefinitionWithProof(xide_language_check* check,const char* path,int version,int line,int character);
xvalue* xideLanguageCheckHover(xide_language_check* check,const char* path,int version,int line,int character);
xvalue* xideLanguageCheckDocumentSymbols(xide_language_check* check,const char* path,int version);
xvalue* xideLanguageCheckDocumentHighlights(xide_language_check* check,const char* path,int version,int line,int character);
/* Snapshot results remain guarded by the worker until cancellation/destruction.
   Polling interval is 1 s plus sweep duration; invalidation is permanent.
   No compiler process is rerun by the guard. Saved checks have no guard. */
enum xide_guard_state { XIDE_GUARD_NONE,XIDE_GUARD_RUNNING,XIDE_GUARD_INVALID };
enum xide_guard_state xideLanguageCheckGuardState(xide_language_check* check,char* message,size_t capacity);
/* Detached immutable input proof for diagnostics retained across later checks.
   Captures only verified reports/configuration, without AST, IR, navigation,
   document text, a check worker or a compiler-cache dependency. Capture does
   no filesystem I/O; caller owns a reference. Validation is worker-only and
   reuses the exact snapshot guard rules. A cancelled validation does not
   poison the proof; an observed input failure permanently invalidates it. */
typedef struct xide_language_input_guard xide_language_input_guard;
xide_language_input_guard* xideLanguageCheckCaptureInputGuard(xide_language_check* check,xide_model_error* error);
xide_language_input_guard* xideLanguageInputGuardRetain(xide_language_input_guard* guard);
void xideLanguageInputGuardRelease(xide_language_input_guard* guard);
int xideLanguageInputGuardValidate(xide_language_input_guard* guard,int (*cancelled)(void*),void* user,xide_model_error* error);
/* Borrowed immutable actual-input file reports, not all captured overlays.
   Valid until the caller releases its reference; safe across validations. */
const xvalue* xideLanguageInputGuardFiles(const xide_language_input_guard* guard);
#ifdef XLS_COMPILER_XRT
/* Borrowed complete immutable resolution candidates,including absent paths.
   Same lifetime as Files;no sorting,canonicalization,allocation or I/O. Native
   reverse indexing applies the existing lexical policy and owns its keys. */
const xvalue* xideLanguageInputGuardProbes(const xide_language_input_guard* guard);
#endif
/* Immutable semantic scope equality, without I/O or allocation. Distinct
   explicit contexts must not contribute to one file's merged diagnostics.
   Both default/legacy guards are equal in scope. NULL guards are invalid.
   Equality checks complete context bytes, not pointer/hash identity alone. */
int xideLanguageInputGuardSameContext(const xide_language_input_guard* left,const xide_language_input_guard* right);
/* Borrowed native context, NULL for legacy/GUI proofs. No I/O or allocation. */
const xls_context* xideLanguageInputGuardContext(const xide_language_input_guard* guard);
/* Worker-only exact invalidation scope: actual inputs and every observed
   import candidate, including absent paths and manifests. Sorted/deduplicated
   with existing platform lexical path rules; no filesystem access. Caller
   owns the array, which outlives guard. Cancellation does not invalidate guard. */
xvalue* xideLanguageInputGuardPaths(const xide_language_input_guard* guard,int (*cancelled)(void*),void* user);
/* Same exact scope for the union of at most64 contributing entry guards. */
xvalue* xideLanguageInputGuardsPaths(const xide_language_input_guard* const* guards,size_t count,int (*cancelled)(void*),void* user);
int xideLanguageInputGuardCurrent(const xide_language_input_guard* guard);
void xideLanguageInputGuardInvalidate(xide_language_input_guard* guard);
/* Lexical inputs/candidate lookup only, without disk access. */
int xideLanguageInputGuardTouches(const xide_language_input_guard* guard,const char* path);
/* Worker-only serialized metadata plus retained immutable context bytes for
   bounded diagnostic retention; shared contexts are conservatively charged
   per guard. SIZE_MAX on failure. This is not a heap/RSS measurement. */
size_t xideLanguageInputGuardReportBytes(const xide_language_input_guard* guard);
/* Nonblocking retirement: Destroy can be called after WorkerFinished without
   waiting for disk work on the owner thread. Cache must outlive the worker. */
int xideLanguageCheckWorkerFinished(xide_language_check* check);
/* Windows service adapter only: borrowed thread-completion wait handle. It
   stays valid until Destroy; host can wait alongside protocol input instead
   of delaying the next cursor request by its periodic polling interval. */
void* xideLanguageCheckWorkerSignal(xide_language_check* check);
/* Caller frees the copied process-local package environment; NULL is unset. */
int xideLanguageCheckPackagePath(char** out,xide_model_error* error);
void xideLanguageCheckCancel(xide_language_check* check);
void xideLanguageCheckDestroy(xide_language_check* check);
#endif
