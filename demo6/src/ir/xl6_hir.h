#ifndef XL6_HIR_H
#define XL6_HIR_H

#include "src/ast/xl6_ast.h"
#include "src/diag/xl6_diag.h"

typedef struct XL6_HirStats {
    size_t nodeCount;
    size_t valueNodeCount;
    size_t typedValueNodeCount;
} XL6_HirStats;

int xl6HirFinalizeModule(XL6_AstNode* root, XL6_DiagList* diags, XL6_HirStats* stats);
const XL6_AstNode* xl6HirExprType(const XL6_AstNode* node);

#endif
