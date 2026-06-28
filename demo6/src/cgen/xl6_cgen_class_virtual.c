static int xl6CgenVirtualMemberKind(const XL6_AstNode* member)
{
    return member != NULL &&
           (member->kind == XL6_AST_FUNC ||
            member->kind == XL6_AST_GET ||
            member->kind == XL6_AST_LET);
}

static int xl6CgenVirtualMemberIsSlot(const XL6_AstNode* member)
{
    return xl6CgenVirtualMemberKind(member) &&
           !xl6CgenIsStaticMember(member) &&
           xl6CgenAstHasFlag(member, XL6_AST_FLAG_VIRTUAL) &&
           !xl6CgenAstHasFlag(member, XL6_AST_FLAG_OVERRIDE);
}

static int xl6CgenVirtualMemberIsDispatchable(const XL6_AstNode* member)
{
    return xl6CgenVirtualMemberKind(member) &&
           !xl6CgenIsStaticMember(member) &&
           (xl6CgenAstHasFlag(member, XL6_AST_FLAG_VIRTUAL) ||
            xl6CgenAstHasFlag(member, XL6_AST_FLAG_OVERRIDE));
}

static int xl6CgenClassHasLocalVirtualSlots(const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        if ( xl6CgenVirtualMemberIsSlot(body->children[i]) ) {
            return 1;
        }
    }
    return 0;
}

static void xl6CgenVirtualTableTypeName(XL6_Output* out, const XL6_AstNode* ownerClass)
{
    xl6OutputPuts("XL6_VTable_", out);
    xl6CgenFputsEncodedText(out, ownerClass != NULL ? ownerClass->text : "_");
}

static void xl6CgenVirtualTableObjectName(XL6_Output* out, const XL6_AstNode* ownerClass, const XL6_AstNode* implClass)
{
    xl6OutputPuts("xl6_vtable_", out);
    xl6CgenFputsEncodedText(out, ownerClass != NULL ? ownerClass->text : "_");
    xl6OutputPuts("__", out);
    xl6CgenFputsEncodedText(out, implClass != NULL ? implClass->text : "_");
}

static void xl6CgenVirtualTableFieldName(XL6_Output* out, const XL6_AstNode* ownerClass)
{
    xl6OutputPuts("__xl6_vt_", out);
    xl6CgenFputsEncodedText(out, ownerClass != NULL ? ownerClass->text : "_");
}

static void xl6CgenVirtualSlotName(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* params;
    const XL6_AstNode* returns;
    const char* name;
    unsigned long long sig;

    params = xl6CgenCallableParams(member);
    returns = member != NULL && member->childCount > 2 ? member->children[2] : NULL;
    sig = xl6CgenFunctionSignatureId(ctx, params, returns);
    name = xl6CgenCallableName(member);

    xl6OutputPuts("__xl6_vslot_", ctx->out);
    if ( member != NULL && member->kind == XL6_AST_GET ) {
        xl6OutputPuts("get__", ctx->out);
    } else if ( member != NULL && member->kind == XL6_AST_LET ) {
        xl6OutputPuts("let__", ctx->out);
    }
    xl6CgenFputsEncodedText(ctx->out, name != NULL ? name : "_");
    xl6OutputPrintf(ctx->out, "__%llx", sig);
}

static int xl6CgenCanonicalTypeEquals(XL6_CgenContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    XL6_AstNode* ca;
    XL6_AstNode* cb;
    int result;

    ca = xl6CgenCloneCanonicalTypeNode(ctx, a);
    cb = xl6CgenCloneCanonicalTypeNode(ctx, b);
    if ( ca == NULL || cb == NULL ) {
        result = ca == cb;
    } else {
        result = xl6CgenTypeNodeEqual(ca, cb);
    }
    xl6AstFree(ca);
    xl6AstFree(cb);
    return result;
}

static int xl6CgenVirtualParamListsEqual(XL6_CgenContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t aCount;
    size_t bCount;
    size_t i;

    aCount = a != NULL ? a->childCount : 0;
    bCount = b != NULL ? b->childCount : 0;
    if ( aCount != bCount ||
         xl6CgenParamRequiredCount(a) != xl6CgenParamRequiredCount(b) ||
         xl6CgenFixedParamCount(a) != xl6CgenFixedParamCount(b) ) {
        return 0;
    }

    for ( i = 0; i < aCount; ++i ) {
        const XL6_AstNode* aParam = a->children[i];
        const XL6_AstNode* bParam = b->children[i];
        const XL6_AstNode* aType;
        const XL6_AstNode* bType;

        if ( xl6CgenParamIsArrayVararg(aParam) != xl6CgenParamIsArrayVararg(bParam) ||
             xl6CgenParamIsDictVararg(aParam) != xl6CgenParamIsDictVararg(bParam) ) {
            return 0;
        }
        aType = aParam != NULL && aParam->childCount > 0 ? aParam->children[0] : NULL;
        bType = bParam != NULL && bParam->childCount > 0 ? bParam->children[0] : NULL;
        if ( !xl6CgenCanonicalTypeEquals(ctx, aType, bType) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenVirtualReturnListsEqual(XL6_CgenContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    size_t aCount;
    size_t bCount;
    size_t i;

    aCount = a != NULL ? a->childCount : 0;
    bCount = b != NULL ? b->childCount : 0;
    if ( aCount != bCount ) {
        return 0;
    }
    for ( i = 0; i < aCount; ++i ) {
        if ( !xl6CgenCanonicalTypeEquals(ctx, a->children[i], b->children[i]) ) {
            return 0;
        }
    }
    return 1;
}

static int xl6CgenVirtualSignaturesEqual(XL6_CgenContext* ctx, const XL6_AstNode* a, const XL6_AstNode* b)
{
    const char* aName;
    const char* bName;

    if ( a == NULL || b == NULL || a->kind != b->kind ) {
        return 0;
    }
    aName = xl6CgenCallableName(a);
    bName = xl6CgenCallableName(b);
    if ( aName == NULL || bName == NULL || !xl6CTextEquals(aName, bName) ) {
        return 0;
    }
    return xl6CgenVirtualParamListsEqual(ctx, xl6CgenCallableParams(a), xl6CgenCallableParams(b)) &&
           xl6CgenVirtualReturnListsEqual(ctx,
                                          a->childCount > 2 ? a->children[2] : NULL,
                                          b->childCount > 2 ? b->children[2] : NULL);
}

static const XL6_AstNode* xl6CgenFindBaseVirtualTarget(
    XL6_CgenContext* ctx,
    const XL6_AstNode* baseClass,
    const XL6_AstNode* member,
    const XL6_AstNode** ownerClass)
{
    const XL6_AstNode* body;
    XL6_AstNode* nextBase;
    size_t i;

    if ( ownerClass != NULL ) {
        *ownerClass = NULL;
    }
    body = xl6CgenClassBody(baseClass);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* candidate = body->children[i];
        if ( xl6CgenIsStaticMember(candidate) ) {
            continue;
        }
        if ( xl6CgenVirtualSignaturesEqual(ctx, member, candidate) ) {
            if ( ownerClass != NULL ) {
                *ownerClass = baseClass;
            }
            return candidate;
        }
    }

    nextBase = xl6CgenBaseClass(ctx, baseClass);
    return nextBase != NULL ? xl6CgenFindBaseVirtualTarget(ctx, nextBase, member, ownerClass) : NULL;
}

static const XL6_AstNode* xl6CgenVirtualSlotOwner(
    XL6_CgenContext* ctx,
    const XL6_AstNode* memberOwnerClass,
    const XL6_AstNode* member,
    const XL6_AstNode** outSlot)
{
    const XL6_AstNode* baseTargetOwner;
    const XL6_AstNode* baseTarget;

    if ( outSlot != NULL ) {
        *outSlot = member;
    }
    if ( memberOwnerClass == NULL || member == NULL || !xl6CgenVirtualMemberIsDispatchable(member) ) {
        return NULL;
    }
    if ( xl6CgenVirtualMemberIsSlot(member) ) {
        return memberOwnerClass;
    }

    baseTargetOwner = NULL;
    baseTarget = xl6CgenFindBaseVirtualTarget(ctx, xl6CgenBaseClass(ctx, memberOwnerClass), member, &baseTargetOwner);
    if ( baseTarget == NULL || baseTargetOwner == NULL ) {
        return NULL;
    }
    return xl6CgenVirtualSlotOwner(ctx, baseTargetOwner, baseTarget, outSlot);
}

static const XL6_AstNode* xl6CgenFindVirtualImplementation(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* slot,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* body;
    XL6_AstNode* baseClass;
    size_t i;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( xl6CgenIsStaticMember(member) ) {
            continue;
        }
        if ( xl6CgenVirtualSignaturesEqual(ctx, member, slot) ) {
            if ( outOwnerClass != NULL ) {
                *outOwnerClass = classNode;
            }
            return member;
        }
    }
    baseClass = xl6CgenBaseClass(ctx, classNode);
    return baseClass != NULL ? xl6CgenFindVirtualImplementation(ctx, baseClass, slot, outOwnerClass) : NULL;
}

static void xl6CgenVirtualReturnType(XL6_CgenContext* ctx, const XL6_AstNode* member)
{
    const XL6_AstNode* returns;

    returns = member != NULL && member->childCount > 2 ? member->children[2] : NULL;
    if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFuncReturnStructName(ctx, member);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
}

static void xl6CgenVirtualFunctionPointerType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* slotOwnerClass,
    const XL6_AstNode* slot)
{
    xl6CgenVirtualReturnType(ctx, slot);
    xl6OutputPuts(" (*)(", ctx->out);
    xl6CgenClassParamListEx(ctx, slotOwnerClass, xl6CgenCallableParams(slot), 1);
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenVirtualFunctionPointerDecl(
    XL6_CgenContext* ctx,
    const XL6_AstNode* slotOwnerClass,
    const XL6_AstNode* slot)
{
    xl6CgenVirtualReturnType(ctx, slot);
    xl6OutputPuts(" (*", ctx->out);
    xl6CgenVirtualSlotName(ctx, slot);
    xl6OutputPuts(")(", ctx->out);
    xl6CgenClassParamListEx(ctx, slotOwnerClass, xl6CgenCallableParams(slot), 1);
    xl6OutputPuts(")", ctx->out);
}

static void xl6CgenVirtualFieldDecl(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    if ( !xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        return;
    }
    xl6OutputPuts("    const struct ", ctx->out);
    xl6CgenVirtualTableTypeName(ctx->out, classNode);
    xl6OutputPuts("* ", ctx->out);
    xl6CgenVirtualTableFieldName(ctx->out, classNode);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenVirtualTableStructDecl(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    const XL6_AstNode* body;
    size_t i;

    if ( !xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        return;
    }

    xl6OutputPuts("struct ", ctx->out);
    xl6CgenVirtualTableTypeName(ctx->out, classNode);
    xl6OutputPuts(" {\n", ctx->out);
    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* member = body->children[i];
        if ( !xl6CgenVirtualMemberIsSlot(member) ) {
            continue;
        }
        xl6OutputPuts("    ", ctx->out);
        xl6CgenVirtualFunctionPointerDecl(ctx, classNode, member);
        xl6OutputPuts(";\n", ctx->out);
    }
    xl6OutputPuts("};\n", ctx->out);
}

static void xl6CgenVirtualTableObjectDeclForOwner(
    XL6_CgenContext* ctx,
    const XL6_AstNode* dynamicClass,
    const XL6_AstNode* slotOwnerClass)
{
    const XL6_AstNode* body;
    size_t i;
    int wrote;

    if ( !xl6CgenClassHasLocalVirtualSlots(slotOwnerClass) ) {
        return;
    }

    xl6OutputPuts("static const struct ", ctx->out);
    xl6CgenVirtualTableTypeName(ctx->out, slotOwnerClass);
    xl6OutputPutc(' ', ctx->out);
    xl6CgenVirtualTableObjectName(ctx->out, slotOwnerClass, dynamicClass);
    xl6OutputPuts(" = {\n", ctx->out);

    body = xl6CgenClassBody(slotOwnerClass);
    wrote = 0;
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* slot = body->children[i];
        const XL6_AstNode* implOwner;
        const XL6_AstNode* impl;

        if ( !xl6CgenVirtualMemberIsSlot(slot) ) {
            continue;
        }
        implOwner = NULL;
        impl = xl6CgenFindVirtualImplementation(ctx, dynamicClass, slot, &implOwner);
        if ( impl == NULL || implOwner == NULL ) {
            impl = slot;
            implOwner = slotOwnerClass;
        }
        xl6OutputPuts("    .", ctx->out);
        xl6CgenVirtualSlotName(ctx, slot);
        xl6OutputPuts(" = ", ctx->out);
        if ( implOwner != slotOwnerClass ) {
            xl6OutputPutc('(', ctx->out);
            xl6CgenVirtualFunctionPointerType(ctx, slotOwnerClass, slot);
            xl6OutputPutc(')', ctx->out);
        }
        xl6CgenClassCallableSymbol(ctx, implOwner, impl);
        xl6OutputPuts(",\n", ctx->out);
        wrote = 1;
    }
    (void)wrote;
    xl6OutputPuts("};\n", ctx->out);
}

static void xl6CgenVirtualTableObjectDeclsForChain(
    XL6_CgenContext* ctx,
    const XL6_AstNode* dynamicClass,
    const XL6_AstNode* classNode)
{
    XL6_AstNode* baseClass;

    if ( classNode == NULL ) {
        return;
    }
    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass != NULL ) {
        xl6CgenVirtualTableObjectDeclsForChain(ctx, dynamicClass, baseClass);
    }
    xl6CgenVirtualTableObjectDeclForOwner(ctx, dynamicClass, classNode);
}

static void xl6CgenVirtualTableObjectDecls(XL6_CgenContext* ctx, const XL6_AstNode* classNode)
{
    xl6CgenVirtualTableObjectDeclsForChain(ctx, classNode, classNode);
}

static void xl6CgenClassInitVTablesForChain(
    XL6_CgenContext* ctx,
    const XL6_AstNode* dynamicClass,
    const XL6_AstNode* classNode,
    const char* varPath)
{
    XL6_AstNode* baseClass;
    char basePath[512];

    if ( classNode == NULL || varPath == NULL ) {
        return;
    }

    baseClass = xl6CgenBaseClass(ctx, classNode);
    if ( baseClass != NULL ) {
        snprintf(basePath, sizeof(basePath), "%s.base", varPath);
        xl6CgenClassInitVTablesForChain(ctx, dynamicClass, baseClass, basePath);
    }

    if ( !xl6CgenClassHasLocalVirtualSlots(classNode) ) {
        return;
    }
    xl6CgenIndent(ctx);
    xl6OutputPuts(varPath, ctx->out);
    xl6OutputPutc('.', ctx->out);
    xl6CgenVirtualTableFieldName(ctx->out, classNode);
    xl6OutputPuts(" = &", ctx->out);
    xl6CgenVirtualTableObjectName(ctx->out, classNode, dynamicClass);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenClassInitVTables(XL6_CgenContext* ctx, const XL6_AstNode* classNode, const char* varPath)
{
    xl6CgenClassInitVTablesForChain(ctx, classNode, classNode, varPath);
}

static int xl6CgenVirtualEmitReceiverObject(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const XL6_AstNode* fromClass,
    const XL6_AstNode* targetClass)
{
    char basePath[256];

    if ( receiver == NULL || fromClass == NULL || targetClass == NULL ) {
        return 0;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, fromClass, targetClass, basePath, sizeof(basePath)) ) {
        return 0;
    }
    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        if ( basePath[0] == '\0' ) {
            xl6OutputPuts("(*self)", ctx->out);
        } else {
            xl6OutputPuts("self->base", ctx->out);
            if ( strlen(basePath) > 5 ) {
                xl6OutputPuts(basePath + 5, ctx->out);
            }
        }
        return 1;
    }
    if ( receiver->kind == XL6_AST_NAME ) {
        xl6CgenNamedValueReference(ctx, receiver->text);
        xl6OutputPuts(basePath, ctx->out);
        return 1;
    }
    return 0;
}

static int xl6CgenVirtualEmitReceiverPointer(
    XL6_CgenContext* ctx,
    const XL6_AstNode* receiver,
    const XL6_AstNode* fromClass,
    const XL6_AstNode* targetClass)
{
    char basePath[256];

    if ( receiver == NULL || fromClass == NULL || targetClass == NULL ) {
        return 0;
    }
    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, fromClass, targetClass, basePath, sizeof(basePath)) ) {
        return 0;
    }
    if ( receiver->kind == XL6_AST_NAME && xl6CTextEquals(receiver->text, "self") && ctx->currentClass != NULL ) {
        if ( basePath[0] == '\0' ) {
            xl6OutputPuts("self", ctx->out);
        } else {
            xl6OutputPuts("&self->base", ctx->out);
            if ( strlen(basePath) > 5 ) {
                xl6OutputPuts(basePath + 5, ctx->out);
            }
        }
        return 1;
    }
    if ( receiver->kind == XL6_AST_NAME ) {
        xl6OutputPutc('&', ctx->out);
        xl6CgenNamedValueReference(ctx, receiver->text);
        xl6OutputPuts(basePath, ctx->out);
        return 1;
    }
    return 0;
}

static int xl6CgenVirtualClassCallableCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* receiver,
    const XL6_AstNode* classNode,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* method)
{
    const XL6_AstNode* slot;
    const XL6_AstNode* slotOwner;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    int fixedCount;

    slot = NULL;
    slotOwner = xl6CgenVirtualSlotOwner(ctx, ownerClass, method, &slot);
    if ( slotOwner == NULL || slot == NULL ) {
        return 0;
    }
    if ( !xl6CgenVirtualEmitReceiverObject(ctx, receiver, classNode, slotOwner) ) {
        return 0;
    }
    xl6OutputPutc('.', ctx->out);
    xl6CgenVirtualTableFieldName(ctx->out, slotOwner);
    xl6OutputPuts("->", ctx->out);
    xl6CgenVirtualSlotName(ctx, slot);
    xl6OutputPutc('(', ctx->out);
    if ( !xl6CgenVirtualEmitReceiverPointer(ctx, receiver, classNode, slotOwner) ) {
        xl6OutputPuts("0", ctx->out);
    }
    params = xl6CgenCallableParams(method);
    fixedCount = xl6CgenFixedParamCount(params);
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        args = call != NULL && call->childCount > 1 ? call->children[1] : NULL;
        xl6CgenKnownCallArgs(ctx, method, args);
    }
    xl6OutputPutc(')', ctx->out);
    return 1;
}

static int xl6CgenVirtualClassMethodCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* member,
    const XL6_AstNode* receiver,
    const XL6_AstNode* classNode,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* method)
{
    (void)member;
    return method != NULL && method->kind == XL6_AST_FUNC &&
           xl6CgenVirtualClassCallableCall(ctx, call, receiver, classNode, ownerClass, method);
}

static int xl6CgenVirtualClassPropertyCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member,
    const XL6_AstNode* call,
    const XL6_AstNode* receiver,
    const XL6_AstNode* classNode,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* method)
{
    (void)member;
    return method != NULL && (method->kind == XL6_AST_GET || method->kind == XL6_AST_LET) &&
           xl6CgenVirtualClassCallableCall(ctx, call, receiver, classNode, ownerClass, method);
}
