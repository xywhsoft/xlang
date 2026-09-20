static XL6_CgenCoalesceSlot* xl6CgenFindCoalesceSlot(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return NULL;
    }
    for ( i = ctx->coalesceSlotCount; i > 0; --i ) {
        if ( ctx->coalesceSlots[i - 1].node == node ) {
            return &ctx->coalesceSlots[i - 1];
        }
    }
    return NULL;
}

static void xl6CgenCoalesceSlotName(
    char* buffer,
    size_t capacity,
    const XL6_CgenCoalesceSlot* slot)
{
    const char* prefix = "__xl6_coalesce_";

    if ( slot != NULL && slot->node != NULL ) {
        if ( slot->node->kind == XL6_AST_SAFE_MEMBER ) {
            prefix = "__xl6_safe_receiver_";
        } else if ( slot->node->kind == XL6_AST_SAFE_INDEX ) {
            prefix = "__xl6_safe_item_";
        } else if ( slot->node->kind == XL6_AST_MATCH ) {
            prefix = "__xl6_match_value_";
        }
    }
    snprintf(
        buffer,
        capacity,
        "%s%d",
        prefix,
        slot != NULL ? slot->id : 0);
}

static void xl6CgenSafeIndexContainerSlotName(
    char* buffer,
    size_t capacity,
    const XL6_CgenCoalesceSlot* slot)
{
    snprintf(
        buffer,
        capacity,
        "__xl6_safe_container_%d",
        slot != NULL ? slot->id : 0);
}

/*
 * C 没有可移植的局部绑定表达式。每条语句先声明安全表达式需要的临时槽，
 * 真正求值仍留在表达式原位，因此接收者只求值一次，后续分支保持惰性。
 */
static XL6_CgenCoalesceSlot* xl6CgenAddExpressionTempSlot(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    XL6_CgenCoalesceSlot* resized;
    XL6_CgenCoalesceSlot* slot;
    size_t capacity;

    if ( ctx == NULL || node == NULL ) {
        return NULL;
    }
    if ( ctx->coalesceSlotCount == ctx->coalesceSlotCapacity ) {
        capacity = ctx->coalesceSlotCapacity == 0 ?
                   8 :
                   ctx->coalesceSlotCapacity * 2;
        resized = (XL6_CgenCoalesceSlot*)xrtRealloc(
            ctx->coalesceSlots,
            capacity * sizeof(XL6_CgenCoalesceSlot));
        if ( resized == NULL ) {
            xl6CgenError(ctx, node, "out of memory while lowering safe expression");
            return NULL;
        }
        ctx->coalesceSlots = resized;
        ctx->coalesceSlotCapacity = capacity;
    }

    slot = &ctx->coalesceSlots[ctx->coalesceSlotCount++];
    slot->node = node;
    slot->id = ctx->tempId++;
    return slot;
}

static void xl6CgenPrepareNullCoalesceTemps(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    XL6_AstNode* leftType;
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }

    if ( node->kind == XL6_AST_MATCH &&
         (node->flags & XL6_AST_FLAG_MATCH_EXPR) != 0 &&
         node->childCount > 0 ) {
        XL6_AstNode* valueType =
            xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
        XL6_CgenCoalesceSlot* slot =
            valueType != NULL ? xl6CgenAddExpressionTempSlot(ctx, node) : NULL;
        char slotName[64];

        if ( slot != NULL ) {
            size_t ownedMark = ctx->ownedTypeNodeCount;

            xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
            xl6CgenIndent(ctx);
            xl6OutputPuts(xl6CgenValueStorageCType(ctx, valueType), ctx->out);
            xl6OutputPrintf(ctx->out, " %s = {0};\n", slotName);
            xl6CgenOwnTypeNode(ctx, valueType);
            if ( ctx->ownedTypeNodeCount > ownedMark ) {
                xl6CgenAddManagedCleanupForType(ctx, slotName, valueType);
                valueType = NULL;
            }
        }
        xl6AstFree(valueType);
    }

    if ( node->kind == XL6_AST_BINARY &&
         node->childCount >= 2 &&
         xl6CTextEquals(node->text, "??") &&
         node->children[0] != NULL &&
         node->children[0]->kind != XL6_AST_NULL ) {
        leftType = xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
        if ( leftType != NULL &&
             !xl6CTextEquals(leftType->text, "var") &&
             (xl6CgenIsNullableType(leftType) ||
              xl6CTextEquals(leftType->text, "point")) ) {
            XL6_CgenCoalesceSlot* slot =
                xl6CgenAddExpressionTempSlot(ctx, node);
            char slotName[64];

            if ( slot != NULL ) {
                xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
                xl6CgenIndent(ctx);
                xl6OutputPuts(xl6CgenTypeName(ctx, leftType), ctx->out);
                xl6OutputPrintf(ctx->out, " %s = {0};\n", slotName);
            }
        }
        xl6AstFree(leftType);
    }

    if ( node->kind == XL6_AST_SAFE_MEMBER && node->childCount > 0 ) {
        XL6_AstNode* resultType = xl6CgenCloneExprTypeForInference(ctx, node);
        XL6_AstNode* receiverType =
            xl6CgenCloneExprTypeForInference(ctx, node->children[0]);

        if ( resultType != NULL && receiverType != NULL &&
             !xl6CTextEquals(resultType->text, "var") ) {
            XL6_CgenCoalesceSlot* slot =
                xl6CgenAddExpressionTempSlot(ctx, node);
            char slotName[64];

            if ( slot != NULL ) {
                xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
                xl6CgenIndent(ctx);
                xl6OutputPuts(xl6CgenValueStorageCType(ctx, receiverType), ctx->out);
                xl6OutputPrintf(ctx->out, " %s = {0};\n", slotName);
            }
        }
        xl6AstFree(receiverType);
        xl6AstFree(resultType);
    }

    if ( node->kind == XL6_AST_SAFE_INDEX && node->childCount > 0 ) {
        XL6_AstNode* receiverType =
            xl6CgenCloneExprTypeForInference(ctx, node->children[0]);
        const XL6_AstNode* effectiveType = receiverType;

        if ( xl6CgenIsNullableType(receiverType) ) {
            effectiveType = xl6CgenNullableInnerType(receiverType);
        }
        if ( effectiveType != NULL &&
             xl6CgenIsTypedContainerType(ctx, effectiveType) ) {
            XL6_CgenCoalesceSlot* slot =
                xl6CgenAddExpressionTempSlot(ctx, node);
            char slotName[64];
            char containerName[64];

            if ( slot != NULL ) {
                xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
                xl6CgenSafeIndexContainerSlotName(
                    containerName,
                    sizeof(containerName),
                    slot);
                xl6CgenIndent(ctx);
                xl6OutputPuts(
                    xl6CgenValueStorageCType(ctx, effectiveType),
                    ctx->out);
                xl6OutputPrintf(ctx->out, " %s = {0};\n", containerName);
                xl6CgenIndent(ctx);
                xl6OutputPrintf(ctx->out, "const void* %s = 0;\n", slotName);
            }
        }
        xl6AstFree(receiverType);
    }

    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenPrepareNullCoalesceTemps(ctx, node->children[i]);
    }
}

static void xl6CgenNullCoalesceExpr(XL6_CgenContext* ctx, const XL6_AstNode* node)
{
    const XL6_AstNode* left;
    const XL6_AstNode* right;
    XL6_AstNode* leftType;
    XL6_AstNode* rightType;
    XL6_AstNode* resultType;
    XL6_CgenCoalesceSlot* slot;
    char slotName[64];
    int isXValue;

    left = node != NULL && node->childCount > 0 ? node->children[0] : NULL;
    right = node != NULL && node->childCount > 1 ? node->children[1] : NULL;

    if ( left != NULL && left->kind == XL6_AST_NULL ) {
        xl6CgenExpr(ctx, right);
        return;
    }

    leftType = xl6CgenCloneExprTypeForInference(ctx, left);
    rightType = xl6CgenCloneExprTypeForInference(ctx, right);
    isXValue = (leftType != NULL && xl6CTextEquals(leftType->text, "var")) ||
               (rightType != NULL && xl6CTextEquals(rightType->text, "var"));

    if ( isXValue ) {
        if ( left == NULL || leftType == NULL ||
             !xl6CTextEquals(leftType->text, "var") ) {
            xl6CgenError(ctx, node, "null coalesce with var result requires the left operand to be var or null");
            xl6OutputPuts("xrtValueNull()", ctx->out);
        } else {
            xl6CgenError(ctx, node, "dynamic var null coalesce requires statement-level lowering");
            xl6OutputPuts("xrtValueNull()", ctx->out);
        }
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    slot = xl6CgenFindCoalesceSlot(ctx, node);
    if ( slot == NULL ) {
        xl6CgenError(ctx, node, "null coalesce temporary was not prepared");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(leftType);
        xl6AstFree(rightType);
        return;
    }

    xl6CgenCoalesceSlotName(slotName, sizeof(slotName), slot);
    resultType = xl6CgenCloneExprTypeForInference(ctx, node);
    xl6OutputPuts("((", ctx->out);
    xl6OutputPuts(slotName, ctx->out);
    xl6OutputPuts(" = ", ctx->out);
    xl6CgenExpr(ctx, left);
    xl6OutputPuts("), ", ctx->out);
    if ( xl6CgenNullableUsesTaggedValue(leftType) ) {
        xl6OutputPrintf(ctx->out, "%s.hasValue ? ", slotName);
        if ( xl6CgenNullableUsesTaggedValue(resultType) ) {
            xl6OutputPuts(slotName, ctx->out);
            xl6OutputPuts(" : ", ctx->out);
            xl6CgenNullableTaggedValueExpr(ctx, resultType, right);
        } else {
            xl6OutputPrintf(ctx->out, "%s.value : ", slotName);
            xl6CgenExpr(ctx, right);
        }
    } else {
        xl6OutputPrintf(ctx->out, "%s != 0 ? %s : ", slotName, slotName);
        xl6CgenExpr(ctx, right);
    }
    xl6OutputPutc(')', ctx->out);

    xl6AstFree(leftType);
    xl6AstFree(rightType);
    xl6AstFree(resultType);
}
