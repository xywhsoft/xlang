#ifndef XLANG_ANALYSIS_SIGNATURE_H
#define XLANG_ANALYSIS_SIGNATURE_H
#include "analysis/semantic.h"
/* Immutable query views retain the guarded native model. Ordinary completion
 * and signature results have distinct opaque owners and independent caches. */
typedef struct xls_signature_help xls_signature_help;
typedef struct xls_signature_info {
    const char *path,*error;
    size_t offset,count,activeSignature,argument;
    int available,projected;
} xls_signature_info;
typedef struct xls_signature_item {
    const char *label,*documentation;
    size_t parameters,activeParameter;
    uint64_t symbol;
} xls_signature_item;
typedef struct xls_signature_parameter {
    const char* name;
    size_t labelStart,labelEnd;
    int optional,variadic; /* 0=fixed,1=positional,2=named,3=C varargs. */
} xls_signature_parameter;
/* Input/offset/context limits are semantic.h's. Parameters are structured
 * compiler types,never parsed from a formatted signature string. Label spans
 * use UTF-8 bytes; protocol adapters convert/choose string labels. SIZE_MAX
 * denotes an unknown active signature/parameter. Views borrow this owner. */
xls_signature_help* xlsSignatureCreateInContext(xls_context* context,const char* entry,const char* path,size_t offset,
    const xls_semantic_input* inputs,size_t count,int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_signature_help* xlsSignatureRetain(xls_signature_help* result);
void xlsSignatureRelease(xls_signature_help* result);
int xlsSignatureValidate(xls_signature_help* result);
typedef struct xls_signature_work { uint64_t checked,reused,proofHits,proofNodes; } xls_signature_work;
xls_signature_work xlsSignatureWork(const xls_signature_help* result);
/* Work creating the immutable model, also returned by cached/caret views.
 * Counts query-file parse work only; cacheBytes charges the retained snapshot.
 * A first full seed is not a fallback. Limits are completed-data, not RSS. */
typedef struct xls_signature_parse_work { uint64_t full,fragments,reused,bytes,exactHits,cacheBytes,fallbacks; } xls_signature_parse_work;
xls_signature_parse_work xlsSignatureParseWork(const xls_signature_help* result);
/* Original native type-restoration work creating the immutable model. Exact
 * and caret views keep its provenance. Counts pool storage, not process RSS. */
typedef struct xls_signature_storage_work { uint64_t nodes,pages,chargedBytes,heapFallbacks; } xls_signature_storage_work;
xls_signature_storage_work xlsSignatureStorageWork(const xls_signature_help* result);
xls_signature_info xlsSignatureInfo(const xls_signature_help* result);
int xlsSignatureItem(const xls_signature_help* result,size_t index,xls_signature_item* item);
int xlsSignatureParameter(const xls_signature_help* result,size_t index,size_t parameter,xls_signature_parameter* value);
int xlsSignatureInputs(const xls_signature_help* result,xls_inputs* inputs);
int xlsSignatureSource(const xls_signature_help* result,size_t index,xls_input_source* source);
int xlsSignatureProbe(const xls_signature_help* result,size_t index,xls_input_probe* probe);
/* Transitional guarded host adapter: original compiler source/resolution
 * reports only, no CompletionList or signature facts. Requires a real captured
 * input set. XRT-owned UTF-8; release with xlsSemanticStringFree. */
char* xlsSignatureInputJson(xls_signature_help* result,size_t* size);
#endif
