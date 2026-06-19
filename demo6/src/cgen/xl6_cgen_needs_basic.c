#include "src/cgen/xl6_cgen_needs_basic.h"

#include <string.h>

static int xl6CgenNeedsBasicTextEquals(const char* a, const char* b)
{
    return (a != NULL) && (b != NULL) && (strcmp(a, b) == 0);
}

static int xl6CgenNeedsBasicIsRangeCall(const XL6_AstNode* node)
{
    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount < 2 ) {
        return 0;
    }
    return node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6CgenNeedsBasicTextEquals(node->children[0]->text, "range");
}

static int xl6CgenNeedsBasicIsArrayXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "array") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "array<var>"));
}

static int xl6CgenNeedsBasicIsListXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "list") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "list<var>"));
}

static int xl6CgenNeedsBasicIsSetXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "set") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "set<var>"));
}

static int xl6CgenNeedsBasicIsDictXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "dict") ||
            xl6CgenNeedsBasicTextEquals(typeNode->text, "dict<var>"));
}

static int xl6CgenNeedsBasicIsXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CgenNeedsBasicTextEquals(typeNode->text, "var") ||
            xl6CgenNeedsBasicIsArrayXValueType(typeNode) ||
            xl6CgenNeedsBasicIsListXValueType(typeNode) ||
            xl6CgenNeedsBasicIsSetXValueType(typeNode) ||
            xl6CgenNeedsBasicIsDictXValueType(typeNode));
}

static int xl6CgenNeedsBasicIsToStringMemberCallSyntax(const XL6_AstNode* node)
{
    const XL6_AstNode* callee;
    const XL6_AstNode* args;

    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount == 0 ) {
        return 0;
    }

    callee = node->children[0];
    if ( callee == NULL || callee->kind != XL6_AST_MEMBER ||
         !xl6CgenNeedsBasicTextEquals(callee->text, "toString") ) {
        return 0;
    }

    args = node->childCount > 1 ? node->children[1] : NULL;
    return args == NULL || args->childCount == 0;
}

int xl6CgenNodeNeedsRangeContains(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "in") &&
         node->childCount > 1 && xl6CgenNeedsBasicIsRangeCall(node->children[1]) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRangeContains(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsIntPow(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "**") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ASSIGN && xl6CgenNeedsBasicTextEquals(node->text, "**=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsIntPow(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsFunctionRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_ANON_FUNC ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenNeedsBasicTextEquals(node->text, "function") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsFunctionRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsApprox(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_BINARY && xl6CgenNeedsBasicTextEquals(node->text, "~=") ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsApprox(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTimeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TIME ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTimeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsXValueRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenNeedsBasicIsXValueType(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsToStringMemberCallSyntax(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "var") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_ARRAY_LITERAL || node->kind == XL6_AST_LIST_LITERAL ||
         node->kind == XL6_AST_SET_LITERAL || node->kind == XL6_AST_DICT_LITERAL ||
         node->kind == XL6_AST_TYPED_LITERAL ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXValueRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
