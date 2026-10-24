#ifndef XLANG_ANALYSIS_AST_CLONE_H
#define XLANG_ANALYSIS_AST_CLONE_H
#include "src/ast/xl6_ast.h"
#include "analysis/ast_storage.h"
/* Private representation copy, never a second parser/semantic rule set.
 * Sources are copied into the new tree, without touching non-atomic source
 * references in an immutable snapshot owned by another thread. */
typedef int (*xls_ast_transform)(const XL6_AstNode* input,XL6_AstNode* output,void* user);
typedef XL6_AstSource* (*xls_ast_source_target)(const XL6_AstSource* input,void* user);
typedef struct xls_ast_clone_options {
    xls_ast_transform transform;
    void* user;
    size_t bytes; /* zero: no extra byte budget; allocation failure still fails */
    int dropSources;
    /* Optional destination owned by a PRIVATE construction tree on this
     * thread. Never return an immutable/published snapshot's source. NULL
     * selects the normal independent source copy. */
    xls_ast_source_target targetSource;
} xls_ast_clone_options;
XL6_AstNode* xlsAstClone(const XL6_AstNode* input,const xls_ast_clone_options* options,size_t* bytes);
/* Explicit opt-in for cursor fact restoration only. The clone still requires
 * different private destination ownership, identical source path and flags.
 * ONLY a start/end difference with cursor flag=1 on both sides may be admitted;
 * default cloning and the existing options layout stay unchanged. */
typedef int (*xls_ast_cursor_source_policy)(const XL6_AstSource* input,const XL6_AstSource* target,void* user);
XL6_AstNode* xlsAstCloneWithCursorSource(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t* bytes);
/* Optional native representation optimization. Same transform/source contract;
 * failure leaves input untouched. Zero pool allowance selects regular cloning.
 * Work counts constructed nodes/pages and conservative pool capacity charge. */
XL6_AstNode* xlsAstClonePooled(const XL6_AstNode* input,const xls_ast_clone_options* options,
    xls_ast_cursor_source_policy policy,void* user,size_t poolBudget,xls_ast_pool_work* work,size_t* bytes);
#endif
