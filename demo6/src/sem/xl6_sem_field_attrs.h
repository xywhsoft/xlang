#ifndef XL6_SEM_FIELD_ATTRS_H
#define XL6_SEM_FIELD_ATTRS_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

XL6_AstNode* xl6SemFieldDefaultExpr(const XL6_AstNode* field);
int xl6SemCheckFieldAttrs(XL6_DiagList* diags, const XL6_AstNode* field);

#endif
