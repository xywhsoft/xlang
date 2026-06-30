static void xl6CgenEmitExceptionPropagate(XL6_CgenContext* ctx, const XL6_AstNode* at);

static int xl6CgenIsRangeCall(const XL6_AstNode* node)
{
    if ( node == NULL || node->kind != XL6_AST_CALL || node->childCount < 2 ) {
        return 0;
    }
    return node->children[0] != NULL &&
           node->children[0]->kind == XL6_AST_NAME &&
           xl6CTextEquals(node->children[0]->text, "range");
}

static void xl6CgenRangePart(XL6_CgenContext* ctx, const XL6_AstNode* args, int index, const char* defaultText)
{
    if ( args != NULL && args->childCount > (size_t)index ) {
        const XL6_AstNode* arg = args->children[index];
        if ( arg != NULL && arg->childCount > 0 ) {
            xl6CgenExpr(ctx, arg->children[0]);
            return;
        }
    }
    xl6OutputPuts(defaultText, ctx->out);
}

static void xl6CgenRangeContainsExpr(XL6_CgenContext* ctx, const XL6_AstNode* value, const XL6_AstNode* rangeCall)
{
    const XL6_AstNode* args;

    if ( !xl6CgenIsRangeCall(rangeCall) || rangeCall->childCount < 2 ) {
        xl6CgenError(ctx, rangeCall, "in 右侧必须是 range(...)");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    args = rangeCall->children[1];
    if ( args == NULL || args->childCount == 0 || args->childCount > 3 ) {
        xl6CgenError(ctx, rangeCall, "range 参数数量必须是 1、2 或 3");
        xl6OutputPuts("0", ctx->out);
        return;
    }

    xl6OutputPuts("xl6_range_contains_i64(", ctx->out);
    xl6CgenExpr(ctx, value);
    xl6OutputPuts(", ", ctx->out);
    if ( args->childCount == 1 ) {
        xl6OutputPuts("0", ctx->out);
    } else {
        xl6CgenRangePart(ctx, args, 0, "0");
    }
    xl6OutputPuts(", ", ctx->out);
    if ( args->childCount == 1 ) {
        xl6CgenRangePart(ctx, args, 0, "0");
    } else {
        xl6CgenRangePart(ctx, args, 1, "0");
    }
    xl6OutputPuts(", ", ctx->out);
    if ( args->childCount >= 3 ) {
        xl6CgenRangePart(ctx, args, 2, "1");
    } else {
        xl6OutputPuts("1", ctx->out);
    }
    xl6OutputPutc(')', ctx->out);
}

static const XL6_AstNode* xl6CgenOwnSimpleType(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at)
{
    XL6_AstNode* typeNode;

    typeNode = xl6CgenNewSimpleType(name, at);
    if ( typeNode != NULL ) {
        xl6CgenOwnTypeNode(ctx, typeNode);
    }
    return typeNode;
}

static int xl6CgenIsForIterableType(const XL6_AstNode* typeNode)
{
    return typeNode != NULL && typeNode->text != NULL &&
           (xl6CTextEquals(typeNode->text, "var") ||
            xl6CgenIsArrayXValueType(typeNode) ||
            xl6CgenIsListXValueType(typeNode) ||
            xl6CgenIsSetXValueType(typeNode) ||
            xl6CgenIsDictXValueType(typeNode) ||
            xl6CgenIsGeneratorType(typeNode));
}

static const char* xl6CgenForTargetName(const XL6_AstNode* targets, size_t index)
{
    if ( targets == NULL || index >= targets->childCount || targets->children[index] == NULL ||
         targets->children[index]->text == NULL ) {
        return "_";
    }
    return targets->children[index]->text;
}

static void xl6CgenForTargetCName(char* buffer, size_t capacity, const XL6_AstNode* targets, size_t index)
{
    xl6CgenCIdentText(buffer, capacity, xl6CgenForTargetName(targets, index));
}

static void xl6CgenRegisterLoopTargetTypeNode(XL6_CgenContext* ctx, const XL6_AstNode* target, const XL6_AstNode* typeNode)
{
    XL6_AstNode* ownedType;

    if ( target == NULL || target->text == NULL || typeNode == NULL ) {
        return;
    }
    ownedType = xl6CgenCloneTypeNode(typeNode);
    if ( ownedType == NULL ) {
        return;
    }
    xl6CgenOwnTypeNode(ctx, ownedType);
    xl6CgenAddVarType(ctx, target->text, ownedType);
    xl6CgenAddManagedCleanupForType(ctx, target->text, ownedType);
}

static void xl6CgenRegisterLoopTargetType(XL6_CgenContext* ctx, const XL6_AstNode* target, const char* typeName)
{
    const XL6_AstNode* typeNode;

    if ( target == NULL || target->text == NULL || typeName == NULL ) {
        return;
    }
    typeNode = xl6CgenOwnSimpleType(ctx, typeName, target);
    xl6CgenAddVarType(ctx, target->text, typeNode);
    xl6CgenAddManagedCleanupForType(ctx, target->text, typeNode);
}

static void xl6CgenTypedLoopValueBegin(XL6_CgenContext* ctx, const XL6_AstNode* itemType)
{
    if ( xl6CgenTypeContainsFunctionType(itemType) ) {
        xl6OutputPuts("xl6_function_from_callable(*(xrt_callable**)", ctx->out);
        return;
    }
    if ( xl6CgenIsLanguageBoolType(ctx, itemType) ) {
        xl6OutputPuts("((int)(*(bool*)", ctx->out);
        return;
    }
    xl6OutputPuts("(*(", ctx->out);
    xl6OutputPuts(itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", ctx->out);
    xl6OutputPuts("*)", ctx->out);
}

static void xl6CgenTypedLoopValueEnd(XL6_CgenContext* ctx, const XL6_AstNode* itemType)
{
    if ( xl6CgenIsLanguageBoolType(ctx, itemType) ) {
        xl6OutputPuts("))", ctx->out);
        return;
    }
    xl6OutputPutc(')', ctx->out);
}

static void xl6CgenEmitTypedLoopItemGuard(XL6_CgenContext* ctx, int continueId)
{
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_typed_item_%d == 0) { xl6_runtime_fatal(3); }\n", continueId);
}

static const char* xl6CgenForTargetTypeName(const XL6_AstNode* iterableType, size_t targetIndex, size_t targetCount)
{
    if ( iterableType == NULL || iterableType->text == NULL ) {
        return NULL;
    }
    if ( xl6CTextEquals(iterableType->text, "range") ) {
        return "int";
    }
    if ( xl6CTextEquals(iterableType->text, "var") ) {
        return "var";
    }
    if ( xl6CgenIsDictXValueType(iterableType) ) {
        if ( targetCount == 1 || targetIndex == 0 ) {
            return "string";
        }
        return "var";
    }
    if ( xl6CgenIsArrayXValueType(iterableType) || xl6CgenIsListXValueType(iterableType) ||
         xl6CgenIsSetXValueType(iterableType) ) {
        if ( targetCount == 2 && targetIndex == 0 ) {
            return "int";
        }
        return "var";
    }
    return NULL;
}

static void xl6CgenAddIterSnapshotCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at)
{
    const XL6_AstNode* typeNode;

    typeNode = xl6CgenOwnSimpleType(ctx, "var", at);
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_ITER_SNAPSHOT, typeNode, NULL);
}

static void xl6CgenAddXrtPtrCleanup(XL6_CgenContext* ctx, const char* name, const XL6_AstNode* at)
{
    const XL6_AstNode* typeNode;

    typeNode = xl6CgenOwnSimpleType(ctx, "var", at);
    xl6CgenAddCleanupEntry(ctx, name, XL6_CGEN_CLEANUP_XRT_PTR, typeNode, NULL);
}

static void xl6CgenEmitTypedDictKeyCopy(XL6_CgenContext* ctx, const XL6_AstNode* at, int continueId)
{
    char keyCopyName[64];

    snprintf(keyCopyName, sizeof(keyCopyName), "__xl6_typed_key_copy_%d", continueId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "char* %s = 0;\n", keyCopyName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (__xl6_typed_key_%d != 0) {\n", continueId);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s = (char*)xrtMalloc((size_t)__xl6_typed_key_len_%d + 1u);\n", keyCopyName, continueId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) { xl6_runtime_fatal(3); }\n", keyCopyName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "memcpy(%s, __xl6_typed_key_%d, (size_t)__xl6_typed_key_len_%d);\n",
            keyCopyName, continueId, continueId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "%s[__xl6_typed_key_len_%d] = '\\0';\n", keyCopyName, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenAddXrtPtrCleanup(ctx, keyCopyName, at);
}

static void xl6CgenFinishLoopIteration(XL6_CgenContext* ctx, size_t cleanupMark, size_t varMark, int continueId)
{
    xl6CgenEmitCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeCleanupsSince(ctx, cleanupMark);
    xl6CgenFreeVarsSince(ctx, varMark);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_continue_%d:\n", continueId);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenForLoopBody(XL6_CgenContext* ctx, const XL6_AstNode* node, size_t breakCleanupMark, size_t continueCleanupMark, size_t varMark, int breakId, int continueId)
{
    xl6CgenPushLoopTarget(ctx, breakCleanupMark, continueCleanupMark, breakId, continueId);
    if ( node->childCount > 2 ) {
        xl6CgenBlock(ctx, node->children[2]);
    }
    xl6CgenPopLoopTarget(ctx);
    xl6CgenFinishLoopIteration(ctx, continueCleanupMark, varMark, continueId);
}

static void xl6CgenForArrayLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t varMark;
    size_t loopMark;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_count_%d = xvoArrayItemCount(%s);\n", continueId, iterName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (unsigned int __xl6_index_%d = 0; __xl6_index_%d < __xl6_count_%d; ++__xl6_index_%d) {\n",
            continueId, continueId, continueId, continueId);
    ctx->indent += 1;

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* valueTarget = targets->children[0];
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_retain(xvoArrayGetValue(%s, __xl6_index_%d));\n",
                valueName, iterName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    } else {
        const XL6_AstNode* indexTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateInt((long long)__xl6_index_%d);\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "var");
        } else {
            xl6OutputPrintf(ctx->out, "long long %s = (long long)__xl6_index_%d;\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "int");
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_retain(xvoArrayGetValue(%s, __xl6_index_%d));\n",
                valueName, iterName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenForListLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t iterCleanupMark;
    size_t varMark;
    size_t loopMark;
    char snapshotName[64];

    snprintf(snapshotName, sizeof(snapshotName), "__xl6_list_snapshot_%d", continueId);
    iterCleanupMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_XrtIterSnapshot* %s = xl6_xrt_list_snapshot(%s);\n", snapshotName, iterName);
    xl6CgenAddIterSnapshotCleanup(ctx, snapshotName, node);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", snapshotName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
            "for (unsigned int __xl6_list_pos_%d = 0; __xl6_list_pos_%d < %s->count; ++__xl6_list_pos_%d) {\n",
            continueId, continueId, snapshotName, continueId);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_XrtIterItem* __xl6_list_item_%d = &%s->items[__xl6_list_pos_%d];\n",
            continueId, snapshotName, continueId);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* valueTarget = targets->children[0];
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_retain(__xl6_list_item_%d->value);\n",
                valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    } else {
        const XL6_AstNode* indexTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateInt(__xl6_list_item_%d->index);\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "var");
        } else {
            xl6OutputPrintf(ctx->out, "long long %s = __xl6_list_item_%d->index;\n", indexName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "int");
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_retain(__xl6_list_item_%d->value);\n",
                valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenCloseScope(ctx, iterCleanupMark);
}

static void xl6CgenForSetLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t iterCleanupMark;
    size_t varMark;
    size_t loopMark;
    char countName[64];
    char indexVarName[64];

    snprintf(countName, sizeof(countName), "__xl6_set_count_%d", continueId);
    snprintf(indexVarName, sizeof(indexVarName), "__xl6_set_index_%d", continueId);
    iterCleanupMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int %s = xvoSetItemCount(%s);\n", countName, iterName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (unsigned int %s = 0; %s < %s; ++%s) {\n",
            indexVarName, indexVarName, countName, indexVarName);
    ctx->indent += 1;

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* valueTarget = targets->children[0];
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_retain(xvoSetGetValueAt(%s, %s));\n",
                valueName, iterName, indexVarName);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    } else {
        const XL6_AstNode* indexTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateInt((long long)%s);\n", indexName, indexVarName);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "var");
        } else {
            xl6OutputPrintf(ctx->out, "long long %s = (long long)%s;\n", indexName, indexVarName);
            xl6CgenRegisterLoopTargetType(ctx, indexTarget, "int");
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_retain(xvoSetGetValueAt(%s, %s));\n",
                valueName, iterName, indexVarName);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenCloseScope(ctx, iterCleanupMark);
}

static void xl6CgenForDictLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, size_t breakCleanupMark, int breakId, int continueId, int forceVarTargets)
{
    size_t iterCleanupMark;
    size_t varMark;
    size_t loopMark;
    char snapshotName[64];

    snprintf(snapshotName, sizeof(snapshotName), "__xl6_dict_snapshot_%d", continueId);
    iterCleanupMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_XrtIterSnapshot* %s = xl6_xrt_dict_snapshot(%s);\n", snapshotName, iterName);
    xl6CgenAddIterSnapshotCleanup(ctx, snapshotName, node);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "if (%s == 0) {\n", snapshotName);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("xl6_runtime_fatal(3);\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
            "for (unsigned int __xl6_dict_pos_%d = 0; __xl6_dict_pos_%d < %s->count; ++__xl6_dict_pos_%d) {\n",
            continueId, continueId, snapshotName, continueId);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_XrtIterItem* __xl6_dict_item_%d = &%s->items[__xl6_dict_pos_%d];\n",
            continueId, snapshotName, continueId);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        const XL6_AstNode* keyTarget = targets->children[0];
        char keyName[512];

        xl6CgenForTargetCName(keyName, sizeof(keyName), targets, 0);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateText((void*)__xl6_dict_item_%d->key, __xl6_dict_item_%d->keyLen, false);\n",
                    keyName, continueId, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "var");
        } else {
            xl6OutputPrintf(ctx->out, "const char* %s = __xl6_dict_item_%d->key;\n", keyName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "string");
        }
    } else {
        const XL6_AstNode* keyTarget = targets->children[0];
        const XL6_AstNode* valueTarget = targets->children[1];
        char keyName[512];
        char valueName[512];

        xl6CgenForTargetCName(keyName, sizeof(keyName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        if ( forceVarTargets ) {
            xl6OutputPrintf(ctx->out, "xvalue %s = xvoCreateText((void*)__xl6_dict_item_%d->key, __xl6_dict_item_%d->keyLen, false);\n",
                    keyName, continueId, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "var");
        } else {
            xl6OutputPrintf(ctx->out, "const char* %s = __xl6_dict_item_%d->key;\n", keyName, continueId);
            xl6CgenRegisterLoopTargetType(ctx, keyTarget, "string");
        }
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xvalue %s = xl6_xrt_retain(__xl6_dict_item_%d->value);\n",
                valueName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, valueTarget, "var");
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenCloseScope(ctx, iterCleanupMark);
}

static void xl6CgenForRange(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* rangeCall;
    const XL6_AstNode* args;
    size_t varMark;
    size_t loopMark;
    int id;

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "malformed range for-in statement");
        return;
    }

    targets = node->children[0];
    rangeCall = node->children[1];
    if ( !xl6CgenIsRangeCall(rangeCall) ) {
        xl6CgenError(ctx, rangeCall, "当前 C 生成只支持 for ... in range(...)");
        return;
    }
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "for range 只支持一个或两个迭代变量");
        return;
    }

    args = rangeCall->children[1];
    if ( args->childCount == 0 || args->childCount > 3 ) {
        xl6CgenError(ctx, rangeCall, "range 参数数量必须是 1、2 或 3");
        return;
    }

    id = ctx->tempId++;
    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long __xl6_range_start_%d = ", id);
    if ( args->childCount == 1 ) {
        xl6OutputPuts("0", ctx->out);
    } else {
        xl6CgenRangePart(ctx, args, 0, "0");
    }
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long __xl6_range_stop_%d = ", id);
    if ( args->childCount == 1 ) {
        xl6CgenRangePart(ctx, args, 0, "0");
    } else {
        xl6CgenRangePart(ctx, args, 1, "0");
    }
    xl6OutputPuts(";\n", ctx->out);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long __xl6_range_step_%d = ", id);
    if ( args->childCount >= 3 ) {
        xl6CgenRangePart(ctx, args, 2, "1");
    } else {
        xl6OutputPuts("1", ctx->out);
    }
    xl6OutputPuts(";\n", ctx->out);

    if ( args->childCount >= 3 ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if ( __xl6_range_step_%d == 0 ) {\n", id);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
                ctx->out,
                "xl6_error_raise(\"RangeError\", \"range step cannot be 0\", \"\", %d, %d);\n",
                rangeCall != NULL ? rangeCall->line : 0,
                rangeCall != NULL ? rangeCall->column : 0);
        xl6CgenEmitExceptionPropagate(ctx, rangeCall);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long __xl6_range_index_%d = 0;\n", id);

    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out,
            "for (long long __xl6_range_value_%d = __xl6_range_start_%d; "
            "(__xl6_range_step_%d > 0) ? (__xl6_range_value_%d < __xl6_range_stop_%d) : (__xl6_range_value_%d > __xl6_range_stop_%d); "
            "__xl6_range_value_%d += __xl6_range_step_%d, ++__xl6_range_index_%d) {\n",
            id, id, id, id, id, id, id, id, id, id);

    ctx->indent += 1;
    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = __xl6_range_value_%d;\n", valueName, id);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
    } else {
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = __xl6_range_index_%d;\n", indexName, id);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = __xl6_range_value_%d;\n", valueName, id);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[1], "int");
    }

    xl6CgenForLoopBody(ctx, node, loopMark, loopMark, varMark, id, id);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenForXValueIterable(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* iterableType)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* iterable;
    const XL6_AstNode* tempType;
    size_t iterMark;
    int id;
    char iterName[64];

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "malformed dynamic for-in statement");
        return;
    }

    targets = node->children[0];
    iterable = node->children[1];
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "for in 只支持一个或两个迭代变量");
        return;
    }

    id = ctx->tempId++;
    snprintf(iterName, sizeof(iterName), "__xl6_iter_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    iterMark = ctx->cleanupCount;
    tempType = xl6CgenOwnSimpleType(ctx, "var", iterable);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "xvalue %s = ", iterName);
    xl6CgenXValueFromExpr(ctx, iterable);
    xl6OutputPuts(";\n", ctx->out);
    xl6CgenAddXValueCleanup(ctx, iterName, tempType);

    if ( xl6CgenIsArrayXValueType(iterableType) ) {
        xl6CgenForArrayLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else if ( xl6CgenIsListXValueType(iterableType) ) {
        xl6CgenForListLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else if ( xl6CgenIsSetXValueType(iterableType) ) {
        xl6CgenForSetLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else if ( xl6CgenIsDictXValueType(iterableType) ) {
        xl6CgenForDictLoop(ctx, node, targets, iterName, iterMark, id, id, 0);
    } else {
        int arrayId = ctx->tempId++;
        int listId = ctx->tempId++;
        int setId = ctx->tempId++;
        int dictId = ctx->tempId++;

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "int __xl6_iter_type_%d = xvoType(%s);\n", id, iterName);

        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "if (__xl6_iter_type_%d == XL6_XVO_DT_ARRAY) {\n", id);
        ctx->indent += 1;
        xl6CgenForArrayLoop(ctx, node, targets, iterName, iterMark, id, arrayId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "} else if (__xl6_iter_type_%d == XL6_XVO_DT_LIST) {\n", id);
        ctx->indent += 1;
        xl6CgenForListLoop(ctx, node, targets, iterName, iterMark, id, listId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "} else if (__xl6_iter_type_%d == XL6_XVO_DT_COLL) {\n", id);
        ctx->indent += 1;
        xl6CgenForSetLoop(ctx, node, targets, iterName, iterMark, id, setId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "} else if (__xl6_iter_type_%d == XL6_XVO_DT_TABLE) {\n", id);
        ctx->indent += 1;
        xl6CgenForDictLoop(ctx, node, targets, iterName, iterMark, id, dictId, 1);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPuts("else {\n", ctx->out);
        ctx->indent += 1;
        xl6CgenIndent(ctx);
        xl6OutputPrintf(
                ctx->out,
                "xl6_error_raise(\"TypeError\", \"for in value must be array, list, set or dict\", \"\", %d, %d);\n",
                iterable != NULL ? iterable->line : 0,
                iterable != NULL ? iterable->column : 0);
        xl6CgenEmitExceptionPropagate(ctx, iterable);
        ctx->indent -= 1;
        xl6CgenIndent(ctx);
        xl6OutputPuts("}\n", ctx->out);
    }

    xl6CgenCloseScope(ctx, iterMark);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
}

static void xl6CgenForGeneratorIterable(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* iterableType)
{
    const XL6_AstNode* targets;
    const XL6_AstNode* iterable;
    XL6_AstNode* itemType;
    const char* itemCType;
    size_t iterMark;
    size_t varMark;
    size_t loopMark;
    int id;
    int targetCount;
    char iterName[64];

    if ( node->childCount < 3 ) {
        xl6CgenError(ctx, node, "malformed generator for-in statement");
        return;
    }

    targets = node->children[0];
    iterable = node->children[1];
    if ( targets == NULL || targets->childCount == 0 || targets->childCount > 2 ) {
        xl6CgenError(ctx, targets, "generator for in 只支持一个或两个目标变量");
        return;
    }
    targetCount = (int)targets->childCount;

    itemType = xl6CgenCloneGeneratorItemType(ctx, iterableType);
    if ( itemType == NULL ) {
        xl6CgenError(ctx, iterableType, "generator 类型必须写成 generator<T>");
        return;
    }
    itemCType = xl6CgenTypeName(ctx, itemType);

    id = ctx->tempId++;
    snprintf(iterName, sizeof(iterName), "__xl6_gen_iter_%d", id);

    xl6CgenIndent(ctx);
    xl6OutputPuts("{\n", ctx->out);
    ctx->indent += 1;

    iterMark = ctx->cleanupCount;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "XL6_Generator* %s = ", iterName);
    xl6CgenExpr(ctx, iterable);
    xl6OutputPuts(";\n", ctx->out);
    if ( iterable != NULL && iterable->kind == XL6_AST_NAME ) {
        char sourceName[512];

        xl6CgenCIdentText(sourceName, sizeof(sourceName), iterable->text);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s = 0;\n", sourceName);
    }
    xl6CgenAddGeneratorCleanup(ctx, iterName, iterableType);

    if ( targetCount == 2 ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long __xl6_gen_index_%d = 0;\n", id);
    }
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "while (xl6_generator_next(%s, \"\", %d, %d)) {\n", iterName, node->line, node->column);
    ctx->indent += 1;

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targetCount == 2 ) {
        const XL6_AstNode* indexTarget = targets->children[0];
        char indexName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = __xl6_gen_index_%d;\n", indexName, id);
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
        xl6CgenDefaultValue(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "xl6_generator_value_into(%s, &%s, sizeof(%s), \"\", %d, %d);\n",
                iterName, valueName, itemCType, node->line, node->column);
        xl6CgenRegisterLoopTargetTypeNode(ctx, valueTarget, itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    }

    xl6CgenEmitExceptionPropagate(ctx, node);
    xl6CgenForLoopBody(ctx, node, iterMark, loopMark, varMark, id, id);
    if ( targetCount == 2 ) {
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "__xl6_gen_index_%d += 1;\n", id);
    }
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);

    xl6CgenEmitExceptionPropagate(ctx, node);

    xl6CgenCloseScope(ctx, iterMark);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "__xl6_break_%d:\n", id);
    xl6CgenIndent(ctx);
    xl6OutputPuts(";\n", ctx->out);
    xl6AstFree(itemType);
}

static void xl6CgenForTypedArrayLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, const XL6_AstNode* iterableType, size_t breakCleanupMark, int breakId, int continueId)
{
    const XL6_AstNode* itemType;
    size_t varMark;
    size_t loopMark;

    itemType = xl6CgenTypedContainerItemType(ctx, iterableType);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_typed_count_%d = xrtTypedArrayCount(%s);\n", continueId, iterName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (unsigned int __xl6_typed_index_%d = 0; __xl6_typed_index_%d < __xl6_typed_count_%d; ++__xl6_typed_index_%d) {\n",
            continueId, continueId, continueId, continueId);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "void* __xl6_typed_item_%d = xl6_xrt_typed_array_get_at(%s, __xl6_typed_index_%d);\n",
            continueId, iterName, continueId);
    xl6CgenEmitTypedLoopItemGuard(ctx, continueId);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", valueName);
        xl6CgenTypedLoopValueBegin(ctx, itemType);
        xl6OutputPrintf(ctx->out, "__xl6_typed_item_%d", continueId);
        xl6CgenTypedLoopValueEnd(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRegisterLoopTargetTypeNode(ctx, targets->children[0], itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    } else {
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)__xl6_typed_index_%d;\n", indexName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", valueName);
        xl6CgenTypedLoopValueBegin(ctx, itemType);
        xl6OutputPrintf(ctx->out, "__xl6_typed_item_%d", continueId);
        xl6CgenTypedLoopValueEnd(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRegisterLoopTargetTypeNode(ctx, targets->children[1], itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenForTypedListLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, const XL6_AstNode* iterableType, size_t breakCleanupMark, int breakId, int continueId)
{
    const XL6_AstNode* itemType;
    size_t varMark;
    size_t loopMark;

    itemType = xl6CgenTypedContainerItemType(ctx, iterableType);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_typed_count_%d = xrtTypedListCount(%s);\n", continueId, iterName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (unsigned int __xl6_typed_pos_%d = 0; __xl6_typed_pos_%d < __xl6_typed_count_%d; ++__xl6_typed_pos_%d) {\n",
            continueId, continueId, continueId, continueId);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "long long __xl6_typed_key_%d = 0;\n", continueId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "void* __xl6_typed_item_%d = xrtTypedListItemAt(%s, __xl6_typed_pos_%d, &__xl6_typed_key_%d);\n",
            continueId, iterName, continueId, continueId);
    xl6CgenEmitTypedLoopItemGuard(ctx, continueId);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", valueName);
        xl6CgenTypedLoopValueBegin(ctx, itemType);
        xl6OutputPrintf(ctx->out, "__xl6_typed_item_%d", continueId);
        xl6CgenTypedLoopValueEnd(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRegisterLoopTargetTypeNode(ctx, targets->children[0], itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    } else {
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = __xl6_typed_key_%d;\n", indexName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", valueName);
        xl6CgenTypedLoopValueBegin(ctx, itemType);
        xl6OutputPrintf(ctx->out, "__xl6_typed_item_%d", continueId);
        xl6CgenTypedLoopValueEnd(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRegisterLoopTargetTypeNode(ctx, targets->children[1], itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenForTypedSetLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, const XL6_AstNode* iterableType, size_t breakCleanupMark, int breakId, int continueId)
{
    const XL6_AstNode* itemType;
    size_t varMark;
    size_t loopMark;

    itemType = xl6CgenTypedContainerItemType(ctx, iterableType);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_typed_count_%d = xrtTypedSetCount(%s);\n", continueId, iterName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (unsigned int __xl6_typed_index_%d = 0; __xl6_typed_index_%d < __xl6_typed_count_%d; ++__xl6_typed_index_%d) {\n",
            continueId, continueId, continueId, continueId);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "void* __xl6_typed_item_%d = xrtTypedSetItemAt(%s, __xl6_typed_index_%d);\n",
            continueId, iterName, continueId);
    xl6CgenEmitTypedLoopItemGuard(ctx, continueId);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    if ( targets->childCount == 1 ) {
        char valueName[512];

        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", valueName);
        xl6CgenTypedLoopValueBegin(ctx, itemType);
        xl6OutputPrintf(ctx->out, "__xl6_typed_item_%d", continueId);
        xl6CgenTypedLoopValueEnd(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRegisterLoopTargetTypeNode(ctx, targets->children[0], itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    } else {
        char indexName[512];
        char valueName[512];

        xl6CgenForTargetCName(indexName, sizeof(indexName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "long long %s = (long long)__xl6_typed_index_%d;\n", indexName, continueId);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "int");
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", valueName);
        xl6CgenTypedLoopValueBegin(ctx, itemType);
        xl6OutputPrintf(ctx->out, "__xl6_typed_item_%d", continueId);
        xl6CgenTypedLoopValueEnd(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRegisterLoopTargetTypeNode(ctx, targets->children[1], itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}

static void xl6CgenForTypedDictLoop(XL6_CgenContext* ctx, const XL6_AstNode* node, const XL6_AstNode* targets, const char* iterName, const XL6_AstNode* iterableType, size_t breakCleanupMark, int breakId, int continueId)
{
    const XL6_AstNode* itemType;
    size_t varMark;
    size_t loopMark;

    itemType = xl6CgenTypedContainerItemType(ctx, iterableType);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_typed_count_%d = xrtTypedDictCount(%s);\n", continueId, iterName);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "for (unsigned int __xl6_typed_pos_%d = 0; __xl6_typed_pos_%d < __xl6_typed_count_%d; ++__xl6_typed_pos_%d) {\n",
            continueId, continueId, continueId, continueId);
    ctx->indent += 1;
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "const char* __xl6_typed_key_%d = 0;\n", continueId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "unsigned int __xl6_typed_key_len_%d = 0;\n", continueId);
    xl6CgenIndent(ctx);
    xl6OutputPrintf(ctx->out, "void* __xl6_typed_item_%d = xrtTypedDictItemAt(%s, __xl6_typed_pos_%d, &__xl6_typed_key_%d, &__xl6_typed_key_len_%d);\n",
            continueId, iterName, continueId, continueId, continueId);
    xl6CgenEmitTypedLoopItemGuard(ctx, continueId);

    varMark = ctx->varCount;
    loopMark = ctx->cleanupCount;
    xl6CgenEmitTypedDictKeyCopy(ctx, targets, continueId);
    if ( targets->childCount == 1 ) {
        char keyName[512];

        xl6CgenForTargetCName(keyName, sizeof(keyName), targets, 0);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "const char* %s = __xl6_typed_key_copy_%d != 0 ? __xl6_typed_key_copy_%d : \"\";\n",
                keyName, continueId, continueId);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "string");
    } else {
        char keyName[512];
        char valueName[512];

        xl6CgenForTargetCName(keyName, sizeof(keyName), targets, 0);
        xl6CgenForTargetCName(valueName, sizeof(valueName), targets, 1);
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "const char* %s = __xl6_typed_key_copy_%d != 0 ? __xl6_typed_key_copy_%d : \"\";\n",
                keyName, continueId, continueId);
        xl6CgenRegisterLoopTargetType(ctx, targets->children[0], "string");
        xl6CgenIndent(ctx);
        xl6OutputPrintf(ctx->out, "%s %s = ", itemType != NULL ? xl6CgenTypeName(ctx, itemType) : "xvalue", valueName);
        xl6CgenTypedLoopValueBegin(ctx, itemType);
        xl6OutputPrintf(ctx->out, "__xl6_typed_item_%d", continueId);
        xl6CgenTypedLoopValueEnd(ctx, itemType);
        xl6OutputPuts(";\n", ctx->out);
        xl6CgenRegisterLoopTargetTypeNode(ctx, targets->children[1], itemType);
        xl6CgenRetainClassFunctionFieldsForType(ctx, itemType, valueName);
    }

    xl6CgenForLoopBody(ctx, node, breakCleanupMark, loopMark, varMark, breakId, continueId);
    ctx->indent -= 1;
    xl6CgenIndent(ctx);
    xl6OutputPuts("}\n", ctx->out);
}
