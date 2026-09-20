static int xl6CgenTypeIsBool(XL6_CgenContext* ctx, const XL6_AstNode* typeNode)
{
    const XL6_AstNode* concreteType;

    concreteType = xl6CgenSubstituteGenericType(ctx, typeNode);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    return concreteType != NULL && concreteType->text != NULL && xl6CTextEquals(concreteType->text, "bool");
}

static int xl6CgenResolveClassIterableType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* iterableType,
    const XL6_AstNode** outIterableClass,
    const XL6_AstNode** outIterMethod,
    const XL6_AstNode** outIterOwnerClass,
    const XL6_AstNode** outIteratorType,
    const XL6_AstNode** outIteratorClass,
    const XL6_AstNode** outNextMethod,
    const XL6_AstNode** outNextOwnerClass)
{
    const XL6_AstNode* concreteType;
    const XL6_AstNode* iterableClass;
    const XL6_AstNode* iterOwnerClass;
    const XL6_AstNode* iterMethod;
    const XL6_AstNode* iterReturns;
    const XL6_AstNode* iteratorType;
    const XL6_AstNode* iteratorClassType;
    const XL6_AstNode* iteratorClass;
    const XL6_AstNode* nextOwnerClass;
    const XL6_AstNode* nextMethod;
    const XL6_AstNode* nextReturns;
    const XL6_AstNode* iterableTypeArgs;
    const XL6_AstNode* iteratorTypeArgs;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* ownedIteratorType;

    if ( outIterableClass != NULL ) {
        *outIterableClass = NULL;
    }
    if ( outIterMethod != NULL ) {
        *outIterMethod = NULL;
    }
    if ( outIterOwnerClass != NULL ) {
        *outIterOwnerClass = NULL;
    }
    if ( outIteratorType != NULL ) {
        *outIteratorType = NULL;
    }
    if ( outIteratorClass != NULL ) {
        *outIteratorClass = NULL;
    }
    if ( outNextMethod != NULL ) {
        *outNextMethod = NULL;
    }
    if ( outNextOwnerClass != NULL ) {
        *outNextOwnerClass = NULL;
    }

    concreteType = xl6CgenSubstituteGenericType(ctx, iterableType);
    concreteType = xl6CgenResolveAliasType(ctx, concreteType);
    if ( xl6CgenClassHandleTargetType(ctx, concreteType) != NULL ) {
        concreteType = xl6CgenClassHandleTargetType(ctx, concreteType);
    }
    if ( concreteType == NULL || concreteType->text == NULL ) {
        return 0;
    }

    iterableClass = xl6CgenFindClass(ctx, concreteType->text);
    if ( iterableClass == NULL ) {
        return 0;
    }
    iterableTypeArgs = xl6CgenTypeNodeGenericArgs(concreteType);
    if ( xl6CgenClassHasTypeParams(iterableClass) && iterableTypeArgs == NULL ) {
        return 0;
    }

    iterOwnerClass = NULL;
    iterMethod = xl6CgenFindClassProtocolFunc(ctx, iterableClass, "__iter__", &iterOwnerClass);
    iterReturns = iterMethod != NULL && iterMethod->childCount > 2 ? iterMethod->children[2] : NULL;
    if ( iterReturns == NULL || iterReturns->childCount != 1 ) {
        return 0;
    }
    if ( iterOwnerClass == NULL ) {
        iterOwnerClass = iterableClass;
    }

    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( iterableTypeArgs != NULL && xl6CgenClassHasTypeParams(iterableClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(iterableClass);
        ctx->currentGenericArgs = iterableTypeArgs;
    }
    ownedIteratorType = xl6CgenCloneCanonicalTypeNode(ctx, iterReturns->children[0]);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;

    iteratorType = xl6CgenResolveAliasType(ctx, ownedIteratorType);
    if ( iteratorType == NULL || iteratorType->text == NULL ) {
        xl6AstFree(ownedIteratorType);
        return 0;
    }
    iteratorClassType = xl6CgenClassHandleTargetType(ctx, iteratorType);
    if ( iteratorClassType == NULL ) {
        iteratorClassType = iteratorType;
    }
    iteratorClass = xl6CgenFindClass(ctx, iteratorClassType->text);
    if ( iteratorClass == NULL ) {
        xl6AstFree(ownedIteratorType);
        return 0;
    }
    iteratorTypeArgs = xl6CgenTypeNodeGenericArgs(iteratorClassType);
    if ( xl6CgenClassHasTypeParams(iteratorClass) && iteratorTypeArgs == NULL ) {
        xl6AstFree(ownedIteratorType);
        return 0;
    }

    nextOwnerClass = NULL;
    nextMethod = xl6CgenFindClassProtocolFunc(ctx, iteratorClass, "__next__", &nextOwnerClass);
    nextReturns = nextMethod != NULL && nextMethod->childCount > 2 ? nextMethod->children[2] : NULL;
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( iteratorTypeArgs != NULL && xl6CgenClassHasTypeParams(iteratorClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(iteratorClass);
        ctx->currentGenericArgs = iteratorTypeArgs;
    }
    if ( nextReturns == NULL || nextReturns->childCount != 2 ||
         !xl6CgenTypeIsBool(ctx, nextReturns->children[0]) ) {
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
        xl6AstFree(ownedIteratorType);
        return 0;
    }
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    if ( nextOwnerClass == NULL ) {
        nextOwnerClass = iteratorClass;
    }

    xl6CgenOwnTypeNode(ctx, ownedIteratorType);

    if ( outIterableClass != NULL ) {
        *outIterableClass = iterableClass;
    }
    if ( outIterMethod != NULL ) {
        *outIterMethod = iterMethod;
    }
    if ( outIterOwnerClass != NULL ) {
        *outIterOwnerClass = iterOwnerClass;
    }
    if ( outIteratorType != NULL ) {
        *outIteratorType = iteratorType;
    }
    if ( outIteratorClass != NULL ) {
        *outIteratorClass = iteratorClass;
    }
    if ( outNextMethod != NULL ) {
        *outNextMethod = nextMethod;
    }
    if ( outNextOwnerClass != NULL ) {
        *outNextOwnerClass = nextOwnerClass;
    }
    return 1;
}

static int xl6CgenIsClassProtocolIterableType(XL6_CgenContext* ctx, const XL6_AstNode* iterableType)
{
    return xl6CgenResolveClassIterableType(ctx, iterableType, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
}

static XL6_AstNode* xl6CgenCloneClassIterableItemType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* iteratorType,
    const XL6_AstNode* iteratorClass,
    const XL6_AstNode* nextMethod)
{
    const XL6_AstNode* returns;
    const XL6_AstNode* concreteIteratorType;
    const XL6_AstNode* typeArgs;
    const XL6_AstNode* prevParams;
    const XL6_AstNode* prevArgs;
    XL6_AstNode* result;

    returns = nextMethod != NULL && nextMethod->childCount > 2 ? nextMethod->children[2] : NULL;
    if ( returns == NULL || returns->childCount != 2 ) {
        return NULL;
    }
    concreteIteratorType = xl6CgenClassHandleTargetType(ctx, iteratorType);
    if ( concreteIteratorType == NULL ) {
        concreteIteratorType = iteratorType;
    }
    typeArgs = xl6CgenTypeNodeGenericArgs(concreteIteratorType);
    prevParams = ctx->currentGenericParams;
    prevArgs = ctx->currentGenericArgs;
    if ( typeArgs != NULL && xl6CgenClassHasTypeParams(iteratorClass) ) {
        ctx->currentGenericParams = xl6CgenClassTypeParams(iteratorClass);
        ctx->currentGenericArgs = typeArgs;
    }
    result = xl6CgenCloneCanonicalTypeNode(ctx, returns->children[1]);
    ctx->currentGenericParams = prevParams;
    ctx->currentGenericArgs = prevArgs;
    return result;
}

static void xl6CgenEmitClassIteratorNextCall(
    XL6_CgenContext* ctx,
    const char* iterName,
    const XL6_AstNode* iteratorType,
    const XL6_AstNode* iteratorClass,
    const XL6_AstNode* nextOwnerClass,
    const XL6_AstNode* nextMethod)
{
    const XL6_AstNode* concreteIteratorType;
    char basePath[256];
    char iteratorCType[512];

    basePath[0] = '\0';
    if ( !xl6CgenClassBasePath(ctx, iteratorClass, nextOwnerClass, basePath, sizeof(basePath)) ) {
        xl6CgenError(ctx, nextMethod, "cannot generate iterator protocol base path");
        basePath[0] = '\0';
    }
    concreteIteratorType = xl6CgenClassHandleTargetType(ctx, iteratorType);
    if ( concreteIteratorType == NULL ) {
        concreteIteratorType = iteratorType;
    }
    if ( concreteIteratorType != NULL && concreteIteratorType->childCount > 0 && xl6CgenClassHasTypeParams(nextOwnerClass) ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

        ctx->currentGenericParams = xl6CgenClassTypeParams(nextOwnerClass);
        ctx->currentGenericArgs = concreteIteratorType->children[0];
        xl6CgenGenericClassCallableSymbol(ctx, nextOwnerClass, concreteIteratorType, nextMethod);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
    } else {
        xl6CgenClassCallableSymbol(ctx, nextOwnerClass, nextMethod);
    }
    if ( xl6CgenClassHandleTargetType(ctx, iteratorType) != NULL ) {
        xl6CgenClassTypeRefName(iteratorCType, sizeof(iteratorCType), concreteIteratorType);
        if ( basePath[0] == '\0' ) {
            xl6OutputPrintf(ctx->out, "((%s*)xl6_class_data(%s, \"\", 0, 0))",
                            iteratorCType, iterName != NULL ? iterName : "_");
        } else {
            xl6OutputPrintf(ctx->out, "&((%s*)xl6_class_data(%s, \"\", 0, 0))->base",
                            iteratorCType, iterName != NULL ? iterName : "_");
            if ( strlen(basePath) > 5 ) {
                xl6OutputPuts(basePath + 5, ctx->out);
            }
        }
    } else {
        xl6OutputPuts("(&", ctx->out);
        xl6OutputPuts(iterName != NULL ? iterName : "_", ctx->out);
        xl6OutputPuts(basePath, ctx->out);
        xl6OutputPutc(')', ctx->out);
    }
}

static void xl6CgenForClassProtocolIterable(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* iterableType)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* iterable;
    const XL6_AstNode* iterableClass;
    const XL6_AstNode* iterOwnerClass;
    const XL6_AstNode* iterMethod;
    const XL6_AstNode* iteratorType;
    const XL6_AstNode* iteratorClass;
    const XL6_AstNode* nextOwnerClass;
    const XL6_AstNode* nextMethod;
    XL6_AstNode* receiverType;
    XL6_AstNode* itemType;
    char itemCType[256];
    size_t iterMark;
    size_t varMark;
    size_t loopMark;
    int id;
    int targetCount;
    char iterName[64];
    char nextName[64];

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "malformed class protocol for-in statement");
        return;
    }

    targets = node->children[0];
    iterable = node->children[1];
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "class protocol for in only supports one or two targets");
        return;
    }
    targetCount = (int)targets->childCount;

    if ( !xl6CgenResolveClassIterableType(ctx, iterableType,
            &iterableClass, &iterMethod, &iterOwnerClass,
            &iteratorType, &iteratorClass, &nextMethod, &nextOwnerClass) ) {
        xl6CgenError(ctx, iterable, "class protocol iterable requires __iter__() and iterator.__next__() as (bool, T)");
        return;
    }

    receiverType = NULL;
    if ( !xl6CgenResolveClassProtocolReceiver(ctx, iterable, "__iter__",
            &iterableClass, &iterOwnerClass, &iterMethod, &receiverType) ) {
        xl6CgenError(ctx, iterable, "cannot resolve class __iter__ protocol");
        return;
    }

    itemType = xl6CgenCloneClassIterableItemType(ctx, iteratorType, iteratorClass, nextMethod);
    if ( itemType == NULL ) {
        xl6AstFree(receiverType);
        xl6CgenError(ctx, iterable, "iterator __next__ return item type is invalid");
        return;
    }
    snprintf(itemCType, sizeof(itemCType), "%s", xl6CgenTypeName(ctx, itemType));

    id = ctx->tempId++;
    snprintf(iterName, sizeof(iterName), "__xl6_class_iter_%d", id);
    snprintf(nextName, sizeof(nextName), "__xl6_class_next_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    iterMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s %s = ", xl6CgenTypeName(ctx, iteratorType), iterName);
    if ( xl6CgenClassProtocolCallPrefix(ctx, iterable, iterableClass, iterOwnerClass, iterMethod, receiverType) ) {
        xl6OutputPutc(')', ctx->out);
    } else {
        xl6CgenError(ctx, iterable, "cannot generate class __iter__ protocol call");
        xl6OutputPuts("0", ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenAddManagedCleanupForType(ctx, iterName, iteratorType);

    if ( targetCount == 2 ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long __xl6_class_iter_index_%d = 0;\n", id);
    }

    xl6CgenIndent(ctx);
    xl6OutputPuts("while (1) {\n", ctx->out);
    ctx->indent += 1;

    xl6CgenIndent(ctx);
    {
        const XL6_AstNode* concreteIteratorType = xl6CgenClassHandleTargetType(ctx, iteratorType);
        if ( concreteIteratorType == NULL ) {
            concreteIteratorType = iteratorType;
        }
    if ( concreteIteratorType != NULL && concreteIteratorType->childCount > 0 && xl6CgenClassHasTypeParams(iteratorClass) ) {
        const XL6_AstNode* prevParams = ctx->currentGenericParams;
        const XL6_AstNode* prevArgs = ctx->currentGenericArgs;

        ctx->currentGenericParams = xl6CgenClassTypeParams(iteratorClass);
        ctx->currentGenericArgs = concreteIteratorType->children[0];
        xl6CgenFuncReturnStructName(ctx, nextMethod);
        ctx->currentGenericParams = prevParams;
        ctx->currentGenericArgs = prevArgs;
    } else {
        xl6CgenFuncReturnStructName(ctx, nextMethod);
    }
    }
    xl6OutputPrintf(ctx->out, " %s = ", nextName);
    xl6CgenEmitClassIteratorNextCall(ctx, iterName, iteratorType, iteratorClass, nextOwnerClass, nextMethod);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenEmitExceptionPropagate(ctx, node);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (!%s.value0) {\n", nextName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("break;\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targetCount == 2 ) {
        const XL6_AstNode* indexTarget = targets->children[0];
        char indexName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = __xl6_class_iter_index_%d;\n", indexName, id);
        xl6CgenRegisterLoopTargetType(ctx, indexTarget, "int");
    }
    {
        size_t valueTargetIndex;
        const XL6_AstNode* valueTarget;
        char valueName[512];

        valueTargetIndex = targetCount == 2 ? 1u : 0u;
        valueTarget = targets->children[valueTargetIndex];
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, valueTargetIndex);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemCType, valueName);
        if ( xl6CgenIsStringTypeNode(ctx, itemType) ) {
            xl6OutputPrintf(ctx->out, "xl6_xrt_string_dup(%s.value1, 0u)", nextName);
        } else {
            xl6OutputPrintf(ctx->out, "%s.value1", nextName);
        }
        xl6OutputPuts(";\n", ctx->out);
        if ( xl6CgenIsStringTypeNode(ctx, itemType) ) {
            xl6CgenIndent(ctx);
            xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", valueName);
        }
        xl6CgenRegisterLoopTargetTypeNode(ctx, valueTarget, itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    }

    xl6CgenForLoopBody(ctx, node, iterMark, loopMark, varMark, id, id);
    if ( targetCount == 2 ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_class_iter_index_%d += 1;\n", id);
    }

    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6CgenEmitCleanupsSince(ctx, iterMark);
    xl6CgenFreeCleanupsSince(ctx, iterMark);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);

    xl6AstFree(itemType);
    xl6AstFree(receiverType);
}
