#ifndef XL6_ABI_DUMP_H
#define XL6_ABI_DUMP_H

#include "src/ast/xl6_ast.h"
#include "src/common/xl6_output.h"

void xl6AbiDumpText(XL6_Output* out, const XL6_AstNode* root);
void xl6AbiDumpJson(XL6_Output* out, const XL6_AstNode* root);

#endif
