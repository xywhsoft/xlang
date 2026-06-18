#ifndef XL6_IR_DUMP_H
#define XL6_IR_DUMP_H

#include "src/ast/xl6_ast.h"

void xl6IrDumpText(FILE* out, const XL6_AstNode* root);
void xl6IrDumpJson(FILE* out, const XL6_AstNode* root);

#endif
