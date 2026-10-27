#ifndef XLANG_ANALYSIS_AST_STORAGE_H
#define XLANG_ANALYSIS_AST_STORAGE_H
#include "src/ast/xl6_ast.h"
typedef struct xls_ast_storage xls_ast_storage;
typedef struct xls_ast_pool_work { uint64_t nodes,pages,bytes; } xls_ast_pool_work;
/* One forest has one mutable owner. Construct/free under exclusive ownership;
 * immutable readers may clone concurrently without touching the input pool.
 * Strings,children,sources and literal payloads retain normal XRT ownership.
 * A construction pin keeps an empty/failed forest alive until cleanup ends. */
xls_ast_storage* xlsAstStorageCreate(size_t budget);
XL6_AstNode* xlsAstStorageNode(xls_ast_storage* storage,size_t extraBudget);
void xlsAstStorageUnpin(xls_ast_storage* storage);
void xlsAstStorageFreeNode(XL6_AstNode* node);
/* Private fact forests may share direct type roots in one construction.
 * Ordinary paths still opt in only for unlocated bare leaves;static semantic
 * restoration can explicitly prove complete direct-root projection equality.
 * Nested nodes are never independently aliased across distinct root contexts.
 * Each AST edge owns one reference;final release uses the original recursive
 * field/source/pool destructor. No cross-result mutable sharing. */
static inline int xlsAstSharedLeafRelease(XL6_AstNode* node)
{
    if(node && node->xlsLeafReferences>1){--node->xlsLeafReferences;return 1;}
    return 0;
}
xls_ast_pool_work xlsAstStorageWork(const xls_ast_storage* storage);
#endif
