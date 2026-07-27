#ifndef XL6_STDLIB_H
#define XL6_STDLIB_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

int xl6StdlibExpandModule(XL6_AstNode* root, XL6_DiagList* diags);
const char* xl6StdlibImplicitMethodInternalName(const char* typeName, const char* methodName);

#endif
