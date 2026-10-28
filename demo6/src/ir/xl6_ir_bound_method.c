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
    XL6_IrNodeId selectedDeclaration=0;
    XL6_IrTypeId* selectedArgs=NULL;
    size_t selectedCount=0;
    uint64_t bestRank=UINT64_MAX;
    bool ambiguous=false;
    size_t methodLimit=module->nodeCount;
    /* Rank declaration shapes before instantiating a winner. As with a
     * direct call, an inferred parameter costs GENERIC_MATCH, and omitted
     * defaults break ties only after all parameter conversion costs. */
    for(size_t i=0;i<methodLimit;++i) {
        source=xl6IrNodeGet(module,sourceId);
        signature=xl6IrTypeGet(&module->types,signatureId);
        XL6_IrNodeId declarationId=(XL6_IrNodeId)i+1;
        if(!xl6IrBoundMethodCandidateMatches(module,sourceId,declarationId))continue;
        const XL6_IrNode* declaration=xl6IrNodeGet(module,declarationId);
        XL6_SymbolId targetSymbol=declaration->symbol;
        const XL6_IrNode* params=declaration && declaration->childCount>2 ? xl6IrNodeGet(module,module->children[declaration->firstChild+1]) : NULL;
        const XL6_IrNode* returns=declaration && declaration->childCount>2 ? xl6IrNodeGet(module,module->children[declaration->firstChild+2]) : NULL;
        if(!params || !returns || params->childCount<signature->paramCount ||
           (declaration->flags&XL6_AST_FLAG_STATIC))continue;
        uint64_t rank=0;
        bool matches=xl6CallRankAdd(&rank,0,(uint32_t)(params->childCount-signature->paramCount));
        for(size_t p=signature->paramCount;matches && p<params->childCount;++p) {
            const XL6_IrNode* parameter=xl6IrNodeGet(module,module->children[params->firstChild+p]);
            matches=parameter && parameter->childCount>=2;
        }
        XL6_IrTypeId* parameters=NULL,*resolved=NULL;
        size_t count=0;
        bool generic=xl6IrLowerIsGenericDeclaration(module,declarationId)!=0;
        if(matches && generic) {
            if(!xl6IrLowerGenericDeclarationTypeParams(module,declarationId,&parameters,&count) || !count) {xrtFree(parameters);continue;}
            resolved=xrtCalloc(count,sizeof(*resolved));
            if(!resolved) {xrtFree(parameters);xrtFree(selectedArgs);ctx->failed=1;return -1;}
            for(size_t p=0;matches && p<signature->paramCount;++p) {
                const XL6_IrNode* parameter=xl6IrNodeGet(module,module->children[params->firstChild+p]);
                if(!parameter) {matches=false;break;}
                XL6_IrNode concrete=*parameter;concrete.storageType=signature->params[p];
                matches=xl6IrParameterAbiForNode(module,&concrete)==signature->parameterAbis[p] &&
                    xl6IrLowerUnifyGenericType(module,parameter->storageType,signature->params[p],parameters,count,resolved,1);
                if(matches && xl6IrLowerTypeContainsTypeParam(module,parameter->storageType,0))
                    matches=xl6CallRankAdd(&rank,XL6_CALL_GENERIC_MATCH_COST,0)!=0;
            }
            XL6_IrTypeId asyncPayload=0;
            if(declaration->sourceKind==XL6_AST_ASYNC_FUNC) {
                const XL6_IrType* future=signature->returnCount==1 ? xl6IrTypeGet(&module->types,signature->returns[0]) : NULL;
                matches=matches && future && future->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR && future->paramCount==1 && future->params;
                if(matches)asyncPayload=future->params[0];
                const XL6_IrType* payload=xl6IrTypeGet(&module->types,asyncPayload);
                matches=matches && (returns->childCount==1 || (!returns->childCount && payload && (payload->cAbi==XL6_IR_C_ABI_VOID || (payload->name && !strcmp(payload->name,"null")))));
            }else matches=matches && returns->childCount==signature->returnCount;
            for(size_t r=0;matches && r<returns->childCount;++r) {
                const XL6_IrNode* returned=xl6IrNodeGet(module,module->children[returns->firstChild+r]);
                XL6_IrTypeId resultType=asyncPayload ? asyncPayload : signature->returns[r];
                matches=returned && (xl6IrLowerTypeContainsTypeParam(module,returned->storageType,0)
                    ? xl6IrLowerUnifyGenericType(module,returned->storageType,resultType,parameters,count,resolved,1)
                    : xl6IrBoundMethodResultMatches(module,returned->storageType,resultType));
                if(matches && returned->storageType!=resultType && !xl6IrLowerTypeContainsTypeParam(module,returned->storageType,0))
                    matches=xl6CallRankAdd(&rank,10,0)!=0;
            }
            for(size_t p=0;matches && p<count;++p)matches=resolved[p]!=0;
            xrtFree(parameters);
        }else if(matches)matches=xl6IrBoundMethodTargetMatches(module,sourceId,targetSymbol,signatureId)!=0;
        if(!matches) {xrtFree(resolved);continue;}
        if(rank<bestRank) {
            xrtFree(selectedArgs);selectedArgs=resolved;selectedCount=count;
            selected=targetSymbol;selectedDeclaration=declarationId;
            bestRank=rank;ambiguous=false;
        }else {
            if(rank==bestRank && selectedDeclaration!=declarationId)ambiguous=true;
            xrtFree(resolved);
        }
    }
    if(ambiguous) {xrtFree(selectedArgs);return xl6IrLowerDiag(ctx,origin,"typed bound method requires one exact overload"),-1;}
    if(!selectedDeclaration) {xrtFree(selectedArgs);return 0;}
    if(selectedCount) {
        if(output && !xl6IrLowerFindGenericSpecialization(module,selectedDeclaration,selectedArgs,selectedCount)) {
            xrtFree(selectedArgs);return xl6IrLowerDiag(ctx,origin,"typed bound generic method was not closed before body lowering"),-1;
        }
        if(!xl6IrLowerBuildGenericSpecialization(ctx,0,selectedDeclaration,selectedArgs,selectedCount,&selected)) {
            xrtFree(selectedArgs);return -1;
        }
        if(output && !xl6IrCallTargetGet(module,selected) &&
           (selected>=module->symbolNodeCapacity || !xl6IrLowerRegisterCallTarget(ctx,module->symbolNodes[selected]))) {
            xrtFree(selectedArgs);return -1;
        }
    }
    xrtFree(selectedArgs);
    if(!output)return 0;
    if(!xl6IrBoundMethodTargetMatches(module,sourceId,selected,signatureId))
        return xl6IrLowerDiag(ctx,origin,"typed bound method specialization changed its exact callable ABI"),-1;
    signature=xl6IrTypeGet(&module->types,signatureId);
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
        closure->boundMethodDispatchSlot=xl6IrBoundMethodDispatchSlot(module,sourceId,selected);
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

/* Result conversion stays in the one typed invocation body. Product fields
 * are detached and protected before any allocation can fail. Unchanged fields
 * transfer their existing owners; only requested ordinary leaves copy bytes. */
static XL6_IrValueId xl6IrLowerBoundMethodResult(XL6_IrLowerContext* ctx,XL6_IrNodeId origin,
    XL6_IrValueId value,XL6_IrTypeId targetId)
{
    XL6_IrModule* module=ctx->module;
    XL6_IrTypeId sourceId=xl6IrLowerValueType(ctx,value);
    if(sourceId==targetId)return value;
    const XL6_IrType* source=xl6IrTypeGet(&module->types,sourceId);
    const XL6_IrType* target=xl6IrTypeGet(&module->types,targetId);
    if(!xl6IrBoundMethodResultMatches(module,sourceId,targetId))return XL6_IR_VALUE_INVALID;
    if(xl6IrTypeIsCOwned(source))return xl6IrLowerCOwnedCopy(ctx,origin,targetId,value);
    size_t count=source->paramCount,first=ctx->evaluationOwnerCount;
    XL6_IrValueId result=0,*fields=xrtCalloc(count,sizeof(*fields));
    XL6_SymbolId* owners=xrtCalloc(count,sizeof(*owners));
    if(!fields || !owners)goto done;
    for(size_t i=0;i<count;++i){
        const XL6_IrType* field=xl6IrTypeGet(&module->types,source->params[i]);
        bool managed=field->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED;
        if(!xl6IrLowerEmit(ctx,managed ? XL6_IR_INSTR_PRODUCT_TAKE : XL6_IR_INSTR_PRODUCT_GET,
            origin,source->params[i],0,&value,1,&fields[i]))goto done;
        module->instructions[module->instructionCount-1].elementIndex=i;
        if(managed){
            fields[i]=xl6IrLowerProtectEvaluationValue(ctx,origin,source->params[i],fields[i]);
            if(!fields[i])goto done;
            owners[i]=ctx->evaluationOwners[ctx->evaluationOwnerCount-1].storage;
        }
    }
    for(size_t i=0;i<count;++i){
        if(source->params[i]==target->params[i])continue;
        size_t owner=first;
        while(owner<ctx->evaluationOwnerCount && ctx->evaluationOwners[owner].storage!=owners[i])++owner;
        if(!owners[i] || owner==ctx->evaluationOwnerCount ||
           !xl6IrLowerCommitEvaluationOwnerRange(ctx,origin,owner,owner+1))goto done;
        fields[i]=xl6IrLowerBoundMethodResult(ctx,origin,fields[i],target->params[i]);
        if(!fields[i])goto done;
        fields[i]=xl6IrLowerProtectEvaluationValue(ctx,origin,target->params[i],fields[i]);
        if(!fields[i])goto done;
    }
    if(!xl6IrLowerCommitEvaluationOwners(ctx,origin,first) ||
       !xl6IrLowerEmit(ctx,XL6_IR_INSTR_PRODUCT_MAKE,origin,targetId,0,fields,count,&result))result=0;
done:
    xrtFree(fields);xrtFree(owners);return result;
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
        for(size_t m=0;m<nodeLimit;++m) {
            XL6_IrNodeId declarationId=(XL6_IrNodeId)m+1;
            if(xl6IrBoundMethodCandidateMatches(module,(XL6_IrNodeId)n,declarationId) &&
               xl6IrLowerIsGenericDeclaration(module,declarationId)) {generic=true;break;}
        }
        if(!generic) continue;
        XL6_IrNodeId parent=source->parent;
        for(size_t t=1;t<=typeLimit;++t) {
            const XL6_IrType* signature=xl6IrTypeGet(&module->types,(XL6_IrTypeId)t);
            if(!signature || signature->kind!=XL6_IR_TYPE_FUNCTION ||
               signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED ||
               xl6IrLowerTypeContainsTypeParam(module,(XL6_IrTypeId)t,0)) continue;
            XL6_IrNodeId origin=parent;
            for(size_t depth=0;origin && depth<module->nodeCount;++depth) {
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
