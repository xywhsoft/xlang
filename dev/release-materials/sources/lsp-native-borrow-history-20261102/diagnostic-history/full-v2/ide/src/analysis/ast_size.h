#ifndef XLS_AST_SIZE_H
#define XLS_AST_SIZE_H
#include "analysis/ast_storage.h"
#include <xrt.h>
/* Read-only known native allocation sizing. Construction-private traversal,
 * not a freshness proof or process RSS estimate. Pool mode charges the FULL
 * recorded backing upper bound once per storage;logical mode preserves the
 * explicit function unit's original per-node quota. Temporary maps retire. */
typedef struct xls_ast_size {xmap sources,pools;size_t bytes,limit;uint64_t nodes;int backing;} xls_ast_size;
int xlsAstSizeBegin(xls_ast_size*,size_t limit,int backing);
int xlsAstSizeNode(xls_ast_size*,const XL6_AstNode*);
int xlsAstSizeTree(xls_ast_size*,const XL6_AstNode*,size_t depth,int (*cancelled)(void*),void*);
void xlsAstSizeEnd(xls_ast_size*);
#endif
