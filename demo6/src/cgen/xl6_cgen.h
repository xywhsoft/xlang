#ifndef XL6_CGEN_H
#define XL6_CGEN_H

#include "src/ast/xl6_ast.h"
#include "src/common/xl6_output.h"
#include "src/diag/xl6_diag.h"
#include "src/ir/xl6_hir.h"

#include <stddef.h>

typedef struct XL6_CgenOptions {
    int debugMode;
    const char* debugSourcePath;
    int suppressMainWrapper;
} XL6_CgenOptions;

int xl6CgenEmitC(
    XL6_Output* out,
    XL6_AstNode* root,
    const XL6_HirModule* hir,
    XL6_DiagList* diags
);
int xl6CgenEmitCEx(
    XL6_Output* out,
    XL6_AstNode* root,
    const XL6_HirModule* hir,
    XL6_DiagList* diags,
    const XL6_CgenOptions* options
);
int xl6CgenEmitHeader(XL6_Output* out, XL6_AstNode* root, const char* guardName, XL6_DiagList* diags);

/* C 生成与头文件生成必须共享同一套有界、无碰撞的符号编码。 */
void xl6CgenEncodeSymbolComponent(char* out, size_t capacity, const char* text);
void xl6CgenEncodeCIdentifier(char* out, size_t capacity, const char* text);
void xl6CgenEmitAutoExportSymbol(
    XL6_Output* out,
    const XL6_AstNode* root,
    const XL6_AstNode* node);

#endif
