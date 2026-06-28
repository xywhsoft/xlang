#ifndef XL6_CGEN_H
#define XL6_CGEN_H

#include "src/ast/xl6_ast.h"
#include "src/common/xl6_output.h"
#include "src/diag/xl6_diag.h"

typedef struct XL6_CgenOptions {
    int debugMode;
    const char* debugSourcePath;
} XL6_CgenOptions;

int xl6CgenEmitC(XL6_Output* out, XL6_AstNode* root, XL6_DiagList* diags);
int xl6CgenEmitCEx(XL6_Output* out, XL6_AstNode* root, XL6_DiagList* diags, const XL6_CgenOptions* options);
int xl6CgenEmitHeader(XL6_Output* out, XL6_AstNode* root, const char* guardName, XL6_DiagList* diags);

#endif
