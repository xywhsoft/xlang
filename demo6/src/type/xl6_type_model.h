#ifndef XL6_TYPE_MODEL_H
#define XL6_TYPE_MODEL_H

#include "src/ast/xl6_ast.h"

/* Allocation-free source integer spelling shared by semantic range checks
 * and IR literal materialization. A sign belongs to the unary expression. */
int xl6TypeParseIntegerMagnitude(const char* text, uint64_t* value);

const XL6_AstNode* xl6TypeResolveAlias(const XL6_AstNode* root, const XL6_AstNode* typeNode);
const char* xl6TypeCanonicalBuiltinName(const char* name);
char* xl6TypeToCanonicalString(const XL6_AstNode* root, const XL6_AstNode* typeNode);
/* Same canonical formatter; NULL on allocation or byte/depth/visit limits. */
char* xl6TypeToCanonicalStringLimited(const XL6_AstNode* root, const XL6_AstNode* typeNode,
    size_t maxBytes, size_t maxDepth, size_t maxVisits);
int xl6TypeTextIsArrayVararg(const char* text);
int xl6TypeTextIsDictVararg(const char* text);
int xl6TypeTextIsCVararg(const char* text);
int xl6TypeTextIsVararg(const char* text);

#endif
