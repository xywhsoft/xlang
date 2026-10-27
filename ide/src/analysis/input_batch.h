#ifndef XLANG_ANALYSIS_INPUT_BATCH_H
#define XLANG_ANALYSIS_INPUT_BATCH_H
#include "analysis/session.h"
#include "analysis/semantic.h"
/* Private immutable capture for the one-owner check and cursor sessions. */
typedef struct xls_input_batch xls_input_batch;
int xlsInputLimits(const xls_session_limits* requested,xls_session_limits* chosen);
xls_input_batch* xlsInputBatchCreate(const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_input_batch* xlsInputBatchCreateInContext(xls_context* context,const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* Dependency-session opt-in:zero overlays is valid only for a configured
 * disk entry (non-NULL context,NULL target). Existing entrypoints keep their
 * nonempty-input contract. No synthetic editor identity/version is created. */
xls_input_batch* xlsInputBatchCreateClosedInContext(xls_context* context,const xls_session_limits* limits,const xls_input_batch* previous,
    const xls_analysis_document* documents,size_t count,const char* target,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
/* NULL target selects context.entry without creating a synthetic overlay.
 * Target then returns NULL; Entry returns the actual compilation path.
 * Shared frontend input reports guard disk entries and their dependencies. */
const xls_context* xlsInputBatchContext(const xls_input_batch* batch);
/* Immutable input history can be shared by the worker's current revision
 * authority and a retained root. Retain/release do not copy source bodies. */
xls_input_batch* xlsInputBatchRetain(const xls_input_batch* batch);
void xlsInputBatchDestroy(xls_input_batch* batch);
const xls_analysis_document* xlsInputBatchFind(const xls_input_batch* batch,const char* key);
/* Borrowed immutable row in captured order; no lookup/allocation/I/O. */
const xls_analysis_document* xlsInputBatchAt(const xls_input_batch* batch,size_t ordinal);
const xls_analysis_document* xlsInputBatchTarget(const xls_input_batch* batch);
const char* xlsInputBatchEntry(const xls_input_batch* batch);
const char* xlsInputBatchDirectory(const xls_input_batch* batch);
int xlsInputSameBody(const xls_analysis_document* a,const xls_analysis_document* b);
/* Ignores input order and increasing revisions, after capture validation. */
int xlsInputBatchEqual(const xls_input_batch* a,const xls_input_batch* b);
/* For a complete original-frontend usage mask in a's document order. Input
 * membership/keys/identities/paths/context/directory/entry must all agree.
 * Entry and optional query bodies remain mandatory. Only unobserved body
 * changes may be ignored. Remaps usage into b's order, including exact hits;
 * revision validation and disk/probe validation are separate requirements. */
int xlsInputBatchEqualUsed(const xls_input_batch* a,const xls_input_batch* b,
    uint64_t used,const char* query,uint64_t* remapped);
size_t xlsInputBatchInputs(const xls_input_batch* batch,xls_semantic_input* inputs,size_t capacity);
size_t xlsInputBatchBytes(const xls_input_batch* batch);
/* Known input storage charge:text buffer/line capacities,context,labels and
 * batch. Shared text/context charged per root. Excludes map allocator metadata,
 * semantic models/AST/IR/parse aids and RSS. SIZE_MAX means unrepresentable. */
size_t xlsInputBatchStorageBytes(const xls_input_batch* batch);
#endif
