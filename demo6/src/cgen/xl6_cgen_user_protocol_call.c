static int xl6CgenUserProtocolCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* call,
    const XL6_AstNode* member)
{
    const XL6_AstNode* receiver;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* protocolNode;
    const XL6_AstNode* requirement;
    const XL6_AstNode* args;
    XL6_AstNode* receiverType;
    XL6_AstNode* concreteRequirement;
    const char* file;
    int fixedCount;

    if ( ctx == NULL || call == NULL || member == NULL ||
         call->semanticCallKind != XL6_SEM_CALL_WITNESS ) {
        return 0;
    }
    receiver = member->childCount > 0 ? member->children[0] : NULL;
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, receiverType));
    protocolNode = concreteType != NULL
        ? xl6CgenFindProtocol(ctx, concreteType->text)
        : NULL;
    requirement = xl6CgenProtocolRequirementBySymbol(
        protocolNode,
        call->semanticTargetSymbolId);
    if ( protocolNode == NULL || requirement == NULL ) {
        xl6CgenError(ctx, member, "protocol witness 调用缺少语义目标");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(receiverType);
        return 1;
    }
    if ( xl6CgenProtocolIsGeneric(protocolNode) ) {
        concreteRequirement = xl6CgenConcreteProtocolRequirement(
            ctx,
            protocolNode,
            concreteType,
            requirement);
        if ( concreteRequirement == NULL ) {
            xl6CgenError(
                ctx,
                member,
                "cannot instantiate generic protocol witness call");
            xl6OutputPuts("0", ctx->out);
            xl6AstFree(receiverType);
            return 1;
        }
        xl6CgenGenericProtocolCallSymbol(
            ctx,
            concreteType,
            concreteRequirement);
        xl6OutputPutc('(', ctx->out);
        xl6CgenExpr(ctx, receiver);
        args = call->childCount > 1 ? call->children[1] : NULL;
        fixedCount = xl6CgenFixedParamCount(
            xl6CgenCallableParams(concreteRequirement));
        if ( fixedCount > 0 ) {
            xl6OutputPuts(", ", ctx->out);
            xl6CgenKnownCallArgs(ctx, concreteRequirement, args);
        }
        xl6OutputPuts(", ", ctx->out);
        xl6OutputPuts(
            xl6CgenExprIsXValueBorrow(ctx, receiver) ? "false" : "true",
            ctx->out);
        xl6OutputPuts(", ", ctx->out);
        file = ctx->debugSourcePath != NULL ? ctx->debugSourcePath : "";
        xl6CgenCStringLiteral(ctx->out, file);
        xl6OutputPrintf(
            ctx->out,
            ", %d, %d)",
            member->line,
            member->column);
        xl6AstFree(concreteRequirement);
        xl6AstFree(receiverType);
        return 1;
    }
    xl6CgenProtocolCallSymbol(ctx, protocolNode, requirement);
    xl6OutputPutc('(', ctx->out);
    xl6CgenExpr(ctx, receiver);
    args = call->childCount > 1 ? call->children[1] : NULL;
    fixedCount = xl6CgenFixedParamCount(xl6CgenCallableParams(requirement));
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(ctx, requirement, args);
    }
    xl6OutputPuts(", ", ctx->out);
    xl6OutputPuts(
        xl6CgenExprIsXValueBorrow(ctx, receiver) ? "false" : "true",
        ctx->out);
    xl6OutputPuts(", ", ctx->out);
    file = ctx->debugSourcePath != NULL ? ctx->debugSourcePath : "";
    xl6CgenCStringLiteral(ctx->out, file);
    xl6OutputPrintf(
        ctx->out,
        ", %d, %d)",
        member->line,
        member->column);
    xl6AstFree(receiverType);
    return 1;
}

/*
    属性 requirement 已由 typed HIR 唯一选定。
    getter、setter 和维度参数统一走 witness 调用，不在后端重复做重载判断。
*/
static int xl6CgenUserProtocolPropertyCall(
    XL6_CgenContext* ctx,
    const XL6_AstNode* member,
    const XL6_AstNode* indexNode,
    const XL6_AstNode* valueExpr,
    XL6_AstKind kind,
    const XL6_AstNode* semanticSource)
{
    const XL6_AstNode* hirTarget;
    const XL6_AstNode* receiver;
    const XL6_AstNode* concreteType;
    const XL6_AstNode* protocolNode;
    const XL6_AstNode* requirement;
    const XL6_AstNode* callRequirement;
    XL6_AstNode* receiverType;
    XL6_AstNode* call;
    XL6_AstNode* concreteRequirement;
    const char* file;
    int fixedCount;

    if ( ctx == NULL || member == NULL || member->kind != XL6_AST_MEMBER ||
         member->childCount == 0 ||
         (kind != XL6_AST_GET && kind != XL6_AST_LET) ) {
        return 0;
    }
    if ( semanticSource == NULL ) {
        semanticSource = indexNode != NULL ? indexNode : member;
    }
    hirTarget = xl6CgenHirTarget(ctx, semanticSource);
    if ( hirTarget == NULL ||
         semanticSource->semanticCallKind != XL6_SEM_CALL_WITNESS ||
         hirTarget->kind != kind ) {
        return 0;
    }

    receiver = member->children[0];
    receiverType = xl6CgenCloneExprTypeForInference(ctx, receiver);
    concreteType = xl6CgenResolveAliasType(
        ctx,
        xl6CgenSubstituteGenericType(ctx, receiverType));
    protocolNode = concreteType != NULL ?
        xl6CgenFindProtocol(ctx, concreteType->text) : NULL;
    requirement = protocolNode != NULL ?
        xl6CgenProtocolRequirementBySymbol(
            protocolNode,
            semanticSource->semanticTargetSymbolId) : NULL;
    if ( requirement == NULL && protocolNode != NULL &&
         hirTarget->semanticSymbolId != 0 ) {
        requirement = xl6CgenProtocolRequirementBySymbol(
            protocolNode,
            hirTarget->semanticSymbolId);
    }
    if ( protocolNode == NULL || requirement == NULL ) {
        xl6CgenError(ctx, semanticSource, "protocol property witness target is missing");
        xl6AstFree(receiverType);
        return 1;
    }
    concreteRequirement = NULL;
    callRequirement = requirement;
    if ( xl6CgenProtocolIsGeneric(protocolNode) ) {
        concreteRequirement = xl6CgenConcreteProtocolRequirement(
            ctx,
            protocolNode,
            concreteType,
            requirement);
        if ( concreteRequirement == NULL ) {
            xl6CgenError(
                ctx,
                semanticSource,
                "cannot instantiate generic protocol property witness");
            xl6OutputPuts("0", ctx->out);
            xl6AstFree(receiverType);
            return 1;
        }
        callRequirement = concreteRequirement;
    }

    call = xl6CgenPropertyTempCall(member, indexNode, valueExpr);
    if ( call == NULL ) {
        xl6CgenError(ctx, semanticSource, "cannot allocate protocol property call");
        xl6OutputPuts("0", ctx->out);
        xl6AstFree(concreteRequirement);
        xl6AstFree(receiverType);
        return 1;
    }

    if ( concreteRequirement != NULL ) {
        xl6CgenGenericProtocolCallSymbol(
            ctx,
            concreteType,
            callRequirement);
    } else {
        xl6CgenProtocolCallSymbol(ctx, protocolNode, requirement);
    }
    xl6OutputPutc('(', ctx->out);
    xl6CgenExpr(ctx, receiver);
    fixedCount = xl6CgenFixedParamCount(
        xl6CgenCallableParams(callRequirement));
    if ( fixedCount > 0 ) {
        xl6OutputPuts(", ", ctx->out);
        xl6CgenKnownCallArgs(
            ctx,
            callRequirement,
            call->childCount > 1 ? call->children[1] : NULL);
    }
    xl6OutputPuts(", ", ctx->out);
    xl6OutputPuts(
        xl6CgenExprIsXValueBorrow(ctx, receiver) ? "false" : "true",
        ctx->out);
    xl6OutputPuts(", ", ctx->out);
    file = ctx->debugSourcePath != NULL ? ctx->debugSourcePath : "";
    xl6CgenCStringLiteral(ctx->out, file);
    xl6OutputPrintf(
        ctx->out,
        ", %d, %d)",
        semanticSource->line,
        semanticSource->column);

    xl6CgenProtocolTempCallFree(call);
    xl6AstFree(concreteRequirement);
    xl6AstFree(receiverType);
    return 1;
}

static void xl6CgenUserGenericProtocolProtos(XL6_CgenContext* ctx)
{
    size_t instanceIndex;

    for ( instanceIndex = 0;
          ctx != NULL &&
          instanceIndex < ctx->genericProtocolInstanceCount;
          ++instanceIndex ) {
        const XL6_CgenGenericProtocolInstance* instance =
            &ctx->genericProtocolInstances[instanceIndex];
        const XL6_AstNode* body =
            xl6CgenProtocolBody(instance->protocolNode);
        size_t requirementIndex;

        xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
        xl6CgenGenericProtocolTypeDescSymbol(ctx, instance->typeNode);
        xl6OutputPuts("(void);\n", ctx->out);
        for ( requirementIndex = 0;
              body != NULL && requirementIndex < body->childCount;
              ++requirementIndex ) {
            XL6_AstNode* requirement =
                xl6CgenConcreteProtocolRequirement(
                    ctx,
                    instance->protocolNode,
                    instance->typeNode,
                    body->children[requirementIndex]);
            const XL6_AstNode* returns;

            if ( requirement == NULL ) {
                xl6CgenError(
                    ctx,
                    body->children[requirementIndex],
                    "cannot instantiate generic protocol requirement");
                continue;
            }
            returns = requirement->childCount > 2 ?
                requirement->children[2] : NULL;
            if ( returns != NULL && returns->childCount > 1 ) {
                xl6CgenFunctionReturnStructDecl(
                    ctx,
                    xl6CgenCallableParams(requirement),
                    returns);
            }
            xl6OutputPuts("static ", ctx->out);
            xl6CgenProtocolReturnType(ctx, requirement);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenGenericProtocolCallSymbol(
                ctx,
                instance->typeNode,
                requirement);
            xl6OutputPutc('(', ctx->out);
            xl6CgenProtocolParamDecls(
                ctx,
                xl6CgenCallableParams(requirement),
                1,
                1);
            xl6OutputPuts(");\n", ctx->out);
            xl6AstFree(requirement);
        }
    }
}

static void xl6CgenUserProtocolProtos(XL6_CgenContext* ctx)
{
    size_t i;

    if ( !xl6CgenHasUserProtocols(ctx) ) {
        return;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* protocolNode = ctx->root->children[i];
        const XL6_AstNode* body;
        size_t j;

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
        xl6CgenProtocolTypeDescSymbol(ctx->out, protocolNode);
        xl6OutputPuts("(void);\n", ctx->out);

        body = xl6CgenProtocolBody(protocolNode);
        for ( j = 0; body != NULL && j < body->childCount; ++j ) {
            const XL6_AstNode* requirement = body->children[j];
            const XL6_AstNode* returns = requirement != NULL &&
                                         requirement->childCount > 2
                ? requirement->children[2]
                : NULL;

            if ( returns != NULL && returns->childCount > 1 ) {
                xl6CgenFunctionReturnStructDecl(
                    ctx,
                    xl6CgenCallableParams(requirement),
                    returns);
            }
            xl6OutputPuts("static ", ctx->out);
            xl6CgenProtocolReturnType(ctx, requirement);
            xl6OutputPutc(' ', ctx->out);
            xl6CgenProtocolCallSymbol(ctx, protocolNode, requirement);
            xl6OutputPutc('(', ctx->out);
            xl6CgenProtocolParamDecls(
                ctx,
                xl6CgenCallableParams(requirement),
                1,
                1);
            xl6OutputPuts(");\n", ctx->out);
        }
    }
    xl6CgenUserGenericProtocolProtos(ctx);
    xl6OutputPuts(
        "static int xl6_protocol_module_init(void);\n"
        "static void xl6_protocol_module_unit(void);\n"
        "static int xl6_generic_class_protocol_module_init(void);\n"
        "static void xl6_generic_class_protocol_module_unit(void);\n"
        "static int xl6_generic_protocol_module_init(void);\n"
        "static void xl6_generic_protocol_module_unit(void);\n\n",
        ctx->out);
}

/*
    nullable/protocol 容器的运行时类型描述会早于完整 witness 原型生成。
    单独提前声明 TypeDesc，避免 C 编译器把首次调用误判为返回 int。
*/
static void xl6CgenUserProtocolTypeProtos(XL6_CgenContext* ctx)
{
    size_t i;

    if ( !xl6CgenHasUserProtocols(ctx) ) {
        return;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        const XL6_AstNode* protocolNode = ctx->root->children[i];

        if ( protocolNode == NULL ||
             protocolNode->kind != XL6_AST_PROTOCOL ||
             xl6CgenProtocolIsGeneric(protocolNode) ) {
            continue;
        }
        xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
        xl6CgenProtocolTypeDescSymbol(ctx->out, protocolNode);
        xl6OutputPuts("(void);\n", ctx->out);
    }
    for ( i = 0; i < ctx->genericProtocolInstanceCount; ++i ) {
        xl6OutputPuts("static const xrt_type_desc* ", ctx->out);
        xl6CgenGenericProtocolTypeDescSymbol(
            ctx,
            ctx->genericProtocolInstances[i].typeNode);
        xl6OutputPuts("(void);\n", ctx->out);
    }
    xl6OutputPutc('\n', ctx->out);
}

