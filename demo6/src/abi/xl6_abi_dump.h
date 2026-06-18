#ifndef XL6_ABI_DUMP_H
#define XL6_ABI_DUMP_H

#include "src/ast/xl6_ast.h"

void xl6AbiDumpText(FILE* out, const XL6_AstNode* root);
void xl6AbiDumpJson(FILE* out, const XL6_AstNode* root);

#endif
