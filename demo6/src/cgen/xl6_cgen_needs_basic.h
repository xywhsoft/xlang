#ifndef XL6_CGEN_NEEDS_BASIC_H
#define XL6_CGEN_NEEDS_BASIC_H

#include "src/ast/xl6_ast.h"

int xl6CgenNodeNeedsRangeContains(const XL6_AstNode* node);
int xl6CgenNodeNeedsIntPow(const XL6_AstNode* node);
int xl6CgenNodeNeedsFunctionRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsApprox(const XL6_AstNode* node);
int xl6CgenNodeNeedsTimeRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsXValueRuntime(const XL6_AstNode* node);

#endif
