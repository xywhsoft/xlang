#ifndef XLS_ANALYSIS_SEMANTIC_TOKENS_H
#define XLS_ANALYSIS_SEMANTIC_TOKENS_H
#include "analysis/check.h"

/* Semantic symbol roles, not protocol legend indexes or editor styles. */
enum xls_semantic_token_kind {
 XLS_TOKEN_NONE,XLS_TOKEN_TYPE,XLS_TOKEN_CLASS,XLS_TOKEN_INTERFACE,
 XLS_TOKEN_ENUM,XLS_TOKEN_STRUCT,XLS_TOKEN_TYPE_PARAMETER,XLS_TOKEN_PARAMETER,
 XLS_TOKEN_VARIABLE,XLS_TOKEN_PROPERTY,XLS_TOKEN_ENUM_MEMBER,
 XLS_TOKEN_FUNCTION,XLS_TOKEN_METHOD,XLS_TOKEN_KIND_COUNT
};
enum xls_semantic_token_modifier {
 XLS_TOKEN_DECLARATION=1u,XLS_TOKEN_READONLY=2u,XLS_TOKEN_ASYNC=4u
};
enum xls_semantic_tokens_error {
 XLS_TOKENS_OK,XLS_TOKENS_ARGUMENT,XLS_TOKENS_NO_INDEX,XLS_TOKENS_LIMIT,
 XLS_TOKENS_MEMORY,XLS_TOKENS_FACTS,XLS_TOKENS_CANCELLED
};
typedef struct xls_semantic_token {
 xls_index_range range;
 unsigned kind,modifiers;
} xls_semantic_token;
typedef struct xls_semantic_tokens xls_semantic_tokens;
typedef struct xls_semantic_tokens_work {size_t facts,declarations,candidates,bytes,peakBytes,atomBytes;} xls_semantic_tokens_work;

/* One zero-based source-report ordinal in a complete checked index. Reuses
 * native declaration/selection facts; never resolves symbols by spelling or reparses source.
 * Ranges retain original UTF-8-after-BOM offsets and compiler scalar positions.
 * The host batch-validates/converts these against its captured original text.
 * Caller retains the check during construction and independently proves input
 * freshness before publication. Returned immutable arrays own no check, AST,
 * source buffer, context or protocol value and outlive the producer.
 * budget bounds known array/object capacities, including temporary role tables;
 * 0 selects 16MiB, larger budgets are rejected. No partial result on failure. */
xls_semantic_tokens* xlsSemanticTokensCreate(const xls_semantic_check* check,size_t source,
 size_t budget,int (*cancelled)(void*),void* user,enum xls_semantic_tokens_error* error);
xls_semantic_tokens* xlsSemanticTokensRetain(xls_semantic_tokens* tokens);
void xlsSemanticTokensRelease(xls_semantic_tokens* tokens);
size_t xlsSemanticTokensCount(const xls_semantic_tokens* tokens);
const xls_semantic_token* xlsSemanticTokensData(const xls_semantic_tokens* tokens);
xls_semantic_tokens_work xlsSemanticTokensWork(const xls_semantic_tokens* tokens);
#endif
