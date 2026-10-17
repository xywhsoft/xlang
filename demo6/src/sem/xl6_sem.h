#ifndef XL6_SEM_H
#define XL6_SEM_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

int xl6SemCheckModule(
    XL6_AstNode* root,
    XL6_DiagList* diags,
    const char* moduleName
);

struct XL6_CompletionQuery;
int xl6SemCheckModuleWithQuery(XL6_AstNode* root,XL6_DiagList* diags,
    const char* moduleName,int captureIndex,struct XL6_CompletionQuery* query);

int xl6SemCheckModuleWithIndex(XL6_AstNode* root, XL6_DiagList* diags,
    const char* moduleName, int captureIndex);

#endif
