static int xl6CgenClassHasNativeHandle(const XL6_AstNode* classNode);
static void xl6CgenCStringSymbol(XL6_Output* out, const char* symbol);
static XL6_AstNode* xl6CgenFindClass(XL6_CgenContext* ctx, const char* name);

static unsigned long long xl6CgenRootTypeHash(const char* text)
{
    unsigned long long hash = 1469598103934665603ULL;

    while ( text != NULL && *text != '\0' ) {
        hash ^= (unsigned char)*text++;
        hash *= 1099511628211ULL;
    }
    return hash;
}

static int xl6CgenBuildRootTypeIndex(XL6_CgenContext* ctx)
{
    size_t typeCount = 0;
    size_t capacity = 8;
    size_t i;

    if ( ctx == NULL || ctx->root == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        if ( node != NULL && node->text != NULL &&
             (node->kind == XL6_AST_CLASS || node->kind == XL6_AST_STRUCT || node->kind == XL6_AST_UNION) ) {
            typeCount += 1;
        }
    }
    if ( typeCount == 0 ) {
        return 1;
    }
    while ( capacity < typeCount * 2 ) {
        capacity *= 2;
    }
    ctx->rootTypeSlots = (XL6_CgenRootTypeSlot*)xrtCalloc(capacity, sizeof(XL6_CgenRootTypeSlot));
    if ( ctx->rootTypeSlots == NULL ) {
        xl6CgenError(ctx, ctx->root, "out of memory while building cgen root type index");
        return 0;
    }
    ctx->rootTypeSlotCount = capacity;

    for ( i = 0; i < ctx->root->childCount; ++i ) {
        XL6_AstNode* node = ctx->root->children[i];
        size_t slot;

        if ( node == NULL || node->text == NULL ||
             (node->kind != XL6_AST_CLASS && node->kind != XL6_AST_STRUCT && node->kind != XL6_AST_UNION) ) {
            continue;
        }
        slot = (size_t)(xl6CgenRootTypeHash(node->text) & (capacity - 1));
        while ( ctx->rootTypeSlots[slot].name != NULL &&
                !xl6CTextEquals(ctx->rootTypeSlots[slot].name, node->text) ) {
            slot = (slot + 1) & (capacity - 1);
        }
        ctx->rootTypeSlots[slot].name = node->text;
        if ( node->kind == XL6_AST_CLASS ) {
            ctx->rootTypeSlots[slot].classNode = node;
        } else if ( node->kind == XL6_AST_STRUCT ) {
            ctx->rootTypeSlots[slot].structNode = node;
        } else {
            ctx->rootTypeSlots[slot].unionNode = node;
        }
    }
    return 1;
}

static XL6_CgenRootTypeSlot* xl6CgenFindRootTypeSlot(XL6_CgenContext* ctx, const char* name)
{
    size_t slot;
    size_t start;

    if ( ctx == NULL || name == NULL || ctx->rootTypeSlotCount == 0 ) {
        return NULL;
    }
    slot = (size_t)(xl6CgenRootTypeHash(name) & (ctx->rootTypeSlotCount - 1));
    start = slot;
    while ( ctx->rootTypeSlots[slot].name != NULL ) {
        if ( xl6CTextEquals(ctx->rootTypeSlots[slot].name, name) ) {
            return &ctx->rootTypeSlots[slot];
        }
        slot = (slot + 1) & (ctx->rootTypeSlotCount - 1);
        if ( slot == start ) {
            break;
        }
    }
    return NULL;
}

static void xl6CgenFreeRootTypeIndex(XL6_CgenContext* ctx)
{
    if ( ctx == NULL ) {
        return;
    }
    xrtFree(ctx->rootTypeSlots);
    ctx->rootTypeSlots = NULL;
    ctx->rootTypeSlotCount = 0;
}

static XL6_AstNode* xl6CgenFindNativeBackedClass(XL6_CgenContext* ctx, const char* name)
{
    XL6_AstNode* node;

    node = xl6CgenFindClass(ctx, name);
    return node != NULL && xl6CgenClassHasNativeHandle(node) ? node : NULL;
}

static const char* xl6CgenLookupCallableName(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_EXPORT_FUNC ||
         node->kind == XL6_AST_NATIVE_EXTERN_SPECIAL ||
         node->kind == XL6_AST_NATIVE_EXTERN_FUNC ) {
        const XL6_AstNode* nameNode = node->childCount > 0 ? node->children[0] : NULL;
        return nameNode != NULL ? nameNode->text : NULL;
    }
    return node->text;
}

static XL6_AstNode* xl6CgenFindClass(XL6_CgenContext* ctx, const char* name)
{
    XL6_CgenRootTypeSlot* slot;

    slot = xl6CgenFindRootTypeSlot(ctx, name);
    return slot != NULL ? slot->classNode : NULL;
}

static XL6_AstNode* xl6CgenFindUnion(XL6_CgenContext* ctx, const char* name)
{
    XL6_CgenRootTypeSlot* slot;

    slot = xl6CgenFindRootTypeSlot(ctx, name);
    return slot != NULL ? slot->unionNode : NULL;
}

static XL6_AstNode* xl6CgenFindStruct(XL6_CgenContext* ctx, const char* name)
{
    XL6_CgenRootTypeSlot* slot;

    slot = xl6CgenFindRootTypeSlot(ctx, name);
    return slot != NULL ? slot->structNode : NULL;
}

static int xl6CgenIsNativeBackedClassType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CgenFindNativeBackedClass(ctx, typeNode->text) != NULL;
}

static int xl6CgenIsClassType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && xl6CgenFindClass(ctx, typeNode->text) != NULL;
}

static int xl6CgenIsUnionType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && typeNode->childCount == 0 &&
           xl6CgenFindUnion(ctx, typeNode->text) != NULL;
}

static int xl6CgenIsStructType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL && typeNode->childCount == 0 &&
           xl6CgenFindStruct(ctx, typeNode->text) != NULL;
}

static int xl6CgenIsPlainRecordType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    return xl6CgenIsStructType(ctx, typeNode) || xl6CgenIsUnionType(ctx, typeNode);
}

static int xl6CgenClassHasTypeParams(const XL6_AstNode* classNode)
{
    const XL6_AstNode* typeParams;

    typeParams = classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
    return typeParams != NULL && typeParams->childCount > 0;
}

static int xl6CgenAstHasFlag(const XL6_AstNode* node, unsigned int flag)
{
    return node != NULL && (node->flags & flag) != 0;
}

static int xl6CgenIsExternClassProperty(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_GET || node->kind == XL6_AST_LET) &&
           xl6CgenAstHasFlag(node, XL6_AST_FLAG_EXTERN);
}

static int xl6CgenIsStaticMember(const XL6_AstNode* node)
{
    return xl6CgenAstHasFlag(node, XL6_AST_FLAG_STATIC);
}

static int xl6CgenIsInstanceField(const XL6_AstNode* node)
{
    return node != NULL && node->kind == XL6_AST_FIELD && !xl6CgenIsStaticMember(node);
}

static const XL6_AstNode* xl6CgenFieldDefaultExpr(const XL6_AstNode* field)
{
    size_t i;

    if ( field == NULL || field->kind != XL6_AST_FIELD ) {
        return NULL;
    }

    /*
        字段 child[0] 固定为类型。
        从 child[1] 开始，可能是默认值，也可能是字段注解。
    */
    for ( i = 1; i < field->childCount; ++i ) {
        const XL6_AstNode* child = field->children[i];
        if ( child != NULL && child->kind != XL6_AST_FIELD_ATTR ) {
            return child;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenClassTypeParams(const XL6_AstNode* classNode)
{
    return classNode != NULL && classNode->childCount > 2 ? classNode->children[2] : NULL;
}

static const XL6_AstNode* xl6CgenFuncTypeParams(const XL6_AstNode* funcNode)
{
    return funcNode != NULL && funcNode->childCount > 4 ? funcNode->children[4] : NULL;
}

static int xl6CgenFuncIsGeneric(const XL6_AstNode* funcNode)
{
    const XL6_AstNode* typeParams;

    typeParams = xl6CgenFuncTypeParams(funcNode);
    return typeParams != NULL && typeParams->childCount > 0;
}

static int xl6CgenIsFuncDeclNode(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_FUNC ||
            node->kind == XL6_AST_ASYNC_FUNC ||
            node->kind == XL6_AST_EXPORT_FUNC);
}

static XL6_AstNode* xl6CgenCloneCanonicalTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* node);

static const XL6_AstNode* xl6CgenSubstituteGenericType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    size_t i;

    if ( ctx == NULL || typeNode == NULL || typeNode->text == NULL ||
         ctx->currentGenericParams == NULL || ctx->currentGenericArgs == NULL ) {
        return typeNode;
    }
    for ( i = 0; i < ctx->currentGenericParams->childCount && i < ctx->currentGenericArgs->childCount; ++i ) {
        const XL6_AstNode* param = ctx->currentGenericParams->children[i];
        if ( param != NULL && xl6CTextEquals(param->text, typeNode->text) ) {
            return ctx->currentGenericArgs->children[i];
        }
    }
    return typeNode;
}

static const XL6_AstNode* xl6CgenClassBody(const XL6_AstNode* classNode)
{
    return classNode != NULL && classNode->childCount > 1 ? classNode->children[1] : NULL;
}

static int xl6CgenBodyHasDerive(const XL6_AstNode* body, const char* ability)
{
    size_t i;

    if ( body == NULL || ability == NULL ) {
        return 0;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_DERIVE &&
             xl6CTextEquals(member->text, ability) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenClassHasDerive(const XL6_AstNode* classNode, const char* ability)
{
    return xl6CgenBodyHasDerive(xl6CgenClassBody(classNode), ability);
}

static int xl6CgenNativeBackedClassHasDerive(const XL6_AstNode* nativeClassNode, const char* ability)
{
    const XL6_AstNode* body;

    body = xl6CgenClassBody(nativeClassNode);
    return xl6CgenBodyHasDerive(body, ability);
}

static const XL6_AstNode* xl6CgenUnionBody(const XL6_AstNode* unionNode)
{
    return unionNode != NULL && unionNode->childCount > 0 ? unionNode->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFindUnionFieldNode(const XL6_AstNode* unionNode, const char* fieldName);

static const XL6_AstNode* xl6CgenStructBody(const XL6_AstNode* structNode)
{
    return structNode != NULL && structNode->childCount > 0 ? structNode->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFindStructFieldNode(const XL6_AstNode* structNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenStructBody(structNode);
    for ( i = 0; body != NULL && fieldName != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL && member->kind == XL6_AST_FIELD && xl6CTextEquals(member->text, fieldName) ) {
            return member;
        }
        if ( member != NULL && member->kind == XL6_AST_UNION && member->text == NULL ) {
            const XL6_AstNode* field = xl6CgenFindUnionFieldNode(member, fieldName);
            if ( field != NULL ) {
                return field;
            }
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindPlainRecordFieldNode(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode,
    const char* fieldName)
{
    if ( xl6CgenIsStructType(ctx, typeNode) ) {
        return xl6CgenFindStructFieldNode(xl6CgenFindStruct(ctx, typeNode->text), fieldName);
    }
    if ( xl6CgenIsUnionType(ctx, typeNode) ) {
        return xl6CgenFindUnionFieldNode(xl6CgenFindUnion(ctx, typeNode->text), fieldName);
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindUnionFieldNode(const XL6_AstNode* unionNode, const char* fieldName)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenUnionBody(unionNode);
    if ( body == NULL || fieldName == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* field = body->children[i];
        if ( field != NULL && field->kind == XL6_AST_FIELD && xl6CTextEquals(field->text, fieldName) ) {
            return field;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassFunc(const XL6_AstNode* classNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* nameNode;
        if ( member == NULL || member->kind != XL6_AST_FUNC || member->childCount == 0 ) {
            continue;
        }
        nameNode = member->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindClassSpecial(const XL6_AstNode* classNode, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             member->kind == XL6_AST_CREATE &&
             xl6CTextEquals(member->text, name) ) {
            return member;
        }
        if ( member != NULL &&
             member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL &&
             xl6CTextEquals(xl6CgenLookupCallableName(member), name) ) {
            return member;
        }
    }
    return NULL;
}

static int xl6CgenClassHasCreate(const XL6_AstNode* classNode)
{
    return xl6CgenFindClassSpecial(classNode, "create") != NULL;
}

static const XL6_AstNode* xl6CgenCallableParams(const XL6_AstNode* node)
{
    if ( node == NULL ) {
        return NULL;
    }

    /*
        不同声明节点的参数表位置不同，这里统一取得可调用参数表。
        普通 func / native-backed extern func: child[1]
        class create: child[0]
        native-backed extern create/destroy: child[1]
    */
    if ( node->kind == XL6_AST_CREATE ) {
        return node->childCount > 0 ? node->children[0] : NULL;
    }
    return node->childCount > 1 ? node->children[1] : NULL;
}


static const XL6_AstNode* xl6CgenFindClassCreateCallableWithoutArgs(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( member != NULL &&
             (member->kind == XL6_AST_CREATE || member->kind == XL6_AST_NATIVE_EXTERN_SPECIAL) &&
             xl6CTextEquals(xl6CgenLookupCallableName(member), "create") &&
             xl6CgenParamRequiredCount(xl6CgenCallableParams(member)) == 0 ) {
            return member;
        }
    }
    return NULL;
}

static int xl6CgenClassHasNativeHandle(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* typeNode;

        if ( member == NULL ||
             member->kind != XL6_AST_FIELD ||
             (member->flags & XL6_AST_FLAG_NATIVE) == 0 ||
             (member->flags & XL6_AST_FLAG_STATIC) != 0 ||
             !xl6CTextEquals(member->text, "handle") ||
             member->childCount == 0 ) {
            continue;
        }
        typeNode = member->children[0];
        return typeNode != NULL && xl6CTextEquals(typeNode->text, "point");
    }
    return 0;
}

static int xl6CgenClassIsPureNativeHandle(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;
    int hasHandle;

    if ( !xl6CgenClassHasNativeHandle(classNode) ) {
        return 0;
    }

    body = xl6CgenClassBody(classNode);
    hasHandle = 0;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];

        if ( member == NULL || member->kind != XL6_AST_FIELD || xl6CgenIsStaticMember(member) ) {
            continue;
        }
        if ( (member->flags & XL6_AST_FLAG_NATIVE) != 0 &&
             xl6CTextEquals(member->text, "handle") ) {
            hasHandle = 1;
            continue;
        }

        /*
            只有纯句柄类使用 handle type-desc。
            class 一旦携带额外 xlang 字段，就必须保留普通 record 语义。
        */
        return 0;
    }
    return hasHandle;
}

static const XL6_AstNode* xl6CgenFindNativeBackedClassMember(const XL6_AstNode* nativeClassNode, XL6_AstKind kind, const char* name)
{
    const XL6_AstNode* body;
    size_t i;

    if ( nativeClassNode == NULL || name == NULL ) {
        return NULL;
    }
    body = xl6CgenClassBody(nativeClassNode);
    if ( body == NULL ) {
        return NULL;
    }
    for ( i = 0; i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        const XL6_AstNode* nameNode;
        if ( member == NULL || member->kind != kind || member->childCount == 0 ) {
            continue;
        }
        nameNode = member->children[0];
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return member;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenFindNativeBackedClassSpecial(const XL6_AstNode* nativeClassNode, const char* name)
{
    return xl6CgenFindNativeBackedClassMember(nativeClassNode, XL6_AST_NATIVE_EXTERN_SPECIAL, name);
}

static const XL6_AstNode* xl6CgenFindNativeBackedClassExternFunc(const XL6_AstNode* nativeClassNode, const char* name)
{
    return xl6CgenFindNativeBackedClassMember(nativeClassNode, XL6_AST_NATIVE_EXTERN_FUNC, name);
}

static int xl6CgenIsExternalVarDecl(const XL6_AstNode* node)
{
    return node != NULL &&
           (node->kind == XL6_AST_EXTERN_VAR || node->kind == XL6_AST_EXPORT_VAR || node->kind == XL6_AST_EXPORT_CONST);
}

static const XL6_AstNode* xl6CgenExternalVarNameNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenExternalVarTypeNode(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 1 ? node->children[1] : NULL;
}

static const XL6_AstNode* xl6CgenExternalVarInitExpr(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 2 ? node->children[2] : NULL;
}

static int xl6CgenExternalVarHasCSymbol(const XL6_AstNode* node)
{
    return node != NULL && node->text != NULL && node->text[0] != '\0';
}

static const char* xl6CgenExternalVarLocalName(const XL6_AstNode* node)
{
    const XL6_AstNode* nameNode;

    nameNode = xl6CgenExternalVarNameNode(node);
    return nameNode != NULL ? nameNode->text : NULL;
}

static void xl6CgenExternalVarStorageNameText(
    char* buffer,
    size_t capacity,
    const XL6_AstNode* node)
{
    const char* text;
    size_t len;

    if ( buffer == NULL || capacity == 0 ) {
        return;
    }
    buffer[0] = '\0';
    if ( !xl6CgenExternalVarHasCSymbol(node) ) {
        xl6CgenCIdentText(buffer, capacity, xl6CgenExternalVarLocalName(node));
        return;
    }

    text = node->text;
    len = strlen(text);
    if ( len >= 2 && (text[0] == '"' || text[0] == '\'') ) {
        snprintf(buffer, capacity, "%.*s", (int)(len - 2), text + 1);
        return;
    }
    snprintf(buffer, capacity, "%s", text);
}

static void xl6CgenExternalVarStorageName(XL6_Output* out, const XL6_AstNode* node)
{
    char storageName[512];

    xl6CgenExternalVarStorageNameText(storageName, sizeof(storageName), node);
    xl6OutputPuts(storageName, out);
}

static int xl6CgenIsTypeAliasDecl(const XL6_AstNode* node)
{
    return node != NULL && (node->kind == XL6_AST_TYPE_ALIAS || node->kind == XL6_AST_EXTERN_TYPE_ALIAS);
}

static const XL6_AstNode* xl6CgenTypeAliasTargetType(const XL6_AstNode* node)
{
    return node != NULL && node->childCount > 0 ? node->children[0] : NULL;
}

static const XL6_AstNode* xl6CgenFindTypeAlias(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        if ( xl6CgenIsTypeAliasDecl(node) && xl6CTextEquals(node->text, name) ) {
            return node;
        }
    }
    return NULL;
}

static const XL6_AstNode* xl6CgenResolveAliasType(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    int guard;

    for ( guard = 0; guard < 32; ++guard ) {
        const XL6_AstNode* aliasNode;
        if ( typeNode == NULL || typeNode->kind != XL6_AST_TYPE || typeNode->text == NULL || typeNode->childCount > 0 ) {
            return typeNode;
        }
        aliasNode = xl6CgenFindTypeAlias(ctx, typeNode->text);
        if ( aliasNode == NULL ) {
            return typeNode;
        }
        typeNode = xl6CgenTypeAliasTargetType(aliasNode);
    }
    return typeNode;
}

static const XL6_AstNode* xl6CgenFindExternalVar(XL6_CgenContext* ctx, const char* name)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL || name == NULL ) {
        return NULL;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* node = ctx->root->children[i];
        const XL6_AstNode* nameNode;
        if ( !xl6CgenIsExternalVarDecl(node) ) {
            continue;
        }
        nameNode = xl6CgenExternalVarNameNode(node);
        if ( nameNode != NULL && xl6CTextEquals(nameNode->text, name) ) {
            return node;
        }
    }
    return NULL;
}
