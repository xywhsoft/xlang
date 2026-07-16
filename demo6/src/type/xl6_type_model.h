#ifndef XL6_TYPE_MODEL_H
#define XL6_TYPE_MODEL_H

#include "src/ast/xl6_ast.h"

const XL6_AstNode* xl6TypeResolveAlias(const XL6_AstNode* root, const XL6_AstNode* typeNode);
char* xl6TypeToCanonicalString(const XL6_AstNode* root, const XL6_AstNode* typeNode);
int xl6TypeTextIsArrayVararg(const char* text);
int xl6TypeTextIsDictVararg(const char* text);
int xl6TypeTextIsCVararg(const char* text);
int xl6TypeTextIsVararg(const char* text);

#endif
