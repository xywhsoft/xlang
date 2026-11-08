#ifndef XLANG_ANALYSIS_AST_MATERIALIZE_H
#define XLANG_ANALYSIS_AST_MATERIALIZE_H
#include "analysis/ast_clone.h"
typedef struct xls_ast_materialize_work {
    uint64_t calls,pooledCopies,heapCopies,poolFallbacks,nodes,pages,chargedBytes;
} xls_ast_materialize_work;
typedef struct xls_ast_materialize_scope {
    struct xls_ast_materialize_scope* previous;
    xls_ast_storage* storage;
    size_t limit;
    xls_ast_materialize_work work;
} xls_ast_materialize_scope;
/* Private compiler trees only. Never use for retained syntax units or views:
 * those have a different completed-cache accounting/lifetime contract. Scopes
 * use separate backing for large function/body copies and amortize small
 * declarations in a shared pool. Nested scopes consume the parent's allowance.
 * Output fields/sources remain ordinary XRT and may mutate normally. */
void xlsAstMaterializeBegin(xls_ast_materialize_scope* scope,size_t bytes);
void xlsAstMaterializeEnd(xls_ast_materialize_scope* scope,xls_ast_materialize_work* work);
size_t xlsAstMaterializeBudget(void);
/* Only audited STATELESS placement/source-free projections. Failure may retry
 * ordinary copying. The generic clone APIs never implicitly retry callbacks. */
XL6_AstNode* xlsAstMaterialize(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes);
#endif
