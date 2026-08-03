/*
 * nullable 和 weak 是规范类型，不是用户可注册的普通泛型。
 * 后端只读取语义阶段建立的内部节点，不接受 nullable<T> 兼容写法。
 */

static int xl6CgenIsNullableType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL &&
           typeNode->kind == XL6_AST_TYPE &&
           typeNode->text != NULL &&
           xl6CTextEquals(typeNode->text, "nullable") &&
           (typeNode->flags & XL6_AST_FLAG_INTERNAL_NULLABLE) != 0;
}

static const XL6_AstNode* xl6CgenNullableInnerType(const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;

    if ( !xl6CgenIsNullableType(typeNode) || typeNode->childCount != 1 ) {
        return NULL;
    }
    args = typeNode->children[0];
    return args != NULL && args->kind == XL6_AST_LIST && args->childCount == 1
        ? args->children[0]
        : NULL;
}

static int xl6CgenNullableUsesXValue(const XL6_AstNode* typeNode)
{
    const XL6_AstNode* inner = xl6CgenNullableInnerType(typeNode);

    return inner != NULL && inner->text != NULL &&
           (xl6CTextEquals(inner->text, "__class_handle") ||
            (inner->flags & XL6_AST_FLAG_PROTOCOL_HANDLE) != 0);
}

static int xl6CgenNullableUsesPointer(const XL6_AstNode* typeNode)
{
    const XL6_AstNode* inner = xl6CgenNullableInnerType(typeNode);

    if ( inner == NULL || inner->text == NULL ) {
        return 0;
    }
    return xl6CTextEquals(inner->text, "string") ||
           xl6CTextEquals(inner->text, "point") ||
           xl6CTextEquals(inner->text, "array") ||
           xl6CTextEquals(inner->text, "array<var>") ||
           xl6CTextEquals(inner->text, "list") ||
           xl6CTextEquals(inner->text, "list<var>") ||
           xl6CTextEquals(inner->text, "set") ||
           xl6CTextEquals(inner->text, "set<var>") ||
           xl6CTextEquals(inner->text, "dict") ||
           xl6CTextEquals(inner->text, "dict<var>") ||
           xl6CTextEquals(inner->text, "future") ||
           xl6CTextEquals(inner->text, "task") ||
           xl6CTextEquals(inner->text, "generator") ||
           xl6CTextEquals(inner->text, "error") ||
           xl6CTextEquals(inner->text, "thread") ||
           xl6CTextEquals(inner->text, "mutex") ||
           xl6CTextEquals(inner->text, "cond") ||
           xl6CTextEquals(inner->text, "rwlock") ||
           xl6CTextEquals(inner->text, "sem");
}

static int xl6CgenNullableUsesTaggedValue(const XL6_AstNode* typeNode)
{
    return xl6CgenIsNullableType(typeNode) &&
           !xl6CgenNullableUsesXValue(typeNode) &&
           !xl6CgenNullableUsesPointer(typeNode);
}

static void xl6CgenNullableHashBytes(
    unsigned long long* hash,
    const char* text)
{
    const unsigned char* p = (const unsigned char*)text;

    if ( hash == NULL || text == NULL ) {
        return;
    }
    while ( *p != 0 ) {
        *hash ^= (unsigned long long)*p++;
        *hash *= 1099511628211ULL;
    }
}

static void xl6CgenNullableHashType(
    unsigned long long* hash,
    const XL6_AstNode* typeNode)
{
    size_t i;

    if ( hash == NULL || typeNode == NULL ) {
        xl6CgenNullableHashBytes(hash, "<null>");
        return;
    }
    xl6CgenNullableHashBytes(hash, typeNode->text != NULL ? typeNode->text : "_");
    xl6CgenNullableHashBytes(hash, "<");
    for ( i = 0; i < typeNode->childCount; ++i ) {
        xl6CgenNullableHashType(hash, typeNode->children[i]);
        xl6CgenNullableHashBytes(hash, ",");
    }
    xl6CgenNullableHashBytes(hash, ">");
}

static unsigned long long xl6CgenNullableTypeId(const XL6_AstNode* typeNode)
{
    unsigned long long hash;

    /*
     * nullable helper 是模块内部 C 符号，不是公开 ABI。
     * 同一规范类型可能来自声明 AST、HIR 循环目标或泛型替换结果，其中只有
     * 一部分节点携带 semanticTypeAbiId。符号名若读取这个可选缓存，会让
     * copy/drop 对同一类型生成不同名字。结构哈希不读取阶段性元数据。
     */
    hash = 1469598103934665603ULL;
    xl6CgenNullableHashType(&hash, typeNode);
    return hash != 0 ? hash : 1;
}

static void xl6CgenNullableCTypeName(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* typeNode)
{
    unsigned long long abiId;

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    abiId = xl6CgenNullableTypeId(typeNode);
    snprintf(buffer, capacity, "XL6_Nullable_%016llx", abiId);
}

static int xl6CgenIsWeakType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL &&
           typeNode->kind == XL6_AST_TYPE &&
           typeNode->text != NULL &&
           xl6CTextEquals(typeNode->text, "weak");
}

static const XL6_AstNode* xl6CgenWeakTargetType(const XL6_AstNode* typeNode)
{
    const XL6_AstNode* args;

    if ( !xl6CgenIsWeakType(typeNode) || typeNode->childCount != 1 ) {
        return NULL;
    }
    args = typeNode->children[0];
    return args != NULL && args->kind == XL6_AST_LIST && args->childCount == 1
        ? args->children[0]
        : NULL;
}
