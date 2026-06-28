static void xl6CgenCStringSymbol(XL6_Output* out, const char* text)
{
    size_t len;
    size_t i;

    if ( text == NULL ) {
        xl6OutputPuts("_", out);
        return;
    }

    len = strlen(text);
    if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
        for ( i = 1; i + 1 < len; ++i ) {
            xl6OutputPutc(text[i], out);
        }
        return;
    }
    xl6OutputPuts(text, out);
}

static int xl6CgenCustomHasDestroy(const XL6_AstNode* customNode)
{
    return xl6CgenFindCustomSpecial(customNode, "destroy") != NULL;
}

static int xl6CgenClassHasDestroy(const XL6_AstNode* classNode)
{
    return xl6CgenFindClassSpecial(classNode, "destroy") != NULL;
}

static int xl6CgenIsFunctionType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CTextEquals(typeNode->text, "function");
}

static int xl6CgenIsBareFunctionType(const XL6_AstNode* typeNode)
{
    return xl6CgenIsFunctionType(typeNode) && typeNode->childCount == 0;
}

static int xl6CgenTypeContainsFunctionType(const XL6_AstNode* typeNode)
{
    size_t i;

    if ( xl6CgenIsFunctionType(typeNode) ) {
        return 1;
    }
    for ( i = 0; typeNode != NULL && i < typeNode->childCount; ++i ) {
        if ( xl6CgenTypeContainsFunctionType(typeNode->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenIsArrayXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "array") || xl6CTextEquals(typeNode->text, "array<var>"));
}

static int xl6CgenIsListXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "list") || xl6CTextEquals(typeNode->text, "list<var>"));
}

static int xl6CgenIsSetXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "set") || xl6CTextEquals(typeNode->text, "set<var>"));
}

static int xl6CgenIsDictXValueType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "dict") || xl6CTextEquals(typeNode->text, "dict<var>"));
}
