#ifndef XLANG_ANALYSIS_AST_CLONE_H
#define XLANG_ANALYSIS_AST_CLONE_H
#include "src/ast/xl6_ast.h"
/* Private representation copy, never a second parser/semantic rule set.
 * Sources are copied into the new tree, without touching non-atomic source
 * references in an immutable snapshot owned by another thread. */
typedef int (*xls_ast_transform)(const XL6_AstNode* input,XL6_AstNode* output,void* user);
typedef struct xls_ast_clone_options {
    xls_ast_transform transform;
    void* user;
    size_t bytes; /* zero: no extra byte budget; allocation failure still fails */
    int dropSources;
} xls_ast_clone_options;
XL6_AstNode* xlsAstClone(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes);
#endif
