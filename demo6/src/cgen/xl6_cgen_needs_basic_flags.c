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
    if ( node->kind == XL6_AST_CALL &&
         node->childCount > 0 &&
         node->children[0] != NULL &&
         node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "thread") ) {
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
    if ( xl6CgenNeedsBasicIsBuiltinObjectClass(node) ) {
        return 0;
    }
    if ( node->kind == XL6_AST_ENUM &&
         (node->flags & XL6_AST_FLAG_NUMERIC_ENUM) == 0 ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE && xl6CgenNeedsBasicIsXValueType(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "type") ) {
        return 1;
    }
    if ( node->kind == XL6_AST_TYPE_IS || node->kind == XL6_AST_TYPE_AS ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsToStringMemberCallSyntax(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsValueDerive(node) ||
         xl6CgenNeedsBasicIsPersistenceGenericCall(node) ) {
        return 1;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 &&
         node->children[0] != NULL && node->children[0]->kind == XL6_AST_NAME &&
         xl6CgenNeedsBasicTextEquals(node->children[0]->text, "var") ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsPrintIntrinsicCall(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsContainerMethodCall(node) ) {
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

int xl6CgenNodeNeedsCryptoRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsCryptoClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsCryptoRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsCryptoRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsUrlRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsUrlRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsUrlRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsQueryRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsQueryRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsQueryRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsXidRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsXidClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXidRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsRegexRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsRegexClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsRegexRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsRegexRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsJsonRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsJsonClass(node) || xl6CgenNeedsBasicIsJsonRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsJsonRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsProcessRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsProcessClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsProcessRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsProcessRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsNetRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsNetClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsNetRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsDirRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsDirRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsDirRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsStringEdgeRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsStringEdgeRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStringEdgeRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsStdStringRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsStdStringRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsStdStringRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsPathRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsPathRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsPathRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

#include "src/cgen/xl6_cgen_needs_network.c"
#include "src/cgen/xl6_cgen_needs_stdlib.c"
