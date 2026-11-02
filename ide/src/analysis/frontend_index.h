#ifndef XLANG_ANALYSIS_FRONTEND_INDEX_H
#define XLANG_ANALYSIS_FRONTEND_INDEX_H
#include "analysis/index.h"
#include "analysis/frontend_function_index.h"
#include "src/sem/xl6_sem_index.h"
/* Private frontend boundary. Implemented in the existing collector's
 * translation unit, so collection and identity rules have one owner. */
xls_semantic_index* xlsFrontendIndexCreate(const XL6_AstNode* root,
    const XL6_SourceReport* report,char* error,size_t capacity);
struct xls_sem_memo;
/* Current source/selection/type facts still use the original collector.
 * Only canonical strings may reuse an independently owned prior asset after
 * exact live semantic-unit,final-environment and final-output receipt proofs.
 * Both memos and the prior index remain alive throughout this call. */
xls_semantic_index* xlsFrontendIndexCreateCached(const XL6_AstNode*,const XL6_SourceReport*,
    const struct xls_sem_memo*,const xls_semantic_index*,const struct xls_sem_memo*,char*,size_t);
xls_index_canonical_work xlsFrontendIndexCanonicalWork(const xls_semantic_index*);
typedef struct xls_index_canonical_control {struct xls_index_canonical_control* previous;size_t bytes;} xls_index_canonical_control;
void xlsFrontendIndexCanonicalBegin(xls_index_canonical_control*,size_t);
void xlsFrontendIndexCanonicalEnd(xls_index_canonical_control*);
void xlsFrontendIndexDestroy(xls_semantic_index* index);
/* Conservative owned-array/string data charge, excluding borrowed AST and
 * runtime/allocator overhead. Not a process peak-memory measurement. */
size_t xlsFrontendIndexBudgetUsed(const xls_semantic_index* index);
/* Original collector child preorder,with global parent/source IDs intact. */
int xlsFrontendIndexFunctionSpan(const xls_semantic_index*,const XL6_AstNode*,size_t* first,size_t* count);
#endif
