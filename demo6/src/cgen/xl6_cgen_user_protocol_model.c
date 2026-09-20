/*
    用户 protocol 的 C 后端。

    protocol 值本身仍由 xvalue* 承载。具体 class 的数据布局没有第二份包装；
    witness thunk 只负责把 xvalue* 中的 record 数据恢复为 class self 指针。
*/

static void xl6CgenEmitXrtTypeDescRef(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode);
static void xl6CgenEmitFunctionSigObject(
    XL6_CgenContext* ctx,
    const char* localName,
    const char* name,
    unsigned long long signatureId,
    const XL6_AstNode* params,
    const XL6_AstNode* returns);
static void xl6CgenProtocolRequirementSymbolSuffix(
    XL6_CgenContext* ctx,
    const XL6_AstNode* requirement);

static const XL6_AstNode* xl6CgenProtocolBody(const XL6_AstNode* protocolNode)
{
    return protocolNode != NULL && protocolNode->childCount > 0
        ? protocolNode->children[0]
        : NULL;
}

static const XL6_AstNode* xl6CgenProtocolTypeParams(const XL6_AstNode* protocolNode)
{
    return protocolNode != NULL && protocolNode->childCount > 1
        ? protocolNode->children[1]
        : NULL;
}

static int xl6CgenProtocolIsGeneric(const XL6_AstNode* protocolNode)
{
    const XL6_AstNode* params = xl6CgenProtocolTypeParams(protocolNode);
    return params != NULL && params->childCount > 0;
}

static int xl6CgenHasGenericProtocolInstance(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* typeNode)
{
    size_t i;

    for ( i = 0; ctx != NULL && i < ctx->genericProtocolInstanceCount; ++i ) {
        if ( ctx->genericProtocolInstances[i].protocolNode == protocolNode &&
             xl6CgenTypeNodeEqual(
                 ctx->genericProtocolInstances[i].typeNode,
                 typeNode) ) {
            return 1;
        }
    }
    return 0;
}

static int xl6CgenAddGenericProtocolInstance(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* typeNode)
{
    XL6_CgenGenericProtocolInstance* items;
    XL6_AstNode* canonicalType;
    size_t capacity;

    if ( ctx == NULL || protocolNode == NULL || typeNode == NULL ||
         !xl6CgenProtocolIsGeneric(protocolNode) ) {
        return 1;
    }
    canonicalType = xl6CgenCloneCanonicalTypeNode(ctx, typeNode);
    if ( canonicalType == NULL ) {
        return 0;
    }
    if ( xl6CgenHasGenericProtocolInstance(
            ctx,
            protocolNode,
            canonicalType) ) {
        xl6AstFree(canonicalType);
        return 1;
    }
    if ( ctx->genericProtocolInstanceCount ==
         ctx->genericProtocolInstanceCapacity ) {
        capacity = ctx->genericProtocolInstanceCapacity == 0 ?
            8 : ctx->genericProtocolInstanceCapacity * 2;
        items = (XL6_CgenGenericProtocolInstance*)xrtRealloc(
            ctx->genericProtocolInstances,
            capacity * sizeof(XL6_CgenGenericProtocolInstance));
        if ( items == NULL ) {
            xl6AstFree(canonicalType);
            return 0;
        }
        ctx->genericProtocolInstances = items;
        ctx->genericProtocolInstanceCapacity = capacity;
    }
    ctx->genericProtocolInstances[ctx->genericProtocolInstanceCount].protocolNode =
        protocolNode;
    ctx->genericProtocolInstances[ctx->genericProtocolInstanceCount].typeNode =
        canonicalType;
    ctx->genericProtocolInstanceCount += 1;
    return 1;
}

static void xl6CgenCollectGenericProtocolInstancesInNode(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    const XL6_HirNode* hirNode;
    const XL6_AstNode* protocolNode;
    const XL6_AstNode* params;
    const XL6_AstNode* args;
    size_t i;

    if ( ctx == NULL || node == NULL ) {
        return;
    }
    if ( node->kind == XL6_AST_TYPE && node->text != NULL ) {
        protocolNode = xl6CgenFindProtocol(ctx, node->text);
        params = xl6CgenProtocolTypeParams(protocolNode);
        args = node->childCount > 0 ? node->children[0] : NULL;
        if ( protocolNode != NULL && params != NULL && args != NULL &&
             params->childCount > 0 &&
             params->childCount == args->childCount ) {
            if ( !xl6CgenAddGenericProtocolInstance(
                    ctx,
                    protocolNode,
                    node) ) {
                xl6CgenError(
                    ctx,
                    node,
                    "out of memory while collecting generic protocol instance");
            }
        }
    }
    hirNode = xl6CgenHirNode(ctx, node);
    if ( hirNode != NULL && hirNode->type != XL6_HIR_TYPE_INVALID ) {
        XL6_AstNode* hirType = xl6HirTypeCloneAst(&ctx->hir->types, hirNode->type);

        if ( hirType == NULL ) {
            xl6CgenError(ctx, node, "cannot restore typed HIR generic protocol discovery type");
        } else {
            xl6CgenCollectGenericProtocolInstancesInNode(ctx, hirType);
            xl6AstFree(hirType);
        }
    }
    for ( i = 0; i < node->childCount; ++i ) {
        xl6CgenCollectGenericProtocolInstancesInNode(ctx, node->children[i]);
    }
}

static void xl6CgenCollectGenericProtocolInstances(
    XL6_CgenContext* ctx,
    const XL6_AstNode* root)
{
    xl6CgenCollectGenericProtocolInstancesInNode(ctx, root);
}

static XL6_AstNode* xl6CgenCloneProtocolCallableNode(
    XL6_CgenContext* ctx,
    const XL6_AstNode* node)
{
    XL6_AstNode* copy;
    size_t i;

    if ( node == NULL ) {
        return NULL;
    }
    if ( node->kind == XL6_AST_TYPE ) {
        return xl6CgenCloneCanonicalTypeNode(ctx, node);
    }
    copy = node->text != NULL ?
        xl6AstNewText(node->kind, node->text, strlen(node->text), NULL) :
        xl6AstNew(node->kind, NULL);
    if ( copy == NULL ) {
        return NULL;
    }
    copy->flags = node->flags;
    copy->semanticTypeId = node->semanticTypeId;
    copy->semanticTypeAbiId = node->semanticTypeAbiId;
    copy->semanticSymbolId = node->semanticSymbolId;
    copy->semanticTargetSymbolId = node->semanticTargetSymbolId;
    copy->semanticCompoundOperatorSymbolId =
        node->semanticCompoundOperatorSymbolId;
    copy->semanticCallKind = node->semanticCallKind;
    copy->semanticLvalueKind = node->semanticLvalueKind;
    copy->semanticOwnership = node->semanticOwnership;
    copy->line = node->line;
    copy->column = node->column;
    for ( i = 0; i < node->childCount; ++i ) {
        XL6_AstNode* child = xl6CgenCloneProtocolCallableNode(
            ctx,
            node->children[i]);
        if ( child == NULL ) {
            xl6AstFree(copy);
            return NULL;
        }
        xl6AstAddChild(copy, child);
    }
    return copy;
}

static XL6_AstNode* xl6CgenConcreteProtocolRequirement(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* protocolType,
    const XL6_AstNode* requirement)
{
    const XL6_AstNode* savedParams;
    const XL6_AstNode* savedArgs;
    XL6_AstNode* result;

    if ( ctx == NULL || protocolNode == NULL ||
         protocolType == NULL || requirement == NULL ) {
        return NULL;
    }
    savedParams = ctx->currentGenericParams;
    savedArgs = ctx->currentGenericArgs;
    ctx->currentGenericParams = xl6CgenProtocolTypeParams(protocolNode);
    ctx->currentGenericArgs = protocolType->childCount > 0 ?
        protocolType->children[0] : NULL;
    result = xl6CgenCloneProtocolCallableNode(ctx, requirement);
    ctx->currentGenericParams = savedParams;
    ctx->currentGenericArgs = savedArgs;
    return result;
}

static void xl6CgenGenericProtocolSuffix(
    XL6_CgenContext* ctx,
    const XL6_AstNode* typeNode)
{
    xl6OutputPrintf(
        ctx->out,
        "__%016llX",
        xl6CgenTypeIdForTypeNode(ctx, typeNode));
}

static void xl6CgenGenericProtocolTypeDescSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolType)
{
    xl6OutputPuts("xl6_protocol_type_desc_", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        protocolType != NULL && protocolType->text != NULL ?
            protocolType->text : "_");
    xl6CgenGenericProtocolSuffix(ctx, protocolType);
}

static void xl6CgenGenericProtocolCallSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolType,
    const XL6_AstNode* requirement)
{
    xl6OutputPuts("xl6_protocol_call_", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        protocolType != NULL && protocolType->text != NULL ?
            protocolType->text : "_");
    xl6CgenGenericProtocolSuffix(ctx, protocolType);
    xl6OutputPuts("__", ctx->out);
    xl6CgenProtocolRequirementSymbolSuffix(ctx, requirement);
}

static void xl6CgenGenericProtocolThunkSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classType,
    const XL6_AstNode* protocolType,
    const XL6_AstNode* requirement)
{
    xl6OutputPuts("xl6_protocol_witness_", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        classType != NULL && classType->text != NULL ?
            classType->text : "_");
    xl6CgenGenericProtocolSuffix(ctx, classType);
    xl6OutputPuts("__", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        protocolType != NULL && protocolType->text != NULL ?
            protocolType->text : "_");
    xl6CgenGenericProtocolSuffix(ctx, protocolType);
    xl6OutputPuts("__", ctx->out);
    xl6CgenProtocolRequirementSymbolSuffix(ctx, requirement);
}

static void xl6CgenGenericProtocolWitnessSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classType,
    const XL6_AstNode* protocolType)
{
    xl6OutputPuts("xl6_protocol_table_", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        classType != NULL && classType->text != NULL ?
            classType->text : "_");
    xl6CgenGenericProtocolSuffix(ctx, classType);
    xl6OutputPuts("__", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        protocolType != NULL && protocolType->text != NULL ?
            protocolType->text : "_");
    xl6CgenGenericProtocolSuffix(ctx, protocolType);
}

static int xl6CgenHasUserProtocols(const XL6_CgenContext* ctx)
{
    size_t i;

    if ( ctx == NULL || ctx->root == NULL ) {
        return 0;
    }
    for ( i = 0; i < ctx->root->childCount; ++i ) {
        if ( ctx->root->children[i] != NULL &&
             ctx->root->children[i]->kind == XL6_AST_PROTOCOL ) {
            return 1;
        }
    }
    return 0;
}

/*
    protocol witness 只面向已经确定布局的具体 class。
    普通 class 与泛型 class 单态实例统一从这里枚举，后续生成器不再按两套规则判断。
*/
static size_t xl6CgenProtocolClassCandidateCount(const XL6_CgenContext* ctx)
{
    if ( ctx == NULL || ctx->root == NULL ) {
        return 0;
    }
    return ctx->root->childCount + ctx->genericClassInstanceCount;
}

static int xl6CgenProtocolClassCandidate(
    XL6_CgenContext* ctx,
    size_t index,
    const XL6_AstNode** outClassNode,
    const XL6_AstNode** outClassType,
    XL6_AstNode* typeStorage)
{
    const XL6_AstNode* classNode;
    const XL6_CgenGenericClassInstance* instance;

    if ( outClassNode != NULL ) {
        *outClassNode = NULL;
    }
    if ( outClassType != NULL ) {
        *outClassType = NULL;
    }
    if ( ctx == NULL || ctx->root == NULL || typeStorage == NULL ) {
        return 0;
    }

    if ( index < ctx->root->childCount ) {
        classNode = ctx->root->children[index];
        if ( classNode == NULL || classNode->kind != XL6_AST_CLASS ||
             xl6CgenClassHasTypeParams(classNode) ) {
            return 0;
        }
        memset(typeStorage, 0, sizeof(*typeStorage));
        typeStorage->kind = XL6_AST_TYPE;
        typeStorage->text = classNode->text;
        if ( outClassNode != NULL ) {
            *outClassNode = classNode;
        }
        if ( outClassType != NULL ) {
            *outClassType = typeStorage;
        }
        return 1;
    }

    index -= ctx->root->childCount;
    if ( index >= ctx->genericClassInstanceCount ) {
        return 0;
    }
    instance = &ctx->genericClassInstances[index];
    if ( instance->classNode == NULL || instance->typeNode == NULL ) {
        return 0;
    }
    if ( outClassNode != NULL ) {
        *outClassNode = instance->classNode;
    }
    if ( outClassType != NULL ) {
        *outClassType = instance->typeNode;
    }
    return 1;
}

static void xl6CgenProtocolTypeDescSymbol(
    XL6_Output* out,
    const XL6_AstNode* protocolNode)
{
    xl6OutputPuts("xl6_protocol_type_desc_", out);
    xl6CgenFputsEncodedText(
        out,
        protocolNode != NULL && protocolNode->text != NULL
            ? protocolNode->text
            : "_");
}

static const char* xl6CgenProtocolRequirementName(
    const XL6_AstNode* requirement,
    char* buffer,
    size_t capacity)
{
    const char* name = xl6CgenCallableName(requirement);
    const char* prefix = "";

    if ( requirement != NULL && requirement->kind == XL6_AST_GET ) {
        prefix = "get:";
    } else if ( requirement != NULL && requirement->kind == XL6_AST_LET ) {
        prefix = "let:";
    } else if ( requirement != NULL && requirement->kind == XL6_AST_ASYNC_FUNC ) {
        prefix = "async:";
    }
    if ( buffer == NULL || capacity == 0 ) {
        return "";
    }
    snprintf(buffer, capacity, "%s%s", prefix, name != NULL ? name : "");
    return buffer;
}

static unsigned long long xl6CgenProtocolRequirementSignatureId(
    XL6_CgenContext* ctx,
    const XL6_AstNode* requirement)
{
    return xl6CgenFunctionSignatureId(
        ctx,
        xl6CgenCallableParams(requirement),
        requirement != NULL && requirement->childCount > 2
            ? requirement->children[2]
            : NULL);
}

static void xl6CgenProtocolRequirementSymbolSuffix(
    XL6_CgenContext* ctx,
    const XL6_AstNode* requirement)
{
    char runtimeName[384];

    xl6CgenFputsEncodedText(
        ctx->out,
        xl6CgenProtocolRequirementName(
            requirement,
            runtimeName,
            sizeof(runtimeName)));
    xl6OutputPrintf(
        ctx->out,
        "__%016llX",
        xl6CgenProtocolRequirementSignatureId(ctx, requirement));
}

static void xl6CgenProtocolCallSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* requirement)
{
    xl6OutputPuts("xl6_protocol_call_", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        protocolNode != NULL && protocolNode->text != NULL
            ? protocolNode->text
            : "_");
    xl6OutputPuts("__", ctx->out);
    xl6CgenProtocolRequirementSymbolSuffix(ctx, requirement);
}

static void xl6CgenProtocolThunkSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* requirement)
{
    xl6OutputPuts("xl6_protocol_witness_", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        classNode != NULL && classNode->text != NULL ? classNode->text : "_");
    xl6OutputPuts("__", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        protocolNode != NULL && protocolNode->text != NULL
            ? protocolNode->text
            : "_");
    xl6OutputPuts("__", ctx->out);
    xl6CgenProtocolRequirementSymbolSuffix(ctx, requirement);
}

static void xl6CgenProtocolWitnessSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* protocolNode)
{
    xl6OutputPuts("xl6_protocol_table_", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        classNode != NULL && classNode->text != NULL ? classNode->text : "_");
    xl6OutputPuts("__", ctx->out);
    xl6CgenFputsEncodedText(
        ctx->out,
        protocolNode != NULL && protocolNode->text != NULL
            ? protocolNode->text
            : "_");
}

static int xl6CgenProtocolRequirementKindMatches(
    const XL6_AstNode* requirement,
    const XL6_AstNode* candidate)
{
    if ( requirement == NULL || candidate == NULL ) {
        return 0;
    }
    if ( requirement->kind == XL6_AST_ASYNC_FUNC ) {
        return candidate->kind == XL6_AST_ASYNC_FUNC;
    }
    if ( requirement->kind == XL6_AST_FUNC ) {
        return candidate->kind == XL6_AST_FUNC ||
               candidate->kind == XL6_AST_EXPORT_FUNC ||
               candidate->kind == XL6_AST_NATIVE_EXTERN_FUNC;
    }
    return requirement->kind == candidate->kind &&
           (requirement->kind == XL6_AST_GET ||
            requirement->kind == XL6_AST_LET);
}

static int xl6CgenProtocolSignaturesEqual(
    XL6_CgenContext* ctx,
    const XL6_AstNode* requirement,
    const XL6_AstNode* candidate)
{
    const XL6_AstNode* requiredTypeParams;
    const XL6_AstNode* candidateTypeParams;

    if ( !xl6CgenProtocolRequirementKindMatches(requirement, candidate) ||
         !xl6CTextEquals(
             xl6CgenCallableName(requirement),
             xl6CgenCallableName(candidate)) ||
         xl6CgenIsStaticMember(candidate) ||
         xl6CgenAstHasFlag(candidate, XL6_AST_FLAG_PRIVATE) ||
         xl6CgenAstHasFlag(candidate, XL6_AST_FLAG_PROTECTED) ) {
        return 0;
    }

    requiredTypeParams = requirement->childCount > 4
        ? requirement->children[4]
        : NULL;
    candidateTypeParams = candidate->childCount > 4
        ? candidate->children[4]
        : NULL;
    if ( (requiredTypeParams != NULL ? requiredTypeParams->childCount : 0) !=
         (candidateTypeParams != NULL ? candidateTypeParams->childCount : 0) ) {
        return 0;
    }

    /*
        具体的泛型约束与 alpha-equivalence 已由语义层验证。
        后端这里只接受相同的实例化后函数 ABI，避免重新实现一套约束求解器。
    */
    return xl6CgenVirtualParamListsEqual(
               ctx,
               xl6CgenCallableParams(requirement),
               xl6CgenCallableParams(candidate)) &&
           xl6CgenVirtualReturnListsEqual(
               ctx,
               requirement->childCount > 2 ? requirement->children[2] : NULL,
               candidate->childCount > 2 ? candidate->children[2] : NULL);
}

static const XL6_AstNode* xl6CgenFindProtocolWitnessMember(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* requirement,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* body;
    size_t i;

    if ( outOwnerClass != NULL ) {
        *outOwnerClass = NULL;
    }
    if ( classNode == NULL || requirement == NULL ) {
        return NULL;
    }

    body = xl6CgenClassBody(classNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* candidate = body->children[i];

        if ( xl6CgenProtocolSignaturesEqual(ctx, requirement, candidate) ) {
            if ( outOwnerClass != NULL ) {
                *outOwnerClass = classNode;
            }
            return candidate;
        }
    }

    return xl6CgenFindProtocolWitnessMember(
        ctx,
        xl6CgenBaseClass(ctx, classNode),
        requirement,
        outOwnerClass);
}

static int xl6CgenConcreteClassSatisfiesProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* classType,
    const XL6_AstNode* protocolNode,
    const XL6_AstNode* protocolType)
{
    const XL6_AstNode* savedParams;
    const XL6_AstNode* savedArgs;
    const XL6_AstNode* body;
    size_t i;
    int result = 1;

    if ( ctx == NULL || classNode == NULL || classType == NULL ||
         protocolNode == NULL ) {
        return 0;
    }

    savedParams = ctx->currentGenericParams;
    savedArgs = ctx->currentGenericArgs;
    if ( xl6CgenClassHasTypeParams(classNode) ) {
        if ( classType->childCount == 0 || classType->children[0] == NULL ) {
            return 0;
        }
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classType->children[0];
    }

    body = xl6CgenProtocolBody(protocolNode);
    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* requirement = body->children[i];
        XL6_AstNode* concreteRequirement = NULL;

        if ( xl6CgenProtocolIsGeneric(protocolNode) ) {
            if ( protocolType == NULL ) {
                result = 0;
                break;
            }
            concreteRequirement = xl6CgenConcreteProtocolRequirement(
                ctx,
                protocolNode,
                protocolType,
                requirement);
            requirement = concreteRequirement;
        }
        if ( requirement == NULL ||
             xl6CgenFindProtocolWitnessMember(
                 ctx,
                 classNode,
                 requirement,
                 NULL) == NULL ) {
            result = 0;
        }
        xl6AstFree(concreteRequirement);
        if ( !result ) {
            break;
        }
    }

    ctx->currentGenericParams = savedParams;
    ctx->currentGenericArgs = savedArgs;
    return result;
}

static const XL6_AstNode* xl6CgenFindConcreteProtocolWitnessMember(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* classType,
    const XL6_AstNode* requirement,
    const XL6_AstNode** outOwnerClass)
{
    const XL6_AstNode* savedParams;
    const XL6_AstNode* savedArgs;
    const XL6_AstNode* result;

    if ( ctx == NULL || classNode == NULL || classType == NULL ) {
        return NULL;
    }
    savedParams = ctx->currentGenericParams;
    savedArgs = ctx->currentGenericArgs;
    if ( xl6CgenClassHasTypeParams(classNode) ) {
        if ( classType->childCount == 0 || classType->children[0] == NULL ) {
            return NULL;
        }
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classType->children[0];
    }
    result = xl6CgenFindProtocolWitnessMember(
        ctx,
        classNode,
        requirement,
        outOwnerClass);
    ctx->currentGenericParams = savedParams;
    ctx->currentGenericArgs = savedArgs;
    return result;
}

static void xl6CgenConcreteClassCallableSymbol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* classType,
    const XL6_AstNode* ownerClass,
    const XL6_AstNode* callable)
{
    const XL6_AstNode* savedParams;
    const XL6_AstNode* savedArgs;

    if ( xl6CgenClassHasTypeParams(classNode) ) {
        savedParams = ctx->currentGenericParams;
        savedArgs = ctx->currentGenericArgs;
        ctx->currentGenericParams = xl6CgenClassTypeParams(classNode);
        ctx->currentGenericArgs = classType != NULL && classType->childCount > 0
            ? classType->children[0]
            : NULL;
        xl6CgenGenericClassCallableSymbol(
            ctx,
            ownerClass != NULL ? ownerClass : classNode,
            classType,
            callable);
        ctx->currentGenericParams = savedParams;
        ctx->currentGenericArgs = savedArgs;
        return;
    }
    xl6CgenClassCallableSymbol(
        ctx,
        ownerClass != NULL ? ownerClass : classNode,
        callable);
}

static int xl6CgenClassSatisfiesProtocol(
    XL6_CgenContext* ctx,
    const XL6_AstNode* classNode,
    const XL6_AstNode* protocolNode)
{
    XL6_AstNode classType;

    if ( classNode == NULL || protocolNode == NULL ||
         xl6CgenClassHasTypeParams(classNode) ||
         xl6CgenProtocolIsGeneric(protocolNode) ) {
        return 0;
    }
    memset(&classType, 0, sizeof(classType));
    classType.kind = XL6_AST_TYPE;
    classType.text = classNode->text;
    return xl6CgenConcreteClassSatisfiesProtocol(
        ctx,
        classNode,
        &classType,
        protocolNode,
        NULL);
}

static void xl6CgenProtocolReturnType(
    XL6_CgenContext* ctx,
    const XL6_AstNode* requirement)
{
    const XL6_AstNode* returns = requirement != NULL &&
                                 requirement->childCount > 2
        ? requirement->children[2]
        : NULL;

      if ( requirement != NULL && requirement->kind == XL6_AST_ASYNC_FUNC ) {
          xl6OutputPuts("xfuture*", ctx->out);
    } else if ( returns != NULL && returns->childCount > 1 ) {
        xl6CgenFunctionReturnStructName(
            ctx,
            xl6CgenCallableParams(requirement),
            returns);
    } else if ( returns != NULL && returns->childCount == 1 ) {
        xl6OutputPuts(xl6CgenTypeName(ctx, returns->children[0]), ctx->out);
    } else {
        xl6OutputPuts("void", ctx->out);
    }
}

static void xl6CgenProtocolParamDecls(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params,
    int includeSelf,
    int includeDispatchState)
{
    size_t i;
    int wrote = 0;

    if ( includeSelf ) {
        xl6OutputPuts("xvalue* self", ctx->out);
        wrote = 1;
    }
    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];
        const XL6_AstNode* typeNode = param != NULL && param->childCount > 0
            ? param->children[0]
            : NULL;

        if ( wrote ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6OutputPuts(xl6CgenParamCType(ctx, typeNode), ctx->out);
        xl6OutputPutc(' ', ctx->out);
        xl6CgenFputsCIdent(
            ctx->out,
            param != NULL && param->text != NULL ? param->text : "_");
        wrote = 1;
    }
    if ( includeDispatchState ) {
        if ( wrote ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6OutputPuts(
            "bool releaseSelf, const char* file, int line, int column",
            ctx->out);
        wrote = 1;
    }
    if ( !wrote ) {
        xl6OutputPuts("void", ctx->out);
    }
}

static void xl6CgenProtocolParamNames(
    XL6_CgenContext* ctx,
    const XL6_AstNode* params,
    int wrote)
{
    size_t i;

    for ( i = 0; params != NULL && i < params->childCount; ++i ) {
        const XL6_AstNode* param = params->children[i];

        if ( wrote ) {
            xl6OutputPuts(", ", ctx->out);
        }
        xl6CgenFputsCIdent(
            ctx->out,
            param != NULL && param->text != NULL ? param->text : "_");
        wrote = 1;
    }
}

static int xl6CgenProtocolRequirementHasReturn(
    const XL6_AstNode* requirement)
{
    const XL6_AstNode* returns = requirement != NULL &&
                                 requirement->childCount > 2
        ? requirement->children[2]
        : NULL;
    return requirement != NULL &&
           (requirement->kind == XL6_AST_ASYNC_FUNC ||
            (returns != NULL && returns->childCount > 0));
}

static void xl6CgenProtocolDefaultReturn(
    XL6_CgenContext* ctx,
    const XL6_AstNode* requirement)
{
    if ( !xl6CgenProtocolRequirementHasReturn(requirement) ) {
        xl6OutputPuts("        return;\n", ctx->out);
        return;
    }
    xl6OutputPuts("        ", ctx->out);
    xl6CgenProtocolReturnType(ctx, requirement);
    xl6OutputPuts(" result;\n", ctx->out);
    xl6OutputPuts("        memset(&result, 0, sizeof(result));\n", ctx->out);
    xl6OutputPuts("        return result;\n", ctx->out);
}

static const XL6_AstNode* xl6CgenProtocolRequirementBySymbol(
    const XL6_AstNode* protocolNode,
    unsigned int symbolId)
{
    const XL6_AstNode* body = xl6CgenProtocolBody(protocolNode);
    size_t i;

    for ( i = 0; body != NULL && i < body->childCount; ++i ) {
        const XL6_AstNode* requirement = body->children[i];
        if ( requirement != NULL &&
             requirement->semanticSymbolId == symbolId ) {
            return requirement;
        }
    }
    return NULL;
}

