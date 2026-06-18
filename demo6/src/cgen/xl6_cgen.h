#ifndef XL6_CGEN_H
#define XL6_CGEN_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

int xl6CgenEmitC(FILE* out, XL6_AstNode* root, XL6_DiagList* diags);
int xl6CgenEmitHeader(FILE* out, XL6_AstNode* root, const char* guardName, XL6_DiagList* diags);

#endif
