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

/* Parameter unification stays invariant. Only result positions admit the
 * same recursively typed owner-copy conversion as the final adapter. */
static int xl6IrLowerUnifyBoundResult(const XL6_IrModule* module,XL6_IrTypeId formalId,
    XL6_IrTypeId actualId,const XL6_IrTypeId* parameters,size_t count,XL6_IrTypeId* resolved,size_t depth)
{
    if(xl6IrBoundMethodResultMatches(module,formalId,actualId))return 1;
    /* Reuse invariant input inference before considering result covariance. */
    for(size_t p=0;p<count;++p)
        if(formalId==parameters[p] && resolved[p])
            return xl6IrBoundMethodResultMatches(module,resolved[p],actualId);
    const XL6_IrType* formal=xl6IrTypeGet(&module->types,formalId);
    const XL6_IrType* actual=xl6IrTypeGet(&module->types,actualId);
    if(!formal || !actual || depth>module->types.count)return 0;
    if(formal->kind==XL6_IR_TYPE_FUNCTION && actual->kind==XL6_IR_TYPE_FUNCTION) {
        if(formal->cAbi!=actual->cAbi || formal->callableErasureAbi!=actual->callableErasureAbi ||
           formal->paramCount!=actual->paramCount || formal->returnCount!=actual->returnCount)return 0;
        for(size_t p=0;p<formal->paramCount;++p)
            if(formal->parameterModes[p]!=actual->parameterModes[p] ||
               !xl6IrLowerUnifyGenericType(module,formal->params[p],actual->params[p],parameters,count,resolved,depth+1))return 0;
        for(size_t r=0;r<formal->returnCount;++r)
            if(!xl6IrLowerUnifyBoundResult(module,formal->returns[r],actual->returns[r],parameters,count,resolved,depth+1))return 0;
        return 1;
    }
    if((formal->cAbi==XL6_IR_C_ABI_PRODUCT && actual->cAbi==XL6_IR_C_ABI_PRODUCT) ||
       (formal->kind==XL6_IR_TYPE_NULLABLE && actual->kind==XL6_IR_TYPE_NULLABLE)) {
        if(formal->paramCount!=actual->paramCount)return 0;
        for(size_t p=0;p<formal->paramCount;++p)
            if(!xl6IrLowerUnifyBoundResult(module,formal->params[p],actual->params[p],parameters,count,resolved,depth+1))return 0;
        return 1;
    }
    return xl6IrLowerUnifyGenericType(module,formalId,actualId,parameters,count,resolved,depth);
}

static int xl6IrLowerTryTypedBoundMethod(XL6_IrLowerContext* ctx,XL6_IrNodeId origin,
    XL6_IrNodeId sourceId,XL6_IrTypeId signatureId,XL6_IrValueId* output)
{
    XL6_IrModule* module=ctx->module;
    const XL6_IrNode* source=xl6IrNodeGet(module,sourceId);
    const XL6_IrType* signature=xl6IrTypeGet(&module->types,signatureId);
    if(!source || source->kind!=XL6_IR_NODE_MEMBER || source->sourceKind!=XL6_AST_MEMBER ||
       source->childCount!=1 || !source->text || !signature || signature->kind!=XL6_IR_TYPE_FUNCTION ||
       (signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED &&
        signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC &&
        signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC)) return 0;
    XL6_IrNodeId receiverId=module->children[source->firstChild];
    const XL6_IrNode* receiverNode=xl6IrNodeGet(module,receiverId);
    XL6_IrTypeId ownerId=receiverNode ? xl6IrNodeMaterializedValueType(module,receiverNode) : 0;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,ownerId);
    if(owner && owner->kind==XL6_IR_TYPE_NULLABLE && owner->paramCount==1) {
        ownerId=owner->params[0];owner=xl6IrTypeGet(&module->types,ownerId);
    }
    if(!owner || owner->kind!=XL6_IR_TYPE_CLASS ||
       (owner->cAbi!=XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        !(owner->cAbi==XL6_IR_C_ABI_NATIVE_HANDLE && owner->foreignHeader &&
          owner->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL))) return 0;
    XL6_SymbolId selected=0;
    XL6_IrNodeId selectedDeclaration=0;
    XL6_IrTypeId* selectedArgs=NULL;
    size_t selectedCount=0;
    uint64_t bestRank=UINT64_MAX;
    bool ambiguous=false;
    const XL6_IrNode* chosen=xl6IrSymbolNodeGet(module,source->targetSymbol);
    XL6_IrNodeId chosenId=chosen ? (XL6_IrNodeId)(chosen-module->nodes+1) : 0;
    size_t methodLimit=chosenId ? 1 : 0;
    /* Rank declaration shapes before instantiating a winner. As with a
     * direct call, an inferred parameter costs GENERIC_MATCH, and omitted
     * defaults break ties only after all parameter conversion costs. */
    for(size_t i=0;i<methodLimit;++i) {
        source=xl6IrNodeGet(module,sourceId);
        signature=xl6IrTypeGet(&module->types,signatureId);
        XL6_IrNodeId declarationId=chosenId;
        if(!xl6IrBoundMethodCandidateMatches(module,sourceId,declarationId))continue;
        const XL6_IrNode* declaration=xl6IrNodeGet(module,declarationId);
        XL6_SymbolId targetSymbol=declaration->symbol;
        const XL6_IrNode* params=declaration && declaration->childCount>2 ? xl6IrNodeGet(module,module->children[declaration->firstChild+1]) : NULL;
        const XL6_IrNode* returns=declaration && declaration->childCount>2 ? xl6IrNodeGet(module,module->children[declaration->firstChild+2]) : NULL;
        if(!params || !returns || params->childCount<signature->paramCount ||
           (declaration->flags&XL6_AST_FLAG_STATIC))continue;
        XL6_IrCallTarget user={.parameterCount=params->childCount,
            .variadicAbi=xl6IrLowerCallableTargetVariadicAbi(signature->callableErasureAbi)};
        const XL6_IrNode* tail=params->childCount ? xl6IrNodeGet(module,module->children[params->firstChild+params->childCount-1]) : NULL;
        if((tail ? xl6IrCallVariadicAbiForParameter(module,tail) : XL6_IR_CALL_VARIADIC_NONE)!=user.variadicAbi)continue;
        uint64_t rank=0;
        bool matches=xl6CallRankAdd(&rank,0,(uint32_t)(params->childCount-signature->paramCount));
        for(size_t p=0;matches && p<params->childCount;++p) {
            if(xl6IrLowerNamedAdapterSignatureParameter(signature,&user,p)!=SIZE_MAX)continue;
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
            size_t ownerCount=0;
            matches=xl6IrLowerSeedGenericOwner(module,declarationId,ownerId,parameters,count,resolved,&ownerCount)!=0;
            for(size_t p=0;matches && p<signature->paramCount;++p) {
                size_t mapped=xl6IrLowerNamedAdapterTargetParameter(signature,&user,p);
                const XL6_IrNode* parameter=mapped<params->childCount ? xl6IrNodeGet(module,module->children[params->firstChild+mapped]) : NULL;
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
                    ? xl6IrLowerUnifyBoundResult(module,returned->storageType,resultType,parameters,count,resolved,1)
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
            .storageSymbol=xl6IrLowerNewSyntheticSymbol(ctx),.type=receiverType,
            .abi=xl6IrBoundMethodReceiverCaptureAbi(module,target)};
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

/* A changed returned callable owns one retained source. It uses the existing
 * closure environment and bool/out invocation ABI, not a dynamic projection. */
static XL6_IrValueId xl6IrLowerBoundMethodResult(XL6_IrLowerContext*,XL6_IrNodeId,XL6_IrValueId,XL6_IrTypeId);
static XL6_IrValueId xl6IrLowerCallableResult(XL6_IrLowerContext* ctx,XL6_IrNodeId origin,
    XL6_IrValueId value,XL6_IrTypeId targetId)
{
    XL6_IrModule* module=ctx->module;
    XL6_IrTypeId sourceId=xl6IrLowerValueType(ctx,value);
    const XL6_IrType* target=xl6IrTypeGet(&module->types,targetId);
    XL6_IrClosureTarget* closure=NULL;
    for(size_t c=0;c<module->closureTargetCount;++c) {
        XL6_IrClosureTarget* item=&module->closureTargets[c];
        if(item->kind==XL6_IR_CLOSURE_TARGET_RESULT_ADAPTER && item->signatureType==targetId &&
           module->closureCaptures[item->firstCapture].type==sourceId){closure=item;break;}
    }
    if(!closure) {
        if(!xl6IrLowerGrow((void**)&module->nodes,&module->nodeCapacity,module->nodeCount,sizeof(*module->nodes)) ||
           !xl6IrLowerGrow((void**)&module->closureTargets,&module->closureTargetCapacity,module->closureTargetCount,sizeof(*module->closureTargets)) ||
           !xl6IrLowerGrow((void**)&module->closureCaptures,&module->closureCaptureCapacity,module->closureCaptureCount,sizeof(*module->closureCaptures)))return 0;
        XL6_IrNode* declaration=&module->nodes[module->nodeCount++];memset(declaration,0,sizeof(*declaration));
        declaration->kind=XL6_IR_NODE_LIST;declaration->sourceKind=XL6_AST_LIST;declaration->storageType=targetId;declaration->unionField=SIZE_MAX;
        if(!xl6IrLowerAppendSpecializationChild(module,module->root,(XL6_IrNodeId)module->nodeCount))return 0;
        closure=&module->closureTargets[module->closureTargetCount++];memset(closure,0,sizeof(*closure));
        closure->kind=XL6_IR_CLOSURE_TARGET_RESULT_ADAPTER;closure->declaration=(XL6_IrNodeId)module->nodeCount;
        closure->targetSymbol=xl6IrLowerNewSyntheticSymbol(ctx);closure->signatureType=targetId;
        closure->rawCallbackAbi=XL6_IR_CLOSURE_RAW_CALLBACK_NONE;
        closure->firstCapture=module->closureCaptureCount;closure->captureCount=1;closure->forwardCallMetadata=1;
        XL6_IrClosureCapture* capture=&module->closureCaptures[module->closureCaptureCount++];
        *capture=(XL6_IrClosureCapture){.sourceSymbol=xl6IrLowerNewSyntheticSymbol(ctx),
            .storageSymbol=xl6IrLowerNewSyntheticSymbol(ctx),.type=sourceId,.abi=XL6_IR_CLOSURE_CAPTURE_RETAIN_CLOSURE};
        closure->callParameterCount=target->paramCount;
        if(target->paramCount) {
            closure->callParameterNames=xrtCalloc(target->paramCount,sizeof(char*));
            closure->callParameterDefaultAbis=xrtCalloc(target->paramCount,sizeof(*closure->callParameterDefaultAbis));
            closure->callParameterDefaultLiterals=xrtCalloc(target->paramCount,sizeof(char*));
            closure->callParameterDefaultFunctions=xrtCalloc(target->paramCount,sizeof(uint32));
            if(!closure->callParameterNames || !closure->callParameterDefaultAbis ||
               !closure->callParameterDefaultLiterals || !closure->callParameterDefaultFunctions)return 0;
            for(size_t p=0;p<target->paramCount;++p) {
                closure->callParameterNames[p]=xrtStrDup("argument");
                if(!closure->callParameterNames[p])return 0;
                closure->callParameterDefaultAbis[p]=XL6_IR_DYNAMIC_METHOD_DEFAULT_NONE;
            }
        }
    }
    XL6_SymbolId symbol=closure->targetSymbol;
    size_t first=ctx->evaluationOwnerCount;
    value=xl6IrLowerProtectOwnedBorrow(ctx,origin,value);
    XL6_IrValueId result=0;
    if(!value || !xl6IrLowerEmit(ctx,XL6_IR_INSTR_CLOSURE_MAKE,origin,targetId,symbol,&value,1,&result) ||
       !xl6IrLowerEmitRuntimeErrorEdge(ctx,origin,xl6IrNodeGet(module,origin),result) ||
       !xl6IrLowerCompleteEvaluationBorrows(ctx,origin,first,&result))return 0;
    return result;
}

static XL6_IrValueId xl6IrLowerNullableCallableResult(XL6_IrLowerContext* ctx,XL6_IrNodeId origin,
    XL6_IrValueId value,XL6_IrTypeId targetId)
{
    XL6_IrModule* module=ctx->module;
    const XL6_IrType* source=xl6IrTypeGet(&module->types,xl6IrLowerValueType(ctx,value));
    const XL6_IrType* target=xl6IrTypeGet(&module->types,targetId);
    XL6_IrTypeId inner=source->params[0],output=target->params[0];
    size_t first=ctx->evaluationOwnerCount;
    value=xl6IrLowerProtectOwnedBorrow(ctx,origin,value);
    XL6_SymbolId storage=xl6IrLowerNewTempStorage(ctx,targetId,origin);
    XL6_IrValueId test,payload,result;
    XL6_IrBlockId present,absent,merge;
    XL6_IrTypeId boolean=xl6IrLowerFindTypeByCAbi(module,XL6_IR_C_ABI_BOOL);
    if(!value || !storage || !xl6IrLowerEmit(ctx,XL6_IR_INSTR_NULL_TEST,origin,boolean,0,&value,1,&test) ||
       !xl6IrLowerNewBlock(ctx,&present) || !xl6IrLowerNewBlock(ctx,&absent) || !xl6IrLowerNewBlock(ctx,&merge) ||
       !xl6IrLowerEmit(ctx,XL6_IR_INSTR_BRANCH,origin,0,0,&test,1,NULL))return 0;
    module->instructions[module->instructionCount-1].trueTarget=absent;
    module->instructions[module->instructionCount-1].falseTarget=present;
    xl6IrLowerEnterBlock(ctx,present);
    if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_NULLABLE_UNWRAP,origin,inner,0,&value,1,&payload))return 0;
    result=xl6IrLowerBoundMethodResult(ctx,origin,payload,output);
    if(!result || !xl6IrLowerEmit(ctx,XL6_IR_INSTR_NULLABLE_WRAP,origin,targetId,0,&result,1,&result))return 0;
    if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_STORE,origin,targetId,storage,&result,1,NULL) ||
       !xl6IrLowerEmitJump(ctx,origin,merge))return 0;
    xl6IrLowerEnterBlock(ctx,absent);
    if(!xl6IrLowerEmit(ctx,XL6_IR_INSTR_NULL,origin,targetId,0,NULL,0,&result) ||
       !xl6IrLowerEmit(ctx,XL6_IR_INSTR_STORE,origin,targetId,storage,&result,1,NULL) ||
       !xl6IrLowerEmitJump(ctx,origin,merge))return 0;
    xl6IrLowerEnterBlock(ctx,merge);
    result=xl6IrLowerReadJoinStorage(ctx,origin,targetId,storage);
    return xl6IrLowerCompleteEvaluationBorrows(ctx,origin,first,&result) ? result : 0;
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
    if(source->kind==XL6_IR_TYPE_FUNCTION)return xl6IrLowerCallableResult(ctx,origin,value,targetId);
    if(source->kind==XL6_IR_TYPE_NULLABLE)return xl6IrLowerNullableCallableResult(ctx,origin,value,targetId);
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
        const XL6_IrNode* chosen=xl6IrSymbolNodeGet(module,source->targetSymbol);
        XL6_IrNodeId declarationId=chosen ? (XL6_IrNodeId)(chosen-module->nodes+1) : 0;
        generic=declarationId && xl6IrBoundMethodCandidateMatches(module,(XL6_IrNodeId)n,declarationId) &&
            xl6IrLowerIsGenericDeclaration(module,declarationId);
        if(!generic) continue;
        XL6_IrNodeId parent=source->parent;
        for(size_t t=1;t<=typeLimit;++t) {
            const XL6_IrType* signature=xl6IrTypeGet(&module->types,(XL6_IrTypeId)t);
            if(!signature || signature->kind!=XL6_IR_TYPE_FUNCTION ||
               (signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED &&
                signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC &&
                signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC) ||
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
