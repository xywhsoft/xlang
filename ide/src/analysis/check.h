#ifndef XLANG_ANALYSIS_CHECK_H
#define XLANG_ANALYSIS_CHECK_H
#include "analysis/semantic.h"

typedef struct xls_semantic_check xls_semantic_check;
#define XLS_CHECK_REPORT_BYTES (36u * 1024u * 1024u)
typedef struct xls_check_info {
    const char *path,*phase,*error;
    size_t diagnostics,irTypes,irNodes;
    int accepted,hasIndex;
} xls_check_info;
/* One-based compiler Unicode-scalar coordinates; the host converts to LSP. */
typedef struct xls_check_diagnostic { const char *path,*message; int line,column; } xls_check_diagnostic;

/* Private immutable result, independent of transport/GUI and session ASTs.
 * Shared parser/imports/stdlib/semantics AND typed IR checks run in-process.
 * Absolute entry may be an overlay or a disk file. Up to 64 UTF-8 overlays,
 * 8 MiB/file, 32 MiB total. Inputs are borrowed only during the call; no code
 * generation, compiler process, DLL, global chdir or project execution.
 * Compile errors are valid results with accepted=0; infrastructure errors
 * and cancellation return NULL. Cancellation checks are phase-boundary only. */
xls_semantic_check* xlsSemanticCheckCreate(const char* entry,
    const xls_semantic_input* inputs,size_t count,int captureIndex,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
void xlsSemanticCheckDestroy(xls_semantic_check* result);
xls_check_info xlsSemanticCheckInfo(const xls_semantic_check* result);
int xlsSemanticCheckDiagnostic(const xls_semantic_check* result,size_t index,xls_check_diagnostic* diagnostic);
/* Captured index borrows result. It is available only after full success;
 * adapters may use compiler schema JSON until typed navigation is retained. */
const char* xlsSemanticCheckIndex(const xls_semantic_check* result);
/* Permanent invalidation on disk/source/resolution/environment changes. */
int xlsSemanticCheckValidate(xls_semantic_check* result);
/* Compiler's existing check/source/index report, serialized in memory.
 * Caller releases through xlsSemanticStringFree. Optional size may be NULL. */
char* xlsSemanticCheckJson(xls_semantic_check* result,size_t* size);
#endif
