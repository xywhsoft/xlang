#ifndef XL6_IMPORTS_H
#define XL6_IMPORTS_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

int xl6ImportsExpandModule(XL6_AstNode* root, const char* sourcePath, XL6_DiagList* diags);

#endif
