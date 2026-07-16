int xl6CgenNodeNeedsHttpServerRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsHttpServerClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsHttpServerRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsHttpServerRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsHttpClientRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsHttpClientClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsHttpClientRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsHttpClientRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsBufferRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsBufferClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsBufferRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTlsRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsTlsClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTlsRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
