#ifndef XLANG_ANALYSIS_FRONTEND_INDEX_H
#define XLANG_ANALYSIS_FRONTEND_INDEX_H
#include "analysis/index.h"
#include "src/sem/xl6_sem_index.h"
/* Private frontend boundary. Implemented in the existing collector's
 * translation unit, so collection and identity rules have one owner. */
xls_semantic_index* xlsFrontendIndexCreate(const XL6_AstNode* root,
    const XL6_SourceReport* report,char* error,size_t capacity);
void xlsFrontendIndexDestroy(xls_semantic_index* index);
/* Conservative owned-array/string data charge, excluding borrowed AST and
 * runtime/allocator overhead. Not a process peak-memory measurement. */
size_t xlsFrontendIndexBudgetUsed(const xls_semantic_index* index);
#endif
