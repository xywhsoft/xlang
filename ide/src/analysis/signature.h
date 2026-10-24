#ifndef XLANG_ANALYSIS_SIGNATURE_H
#define XLANG_ANALYSIS_SIGNATURE_H
#include "analysis/semantic.h"
/* The opaque query owner shares the existing guarded native-result lifetime;
 * ordinary completion and signature-mode results never share a cache slot. */
typedef struct xls_semantic_completion xls_signature_help;
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
