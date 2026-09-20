static int xl6CgenNodeNeedsXlangRuntime(const XL6_AstNode* node)
{
    size_t i;

    if ( node == NULL ) {
        return 0;
    }
    if ( node->text != NULL ) {
        if ( node->kind == XL6_AST_IMPORT &&
             (xl6CTextEquals(node->text, "xlang") ||
              xl6CTextEquals(node->text, "xlang.compiler")) ) {
            return 1;
        }
        if ( (node->kind == XL6_AST_TYPE || node->kind == XL6_AST_NAME) &&
             (xl6CTextEquals(node->text, "XLangContext") ||
              xl6CTextEquals(node->text, "XLangHandler")) ) {
            return 1;
        }
        if ( node->kind == XL6_AST_NAME &&
             (strncmp(node->text, "__xl_std_xlang_", 15) == 0 ||
              strncmp(node->text, "xl6_xlang_", 10) == 0) ) {
            return 1;
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        if ( xl6CgenNodeNeedsXlangRuntime(node->children[i]) ) {
            return 1;
        }
    }
    return 0;
}
