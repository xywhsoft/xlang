#ifndef XLANG_ANALYSIS_SIGNATURE_INTERNAL_H
#define XLANG_ANALYSIS_SIGNATURE_INTERNAL_H
#include "analysis/signature.h"
#include "analysis/call_context.h"
#include "analysis/frontend_entry_cache.h"
#include "src/sem/xl6_completion.h"
#include "src/lexer/xl6_lexer_api.h"
#include <xrt.h>
typedef struct xls_signature_record {
    char* label;size_t labelCapacity;
    const char* documentation;
    xls_signature_parameter* parameters;size_t count,active;
    uint64_t symbol,rank;int ranked;
} xls_signature_record;
typedef struct xls_signature_data {
    xls_call_context* index;
    const char* source;size_t size,offset,bytes,active;
    char* path;const char* error;
    xls_call_site site;xarray records;
    int hasSite,projected;
} xls_signature_data;
typedef struct xls_signature_scope {
    struct xls_signature_scope* previous;
    xls_signature_data* data;XL6_CompletionQuery* query;
    XL6_AstNode* call;const char* targetName;
    const XL6_AstNode* typeArgs;
    int (*cancelled)(void*);void* cancelUser;
    int adopted;
    int cancelObserved;
} xls_signature_scope;
void xlsSignatureScopeBegin(xls_signature_scope* scope,xls_signature_data* data,int (*cancelled)(void*),void* user);
void xlsSignatureScopeEnd(xls_signature_scope* scope);
xls_signature_scope* xlsSignatureScopeCurrent(const XL6_CompletionQuery* query);
int xlsSignatureScopeCancelled(xls_signature_scope* scope);
void xlsSignatureDataDestroy(xls_signature_data* data);
/* Append consumes owned record label/parameter allocations on success/failure;
 * names/docs borrow the same private AST held by the eventual result. */
int xlsSignatureAppend(xls_signature_scope* scope,xls_signature_record record);
xls_signature_data* xlsSemanticSignatureData(const xls_semantic_completion* result);
/* Consumes data even on infrastructure failure. */
xls_semantic_completion* xlsSemanticSignatureCreateInContext(xls_context* context,const char* entry,
    const char* path,size_t offset,const xls_semantic_input* inputs,size_t count,xls_signature_data* data,
    xls_entry_cache* cache,xls_semantic_completion* previous,int memoEnabled,
    int (*cancelled)(void*),void* user,char* error,size_t capacity);
xls_signature_help* xlsSignatureCreateRetainedInContext(xls_context* context,const char* entry,const char* path,
    size_t offset,const xls_semantic_input* inputs,size_t count,xls_call_context* index,xls_entry_cache* cache,
    xls_signature_help* previous,int (*cancelled)(void*),void* user,char* error,size_t capacity);
int xlsFrontendLexerResume(XL6_Lexer* lexer,const char* source,size_t offset,int paren,int bracket,int brace);
#endif
