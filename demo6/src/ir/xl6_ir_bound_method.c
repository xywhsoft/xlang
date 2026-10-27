/* Included after named-adapter construction. Binding snapshots one receiver;
 * the ordinary typed adapter CFG owns defaults, calls and result transfer. */
static int xl6IrLowerBoundMethodKind(const XL6_IrClosureTarget* closure)
{ return closure && closure->kind==XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER; }

static XL6_IrCallTarget xl6IrLowerBoundMethodUserTarget(const XL6_IrCallTarget* target)
{
    XL6_IrCallTarget user=*target;
    --user.parameterCount;
    ++user.parameterTypes;
    ++user.parameterAbis;
    return user;
}

static int xl6IrLowerTryTypedBoundMethod(XL6_IrLowerContext* ctx,XL6_IrNodeId origin,
    XL6_IrNodeId sourceId,XL6_IrTypeId signatureId,XL6_IrValueId* output)
{
    XL6_IrModule* module=ctx->module;
    const XL6_IrNode* source=xl6IrNodeGet(module,sourceId);
    const XL6_IrType* signature=xl6IrTypeGet(&module->types,signatureId);
    if(!source || source->kind!=XL6_IR_NODE_MEMBER || source->sourceKind!=XL6_AST_MEMBER ||
       source->childCount!=1 || !source->text || !signature || signature->kind!=XL6_IR_TYPE_FUNCTION ||
       signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED) return 0;
    XL6_IrNodeId receiverId=module->children[source->firstChild];
    const XL6_IrNode* receiverNode=xl6IrNodeGet(module,receiverId);
    XL6_IrTypeId ownerId=receiverNode ? xl6IrNodeMaterializedValueType(module,receiverNode) : 0;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
    if(owner && owner->kind==XL6_IR_TYPE_NULLABLE && owner->paramCount==1) {
        ownerId=owner->params[0];owner=xl6IrTypeGet(&module->types,ownerId);
    }
    if(!owner || owner->kind!=XL6_IR_TYPE_CLASS || owner->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR) return 0;
    XL6_SymbolId selected=0;
    size_t methodLimit=module->dynamicMethodCount;
    for(size_t i=0;i<methodLimit;++i) {
        XL6_IrDynamicMethod method=module->dynamicMethods[i];
        source=xl6IrNodeGet(module,sourceId);
        signature=xl6IrTypeGet(&module->types,signatureId);
        if(method.ownerType!=ownerId || method.kind!=XL6_IR_DYNAMIC_CALL_METHOD ||
           !method.name || strcmp(method.name,source->text)) continue;
        XL6_SymbolId symbol=method.targetSymbol;
        if(xl6IrLowerIsGenericDeclaration(module,method.declaration)) {
            XL6_IrTypeId* parameters=NULL;size_t count=0;
            if(!xl6IrLowerGenericDeclarationTypeParams(module,method.declaration,&parameters,&count) || !count) {xrtFree(parameters);continue;}
            XL6_IrTypeId* resolved=xrtCalloc(count,sizeof(*resolved));
            if(!resolved) {xrtFree(parameters);ctx->failed=1;return -1;}
            const XL6_IrNode* declaration=xl6IrNodeGet(module,method.declaration);
            const XL6_IrNode* params=declaration && declaration->childCount>2 ? xl6IrNodeGet(module,module->children[declaration->firstChild+1]) : NULL;
            const XL6_IrNode* returns=declaration && declaration->childCount>2 ? xl6IrNodeGet(module,module->children[declaration->firstChild+2]) : NULL;
            bool matches=params && returns && params->childCount>=signature->paramCount && returns->childCount==signature->returnCount;
            for(size_t p=signature->paramCount;matches && p<params->childCount;++p) {
                const XL6_IrNode* parameter=xl6IrNodeGet(module,module->children[params->firstChild+p]);
                matches=parameter && parameter->childCount>=2;
            }
            for(size_t p=0;matches && p<signature->paramCount;++p) {
                const XL6_IrNode* parameter=xl6IrNodeGet(module,module->children[params->firstChild+p]);
                matches=parameter && xl6IrLowerUnifyGenericType(module,parameter->storageType,signature->params[p],parameters,count,resolved,1);
            }
            for(size_t r=0;matches && r<signature->returnCount;++r) {
                const XL6_IrNode* returned=xl6IrNodeGet(module,module->children[returns->firstChild+r]);
                matches=returned && xl6IrLowerUnifyGenericType(module,returned->storageType,signature->returns[r],parameters,count,resolved,1);
            }
            for(size_t p=0;matches && p<count;++p) matches=resolved[p]!=0;
            if(matches && output && !xl6IrLowerFindGenericSpecialization(module,method.declaration,resolved,count)) {
                xrtFree(parameters);xrtFree(resolved);
                return xl6IrLowerDiag(ctx,origin,"typed bound generic method was not closed before body lowering"),-1;
            }
            if(matches && !xl6IrLowerBuildGenericSpecialization(ctx,0,method.declaration,resolved,count,&symbol)) {
                xrtFree(parameters);xrtFree(resolved);return -1;
            }
            xrtFree(parameters);xrtFree(resolved);
            if(!matches || !output) continue;
            if(!xl6IrCallTargetGet(module,symbol) &&
               (symbol>=module->symbolNodeCapacity || !xl6IrLowerRegisterCallTarget(ctx,module->symbolNodes[symbol]))) return -1;
            signature=xl6IrTypeGet(&module->types,signatureId);
        }
        if(!output || !xl6IrBoundMethodTargetMatches(module,sourceId,symbol,signatureId)) continue;
        if(selected && selected!=symbol) return xl6IrLowerDiag(ctx,origin,"typed bound method requires one exact overload"),-1;
        selected=symbol;
    }
    if(!output || !selected) return 0;
    XL6_IrClosureTarget* closure=NULL;
    for(size_t i=0;i<module->closureTargetCount;++i)
        if(xl6IrLowerBoundMethodKind(&module->closureTargets[i]) &&
           module->closureTargets[i].declaration==sourceId && module->closureTargets[i].signatureType==signatureId) {
            closure=&module->closureTargets[i];break;
        }
    const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,selected);
    XL6_IrTypeId receiverType=target->parameterTypes[0];
    if(!closure) {
        if(!xl6IrLowerGrow((void**)&module->closureTargets,&module->closureTargetCapacity,module->closureTargetCount,sizeof(*module->closureTargets)) ||
           !xl6IrLowerGrow((void**)&module->closureCaptures,&module->closureCaptureCapacity,module->closureCaptureCount,sizeof(*module->closureCaptures))) {
            ctx->failed=1;return -1;
        }
        closure=&module->closureTargets[module->closureTargetCount++];memset(closure,0,sizeof(*closure));
        closure->kind=XL6_IR_CLOSURE_TARGET_BOUND_METHOD_ADAPTER;
        closure->declaration=sourceId;closure->targetSymbol=xl6IrLowerNewSyntheticSymbol(ctx);
        closure->adaptedTargetSymbol=selected;closure->signatureType=signatureId;
        closure->firstCapture=module->closureCaptureCount;closure->captureCount=1;
        closure->rawCallbackAbi=XL6_IR_CLOSURE_RAW_CALLBACK_NONE;
        XL6_IrClosureCapture* capture=&module->closureCaptures[module->closureCaptureCount++];
        XL6_IrNodeId selfId=xl6IrLowerFindClassReceiverInSubtree(module,target->declaration);
        const XL6_IrNode* self=xl6IrNodeGet(module,selfId);
        *capture=(XL6_IrClosureCapture){.sourceSymbol=self && self->targetSymbol ? self->targetSymbol : xl6IrLowerNewSyntheticSymbol(ctx),
            .storageSymbol=xl6IrLowerNewSyntheticSymbol(ctx),.type=receiverType,.abi=XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE};
        const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
        const XL6_IrNode* params=xl6IrNodeGet(module,module->children[declaration->firstChild+1]);
        XL6_IrCallTarget user=xl6IrLowerBoundMethodUserTarget(target);
        if(!closure->targetSymbol || !capture->sourceSymbol || !capture->storageSymbol ||
           !xl6IrLowerSetNamedAdapterCallParameterNames(module,closure,params,signature,&user) ||
           !xl6IrLowerSetClosureCallParameterDefaults(ctx,closure,params,signature,&user,1)) {
            xl6IrLowerDiag(ctx,origin,"typed bound method has no exact receiver/signature metadata");return -1;
        }
    }
    XL6_SymbolId closureSymbol=closure->targetSymbol;
    size_t first=ctx->evaluationOwnerCount;
    receiverNode=xl6IrNodeGet(module,receiverId);
    XL6_IrValueId receiver=xl6IrLowerNodeAsType(ctx,origin,receiverId,receiverNode,receiverType);
    receiver=xl6IrLowerProtectOwnedBorrow(ctx,origin,receiver);
    if(!receiver || !xl6IrLowerEmit(ctx,XL6_IR_INSTR_CLOSURE_MAKE,origin,signatureId,closureSymbol,&receiver,1,output) ||
       !xl6IrLowerEmitRuntimeErrorEdge(ctx,origin,xl6IrNodeGet(module,origin),*output) ||
       !xl6IrLowerCompleteEvaluationBorrows(ctx,origin,first,output)) return -1;
    return 1;
}

/* Specialization is graph construction. Complete it before executable lowering
 * can hold pointers into the typed node/type arenas. Reuse the verifier's
 * expected-type evidence instead of guessing from a method name. */
static int xl6IrLowerMaterializeTypedBoundMethods(XL6_IrLowerContext* ctx)
{
    XL6_IrModule* module=ctx->module;
    size_t nodeLimit=module->nodeCount,typeLimit=module->types.count;
    for(size_t n=1;n<=nodeLimit;++n) {
        const XL6_IrNode* source=xl6IrNodeGet(module,(XL6_IrNodeId)n);
        if(!source || source->kind!=XL6_IR_NODE_MEMBER || source->sourceKind!=XL6_AST_MEMBER) continue;
        bool generic=false;
        const XL6_IrNode* receiver=source->childCount==1 ? xl6IrNodeGet(module,module->children[source->firstChild]) : NULL;
        XL6_IrTypeId ownerId=receiver ? xl6IrNodeMaterializedValueType(module,receiver) : 0;
        const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
        if(owner && owner->kind==XL6_IR_TYPE_NULLABLE && owner->paramCount==1) ownerId=owner->params[0];
        for(size_t m=0;m<module->dynamicMethodCount;++m) {
            const XL6_IrDynamicMethod* method=&module->dynamicMethods[m];
            if(method->ownerType==ownerId && method->kind==XL6_IR_DYNAMIC_CALL_METHOD && method->name &&
               source->text && !strcmp(method->name,source->text) && xl6IrLowerIsGenericDeclaration(module,method->declaration)) {generic=true;break;}
        }
        if(!generic) continue;
        XL6_IrNodeId parent=source->parent;
        for(size_t t=1;t<=typeLimit;++t) {
            const XL6_IrType* signature=xl6IrTypeGet(&module->types,(XL6_IrTypeId)t);
            if(!signature || signature->kind!=XL6_IR_TYPE_FUNCTION ||
               signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED ||
               xl6IrLowerTypeContainsTypeParam(module,(XL6_IrTypeId)t,0)) continue;
            XL6_IrNodeId origin=parent;
            for(size_t depth=0;origin && depth<5;++depth) {
                const XL6_IrNode* context=xl6IrNodeGet(module,origin);
                bool explicitCast=context && context->sourceKind==XL6_AST_TYPE_AS && context->storageType==t;
                if(explicitCast || xl6IrContextualCallableCastAllowed(module,origin,(XL6_IrNodeId)n,(XL6_IrTypeId)t)) {
                    if(xl6IrLowerTryTypedBoundMethod(ctx,origin,(XL6_IrNodeId)n,(XL6_IrTypeId)t,NULL)<0) return 0;
                    break;
                }
                origin=context ? context->parent : 0;
            }
        }
    }
    return 1;
}
