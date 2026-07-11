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

static int xl6CgenNativeBackedClassHasDestroy(const XL6_AstNode* nativeClassNode)
{
    return xl6CgenFindNativeBackedClassSpecial(nativeClassNode, "destroy") != NULL;
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

static int xl6CgenContainerIsDynamicXValueType(const XL6_AstNode* typeNode, const char* name, const char* legacyName)
{
    const XL6_AstNode* args;

    if ( typeNode == NULL || typeNode->text == NULL || name == NULL ) {
        return 0;
    }
    if ( legacyName != NULL && xl6CTextEquals(typeNode->text, legacyName) ) {
        return 1;
    }
    if ( !xl6CTextEquals(typeNode->text, name) ) {
        return 0;
    }

    /*
        array/list/set/dict 不带泛型参数时才是动态 xvalue 容器。
        一旦写成 array<int> 这类静态泛型容器，就必须走 XRT typed-container 存储。
    */
    args = xl6CgenTypeNodeGenericArgs(typeNode);
    return args == NULL ||
           args->childCount == 0 ||
           (args->childCount == 1 && args->children[0] != NULL &&
            args->children[0]->text != NULL && xl6CTextEquals(args->children[0]->text, "var"));
}

static int xl6CgenIsArrayXValueType(const XL6_AstNode* typeNode)
{
    return xl6CgenContainerIsDynamicXValueType(typeNode, "array", "array<var>");
}

static int xl6CgenIsListXValueType(const XL6_AstNode* typeNode)
{
    return xl6CgenContainerIsDynamicXValueType(typeNode, "list", "list<var>");
}

static int xl6CgenIsSetXValueType(const XL6_AstNode* typeNode)
{
    return xl6CgenContainerIsDynamicXValueType(typeNode, "set", "set<var>");
}

static int xl6CgenIsDictXValueType(const XL6_AstNode* typeNode)
{
    return xl6CgenContainerIsDynamicXValueType(typeNode, "dict", "dict<var>");
}
