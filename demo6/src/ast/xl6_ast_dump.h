#ifndef XL6_AST_DUMP_H
#define XL6_AST_DUMP_H

#include "src/ast/xl6_ast.h"
#include "src/common/xl6_output.h"

void xl6AstDumpText(const XL6_AstNode* node, int indent);
void xl6AstDumpTextFile(XL6_Output* out, const XL6_AstNode* node, int indent);
void xl6AstDumpJson(XL6_Output* out, const XL6_AstNode* node);

#endif
