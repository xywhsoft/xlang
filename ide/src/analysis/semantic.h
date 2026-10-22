#ifndef XLANG_ANALYSIS_SEMANTIC_H
#define XLANG_ANALYSIS_SEMANTIC_H
#include <stddef.h>
#include <stdint.h>

/* Host-independent, query-owned semantic facts. Inputs are borrowed for the
 * call only, paths must be absolute, and offsets exclude an optional UTF-8 BOM.
 * The shared semantic pass mutates a private tree, never a session snapshot.
 * No code generation, project execution, compiler process or DLL is involved. */
typedef struct xls_semantic_completion xls_semantic_completion;
typedef struct xls_semantic_input { const char *path,*text; size_t size; } xls_semantic_input;
typedef struct xls_completion_info {
    const char *path,*prefix,*error;
    size_t offset,start,end,count;
    int available;
} xls_completion_info;
typedef struct xls_completion_item {
    const char *label,*detail,*kind,*selectionPath;
    uint64_t symbol;
    size_t selectionStart,selectionEnd;
    int hasSelection;
} xls_completion_item;

/* At most 64 overlays, 8 MiB/file, 32 MiB total. Cancellation is checked at
 * phase boundaries; existing shared parser/semantic internals are not yet
 * interruptible. Separate results may be used on separate worker threads. */
xls_semantic_completion* xlsSemanticCompletionCreate(const char* entry,const char* queryPath,
    size_t offset,const xls_semantic_input* inputs,size_t count,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Immutable facts may outlive the producing session and be consumed on
 * separate threads. Each consumer owns a reference; retain requires an
 * existing live reference. Validation/serialization are internally locked. */
xls_semantic_completion* xlsSemanticCompletionRetain(xls_semantic_completion* result);
void xlsSemanticCompletionDestroy(xls_semantic_completion* result);
xls_completion_info xlsSemanticCompletionInfo(const xls_semantic_completion* result);
int xlsSemanticCompletionItem(const xls_semantic_completion* result,size_t index,xls_completion_item* item);
/* Revalidates the same captured disk inputs and import candidates. Failure
 * permanently invalidates the result. Views borrow result until destruction. */
int xlsSemanticCompletionValidate(xls_semantic_completion* result);
/* Transitional adapter for the existing guarded service. Serializes using
 * the compiler's own source/completion report, in memory, without temp files.
 * Caller releases with xlsSemanticStringFree, independent of host allocator. */
char* xlsSemanticCompletionJson(xls_semantic_completion* result,size_t* size);
void xlsSemanticStringFree(char* text);
#endif
