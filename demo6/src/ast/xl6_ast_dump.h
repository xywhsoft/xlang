#ifndef XL6_AST_DUMP_H
#define XL6_AST_DUMP_H

#include "src/ast/xl6_ast.h"

#include <stdio.h>

void xl6AstDumpText(const XL6_AstNode* node, int indent);
void xl6AstDumpTextFile(FILE* out, const XL6_AstNode* node, int indent);
void xl6AstDumpJson(FILE* out, const XL6_AstNode* node);

#endif
