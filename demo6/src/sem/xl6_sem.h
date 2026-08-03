#ifndef XL6_SEM_H
#define XL6_SEM_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

int xl6SemCheckModule(
    XL6_AstNode* root,
    XL6_DiagList* diags,
    const char* moduleName
);

#endif
