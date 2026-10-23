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
const xls_context* xlsInputBatchContext(const xls_input_batch* batch);
void xlsInputBatchDestroy(xls_input_batch* batch);
const xls_analysis_document* xlsInputBatchFind(const xls_input_batch* batch,const char* key);
const xls_analysis_document* xlsInputBatchTarget(const xls_input_batch* batch);
const char* xlsInputBatchDirectory(const xls_input_batch* batch);
int xlsInputSameBody(const xls_analysis_document* a,const xls_analysis_document* b);
/* Ignores input order and increasing revisions, after capture validation. */
int xlsInputBatchEqual(const xls_input_batch* a,const xls_input_batch* b);
size_t xlsInputBatchInputs(const xls_input_batch* batch,xls_semantic_input* inputs,size_t capacity);
size_t xlsInputBatchBytes(const xls_input_batch* batch);
#endif
