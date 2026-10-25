#ifndef XLANG_ANALYSIS_CHECK_H
#define XLANG_ANALYSIS_CHECK_H
#include "analysis/semantic.h"
#include "analysis/index.h"
#include "analysis/context.h"

typedef struct xls_semantic_check xls_semantic_check;
#define XLS_CHECK_REPORT_BYTES (36u * 1024u * 1024u)
typedef struct xls_check_info {
    const char *path,*phase,*error;
    size_t diagnostics,irTypes,irNodes;
    int accepted,hasIndex;
} xls_check_info;
/* One-based compiler Unicode-scalar coordinates; the host converts to LSP. */
typedef struct xls_check_diagnostic { const char *path,*message; int line,column; } xls_check_diagnostic;

/* Captured facts, not an implicit validation or a filesystem query. All string
 * pointers borrow the retained check; hashes are copied. Source text is strict
 * UTF-8 after optional BOM removal, with its original CR/LF bytes. Disk rawSize
 * and rawSha256 describe the original file; overlays describe post-BOM bytes
 * in both fields. kind is a bit mask (source=1, manifest=2). */
typedef xls_input_source xls_check_source;
typedef xls_input_probe xls_check_probe;
typedef xls_inputs xls_check_inputs;
/* No allocation, hashing, JSON or I/O. Includes diagnostic/error results with
 * complete captured inputs. Unknown/unobserved package environment is distinct
 * from an observed absent/empty value. Does not observe the current environment.
 * Failure clears non-NULL outputs. Known-invalid results reject fresh views;
 * already borrowed views remain memory-valid until their check is released,
 * but cannot prove current inputs. Validate again before publishing/using facts. */
int xlsSemanticCheckInputs(const xls_semantic_check* result,xls_check_inputs* inputs);
int xlsSemanticCheckSource(const xls_semantic_check* result,size_t index,xls_check_source* source);
int xlsSemanticCheckProbe(const xls_semantic_check* result,size_t index,xls_check_probe* probe);
typedef struct xls_check_semantic_work { uint64_t functionsChecked,functionsReused,contextHits,memoBytes; } xls_check_semantic_work;
/* Work captured by the opt-in retained-entry path. The independent fresh
 * reference API does not instrument function calls and reports zero here. */
xls_check_semantic_work xlsSemanticCheckWork(const xls_semantic_check* result);
/* Immutable ordinary entry parse work of the actual build. Exact-result hits
 * retain it. Overlay chunk syntax uses the existing session counters instead;
 * imports, semantic checking and IR still run after ordinary AST reuse. */
xls_entry_parse_work xlsSemanticCheckEntryWork(const xls_semantic_check* result);
xls_entry_parse_work xlsSemanticCheckModuleWork(const xls_semantic_check* result);

/* Private immutable result, independent of transport/GUI and session ASTs.
 * Shared parser/imports/stdlib/semantics AND typed IR checks run in-process.
 * Absolute entry may be an overlay or a disk file. Up to 64 UTF-8 overlays,
 * 8 MiB/file, 32 MiB total. Inputs are borrowed only during the call; no code
 * generation, compiler process, DLL, global chdir or project execution.
 * Compile errors are valid results with accepted=0; infrastructure errors
 * and cancellation return NULL. Cooperative checks also run at audited
 * semantic function/statement/expression and IR build/verify boundaries;
 * parser, imports and individual unchecked helpers remain non-preemptible. */
xls_semantic_check* xlsSemanticCheckCreate(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Explicit immutable resolution context. Entry must match context.entry.
 * Supports a build directory and raw package environment without changing
 * process state. C/resource/include/link settings currently reject; members
 * are index metadata, not implicit imports. Profile describes this linked
 * frontend, never selects another compiler. NULL preserves the legacy API.
 * Context/result ownership is independent. Manifest freshness is host-owned. */
xls_semantic_check* xlsSemanticCheckCreateInContext(xls_context* context,const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
const xls_context* xlsSemanticCheckContext(const xls_semantic_check* result);
/* Each owner holds a reference. Destroy releases one reference. Immutable
 * facts may be read concurrently; input validation and lazy compatibility
 * exports are serialized internally. Invalidation remains permanent. */
xls_semantic_check* xlsSemanticCheckRetain(xls_semantic_check* result);
void xlsSemanticCheckDestroy(xls_semantic_check* result);
xls_check_info xlsSemanticCheckInfo(const xls_semantic_check* result);
int xlsSemanticCheckDiagnostic(const xls_semantic_check* result,size_t index,xls_check_diagnostic* diagnostic);
/* Captured typed index borrows result, available only after full success.
 * Facts retain the real shared collector and the private checked AST. */
const xls_semantic_index* xlsSemanticCheckTypedIndex(const xls_semantic_check* result);
/* Compatibility export, generated lazily. Failure does not change source
 * diagnostics or invalidate a successful typed check. */
const char* xlsSemanticCheckIndex(xls_semantic_check* result);
/* Permanent invalidation on disk/source/resolution/environment changes. */
int xlsSemanticCheckValidate(xls_semantic_check* result);
/* Compiler's existing check/source/index report, serialized in memory.
 * Caller releases through xlsSemanticStringFree. Optional size may be NULL. */
char* xlsSemanticCheckJson(xls_semantic_check* result,size_t* size);
/* Small source/diagnostic report without the large compatibility index.
 * The host consumes typed facts separately; same input guards and ownership. */
char* xlsSemanticCheckReportJson(xls_semantic_check* result,size_t* size);
#endif
