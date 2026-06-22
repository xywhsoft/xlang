#ifndef XL6_CGEN_NEEDS_BASIC_H
#define XL6_CGEN_NEEDS_BASIC_H

#include "src/ast/xl6_ast.h"

int xl6CgenNodeNeedsRangeContains(const XL6_AstNode* node);
int xl6CgenNodeNeedsIntPow(const XL6_AstNode* node);
int xl6CgenNodeNeedsFunctionRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsApprox(const XL6_AstNode* node);
int xl6CgenNodeNeedsTimeRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsXValueRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsCryptoRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsUrlRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsQueryRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsRegexRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsProcessRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsDirRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsStringEdgeRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsStdStringRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsPathRuntime(const XL6_AstNode* node);
int xl6CgenNodeNeedsHttpServerRuntime(const XL6_AstNode* node);

#endif
