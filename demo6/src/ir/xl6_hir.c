#include "src/ir/xl6_hir.h"

#include <string.h>

/* 这些表达式在任何合法上下文中都必须产生且只产生一个静态类型。 */
static int xl6HirNodeRequiresType(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return 0;
    }
    switch ( node->kind ) {
    case XL6_AST_INT:
    case XL6_AST_FLOAT:
    case XL6_AST_STRING:
    case XL6_AST_INTERP_STRING:
    case XL6_AST_TIME:
    case XL6_AST_BOOL:
    case XL6_AST_NULL:
    case XL6_AST_ANON_FUNC:
    case XL6_AST_INDEX:
    case XL6_AST_SAFE_INDEX:
    case XL6_AST_CONDITIONAL:
    case XL6_AST_BINARY:
    case XL6_AST_TYPE_IS:
    case XL6_AST_TYPE_AS:
    case XL6_AST_UNARY:
    case XL6_AST_AWAIT:
    case XL6_AST_ARRAY_LITERAL:
    case XL6_AST_LIST_LITERAL:
    case XL6_AST_SET_LITERAL:
    case XL6_AST_DICT_LITERAL:
    case XL6_AST_TYPED_LITERAL:
        return 1;
    default:
        return 0;
    }
}

static int xl6HirFinalizeNode(XL6_AstNode* node, XL6_DiagList* diags, XL6_HirStats* stats)
{
    size_t i;
    int ok;

    if ( node == NULL ) {
        return 1;
    }
    stats->nodeCount += 1;
    ok = 1;
    if ( xl6HirNodeRequiresType(node) ) {
        stats->valueNodeCount += 1;
        if ( node->inferredType == NULL ||
             node->inferredType->kind != XL6_AST_TYPE ||
             node->inferredType->text == NULL ||
             node->inferredType->text[0] == '\0' ) {
            xl6DiagAdd(diags, node->line, node->column, "typed HIR value expression is missing its static type");
            ok = 0;
        } else {
            stats->typedValueNodeCount += 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( !xl6HirFinalizeNode(node->children[i], diags, stats) ) {
            ok = 0;
        }
    }
    return ok;
}

int xl6HirFinalizeModule(XL6_AstNode* root, XL6_DiagList* diags, XL6_HirStats* stats)
{
    XL6_HirStats localStats;
    int ok;

    memset(&localStats, 0, sizeof(localStats));
    ok = root != NULL && xl6HirFinalizeNode(root, diags, &localStats);
    if ( stats != NULL ) {
        *stats = localStats;
    }
    return ok;
}

const XL6_AstNode* xl6HirExprType(const XL6_AstNode* node)
{
    return node != NULL ? node->inferredType : NULL;
}
