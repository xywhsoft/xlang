int xl6CgenNodeNeedsEncodingRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsEncodingRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsEncodingRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsLogRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsLoggerClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsLogRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsLogRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsSignalRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsSignalClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsSignalRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsSignalRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsSignalChannelRuntime(const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* callee;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 ) {
        callee = node->children[0];
        if ( callee != NULL &&
             callee->kind == XL6_AST_NAME &&
             xl6CgenNeedsBasicTextEquals(callee->text, "__xl_std_runtime_signal_send_to") ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsSignalChannelRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsTemplateRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsTemplateClass(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsTemplateRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsTemplateRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsAsyncFileRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsAsyncFileRuntimeCall(node) ) {
        return 1;
    }
    if ( xl6CgenNeedsBasicIsAsyncFileClass(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsAsyncFileRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsFileRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( xl6CgenNeedsBasicIsFileClass(node) || xl6CgenNeedsBasicIsFileRuntimeCall(node) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsFileRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsSpecialCollectionRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->text != NULL &&
         (xl6CgenNeedsBasicTextEquals(node->text, "Stack") ||
          xl6CgenNeedsBasicTextEquals(node->text, "AvlTree") ||
          xl6CgenNeedsBasicTextEquals(node->text, "SpscQueue") ||
          xl6CgenNeedsBasicTextEquals(node->text, "MpscQueue") ||
          xl6CgenNeedsBasicTextEquals(node->text, "MpmcQueue") ||
          xl6CgenNeedsBasicTextEquals(node->text, "WaitQueue")) ) {
        return 1;
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsSpecialCollectionRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

int xl6CgenNodeNeedsOsRuntime(const XL6_AstNode* node)
{
    size_t i;
    const XL6_AstNode* callee;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->kind == XL6_AST_CALL && node->childCount > 0 ) {
        callee = node->children[0];
        if ( callee != NULL &&
             callee->kind == XL6_AST_NAME &&
             callee->text != NULL &&
             xl6CgenNeedsBasicTextStartsWith(callee->text, "__xl_std_runtime_os_") ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsOsRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
