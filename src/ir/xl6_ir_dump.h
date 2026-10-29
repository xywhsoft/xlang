#ifndef XL6_IR_DUMP_H
#define XL6_IR_DUMP_H

#include "src/ast/xl6_ast.h"
#include "src/common/xl6_output.h"

void xl6IrDumpText(XL6_Output* out, const XL6_AstNode* root);
void xl6IrDumpJson(XL6_Output* out, const XL6_AstNode* root);

#endif
