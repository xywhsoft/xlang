/* Included by xl6_ir.c after the shared runtime-callable admission rules. */
static XL6_IrCallableBridgeIntegerDomain xl6IrCallableBridgeIntegerDomain(
    const XL6_IrType* type)
{
    XL6_IrCallableBridgeIntegerDomain domain = {0};
    if (type && (type->cAbi == XL6_IR_C_ABI_I64 || type->cAbi == XL6_IR_C_ABI_U64)) {
        domain.rule = xl6IrIntegerAdmissionAbi(type->integerTarget);
        if (domain.rule != XL6_VALUE_ADMIT_NONE) {
            domain.identity = type->typeValueId;
            domain.nominal = type->kind == XL6_IR_TYPE_NUMERIC_ENUM;
        }
    }
    return domain;
}

static int xl6IrCallableBridgeIntegerDomainEqual(
    XL6_IrCallableBridgeIntegerDomain a, XL6_IrCallableBridgeIntegerDomain b)
{
    return a.rule == b.rule && a.identity == b.identity && a.nominal == b.nominal;
}

static int xl6IrCallableBridgeParameterPlan(
    const XL6_IrModule* module, const XL6_IrType* signature,
    size_t parameter, XL6_IrCallableBridgeParameter* plan)
{
    const XL6_IrType* type;
    XL6_IrParameterAbi abi;
    int variadic;
    if (!signature || !plan || parameter >= signature->paramCount ||
        !signature->params || !signature->parameterAbis ||
        !(type = xl6IrTypeGet(&module->types, signature->params[parameter]))) return 0;
    memset(plan, 0, sizeof(*plan));
    plan->parameter = parameter; plan->type = signature->params[parameter];
    plan->valueType=xl6IrCallableBridgePayloadType(&module->types,plan->type,&plan->presenceAbi);
    type=xl6IrTypeGet(&module->types,plan->valueType); if(!type)return 0;
    plan->integerDomain = xl6IrCallableBridgeIntegerDomain(type);
    plan->nativeScalarProfile = xl6IrNativeScalarProfileForCAbi(type->cAbi);
    abi = signature->parameterAbis[parameter];
    plan->invokeAbi = abi == XL6_IR_PARAMETER_ABI_OWNED ? XL6_IR_CALLBACK_INVOKE_TAKE
        : abi == XL6_IR_PARAMETER_ABI_BORROW ? XL6_IR_CALLBACK_INVOKE_BORROW
        : abi == XL6_IR_PARAMETER_ABI_VALUE ? XL6_IR_CALLBACK_INVOKE_VALUE
        : XL6_IR_CALLBACK_INVOKE_INVALID;
    plan->decodedDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE;
    plan->inputDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE;
    variadic = signature->callableErasureAbi != XL6_IR_CALLABLE_ERASURE_FIXED &&
        parameter + 1 == signature->paramCount;
    if (variadic) {
        if (abi != XL6_IR_PARAMETER_ABI_BORROW) return 0;
        if (signature->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC &&
            ((type->kind != XL6_IR_TYPE_BUILTIN && type->kind != XL6_IR_TYPE_GENERIC_INSTANCE) ||
             type->collectionAbi != XL6_IR_COLLECTION_ARRAY ||
             type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR || type->paramCount != 0 ||
             !type->typeValueId || type->nominalSymbol != XL6_SYMBOL_ID_INVALID ||
             !module->runtimeTypeMap.fallbackTypeValueId ||
             !xl6IrFindDynamicXValueBuiltinType(module))) return 0;
        plan->decodeAbi = signature->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC
            ? XL6_IR_CALLABLE_BRIDGE_ARRAY_PACK : XL6_IR_CALLABLE_BRIDGE_DICT_PACK;
        plan->encodeAbi = XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN;
        plan->decodedDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE;
        return 1;
    }
    if (plan->nativeScalarProfile != XL6_NATIVE_SCALAR_NONE) {
        plan->decodeAbi = plan->encodeAbi = XL6_IR_CALLABLE_BRIDGE_NATIVE_SCALAR;
        return plan->invokeAbi == XL6_IR_CALLBACK_INVOKE_VALUE;
    }
    switch (type->cAbi) {
    case XL6_IR_C_ABI_BOOL: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_BOOL; break;
    case XL6_IR_C_ABI_CHAR_U32: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_CHAR; break;
    case XL6_IR_C_ABI_I64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_I64; break;
    case XL6_IR_C_ABI_U64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_U64; break;
    case XL6_IR_C_ABI_F64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_F64; break;
    case XL6_IR_C_ABI_F32: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_F32; break;
    case XL6_IR_C_ABI_TIME_I64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_TIME; break;
    case XL6_IR_C_ABI_RAW_POINTER: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_POINTER; break;
    case XL6_IR_C_ABI_MANAGED_STRING:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_STRING_COPY;
        plan->decodedDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING;
        break;
    case XL6_IR_C_ABI_MANAGED_BYTES:
        plan->decodeAbi = abi == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN
            : XL6_IR_CALLABLE_BRIDGE_BYTES_BORROW;
        plan->encodeAbi = abi == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_CALLABLE_BRIDGE_BYTES_MOVE
            : XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN;
        if (abi == XL6_IR_PARAMETER_ABI_OWNED)
            plan->decodedDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE;
        break;
    case XL6_IR_C_ABI_RUNTIME_ERROR_PTR:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_ERROR_REF;
        plan->decodedDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR;
        break;
    case XL6_IR_C_ABI_CLOSURE_PTR:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY;
        plan->encodeAbi = XL6_IR_CALLABLE_BRIDGE_CLOSURE_REF;
        plan->decodedDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE;
        break;
    case XL6_IR_C_ABI_RUNTIME_FUTURE_PTR:
        plan->decodeAbi=XL6_IR_CALLABLE_BRIDGE_FUTURE_COPY;
        plan->encodeAbi=XL6_IR_CALLABLE_BRIDGE_FUTURE_REF;
        plan->decodedDestroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE;
        break;
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR:
        plan->decodeAbi = abi == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN : XL6_IR_CALLABLE_BRIDGE_XVALUE_BORROW;
        plan->encodeAbi = abi == XL6_IR_PARAMETER_ABI_OWNED
            ? XL6_IR_CALLABLE_BRIDGE_XVALUE_MOVE : XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN;
        if (abi == XL6_IR_PARAMETER_ABI_OWNED)
            plan->decodedDestroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE;
        break;
    default: return 0;
    }
    if (plan->encodeAbi == XL6_IR_CALLABLE_BRIDGE_INVALID) plan->encodeAbi = plan->decodeAbi;
    if (abi == XL6_IR_PARAMETER_ABI_OWNED) plan->inputDestroyAbi = plan->decodedDestroyAbi;
    return plan->invokeAbi != XL6_IR_CALLBACK_INVOKE_INVALID;
}

static int xl6IrCallableBridgeResultPlan(
    const XL6_IrModule* module, const XL6_IrType* signature,
    size_t result, XL6_IrCallableBridgeResult* plan)
{
    const XL6_IrType* type;
    if (!signature || !plan || result >= signature->returnCount || !signature->returns ||
        !(type = xl6IrTypeGet(&module->types, signature->returns[result]))) return 0;
    memset(plan, 0, sizeof(*plan));
    plan->result = result; plan->type = signature->returns[result];
    plan->valueType=xl6IrCallableBridgePayloadType(&module->types,plan->type,&plan->presenceAbi);
    type=xl6IrTypeGet(&module->types,plan->valueType); if(!type)return 0;
    plan->integerDomain = xl6IrCallableBridgeIntegerDomain(type);
    plan->nativeScalarProfile = xl6IrNativeScalarProfileForCAbi(type->cAbi);
    plan->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_NONE;
    if (plan->nativeScalarProfile != XL6_NATIVE_SCALAR_NONE) {
        plan->decodeAbi = plan->encodeAbi = XL6_IR_CALLABLE_BRIDGE_NATIVE_SCALAR;
        return 1;
    }
    switch (type->cAbi) {
    case XL6_IR_C_ABI_BOOL: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_BOOL; break;
    case XL6_IR_C_ABI_CHAR_U32: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_CHAR; break;
    case XL6_IR_C_ABI_I64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_I64; break;
    case XL6_IR_C_ABI_U64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_U64; break;
    case XL6_IR_C_ABI_F64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_F64; break;
    case XL6_IR_C_ABI_F32: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_F32; break;
    case XL6_IR_C_ABI_TIME_I64: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_TIME; break;
    case XL6_IR_C_ABI_RAW_POINTER: plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_POINTER; break;
    case XL6_IR_C_ABI_MANAGED_STRING:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_STRING_COPY;
        plan->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_FREE_STRING; break;
    case XL6_IR_C_ABI_MANAGED_BYTES:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_BYTES_RETAIN;
        plan->encodeAbi = XL6_IR_CALLABLE_BRIDGE_BYTES_MOVE;
        plan->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE; break;
    case XL6_IR_C_ABI_RUNTIME_ERROR_PTR:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_ERROR_REF;
        plan->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_FREE_ERROR; break;
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_XVALUE_RETAIN;
        plan->encodeAbi = XL6_IR_CALLABLE_BRIDGE_XVALUE_MOVE;
        plan->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_XVALUE; break;
    case XL6_IR_C_ABI_CLOSURE_PTR:
        plan->decodeAbi = XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY;
        plan->encodeAbi = XL6_IR_CALLABLE_BRIDGE_CLOSURE_REF;
        plan->destroyAbi = XL6_IR_ASYNC_FRAME_DESTROY_RELEASE_CLOSURE; break;
    case XL6_IR_C_ABI_RUNTIME_FUTURE_PTR:
        plan->decodeAbi=XL6_IR_CALLABLE_BRIDGE_FUTURE_COPY;
        plan->encodeAbi=XL6_IR_CALLABLE_BRIDGE_FUTURE_REF;
        plan->destroyAbi=XL6_IR_ASYNC_FRAME_DESTROY_DESTROY_FUTURE; break;
    default: return 0;
    }
    if (plan->encodeAbi == XL6_IR_CALLABLE_BRIDGE_INVALID) plan->encodeAbi = plan->decodeAbi;
    return 1;
}

const XL6_IrCallableBridge* xl6IrCallableBridgeGet(
    const XL6_IrModule* module, XL6_IrTypeId signatureType)
{
    size_t i;
    if (!module || module->stage != XL6_IR_STAGE_LOWERED) return NULL;
    for (i = 0; i < module->callableBridgeCount; ++i)
        if (module->callableBridges[i].signatureType == signatureType) return &module->callableBridges[i];
    return NULL;
}

/* Dynamic methods project scalar inputs and publish scalar result fields with
 * the same exact codecs as function bridges. Source spellings never select a
 * conversion; tagged presence and native platform profiles remain IR facts. */
int xl6IrDynamicMethodScalarPlanForType(const XL6_IrModule* module,
    XL6_IrTypeId id,XL6_IrCallableBridgeResult* output)
{
    XL6_IrCallableBridgeResult plan;
    XL6_IrType signature={0}; signature.returnCount=1; signature.returns=&id;
    if (!module || !xl6IrCallableBridgeResultPlan(module,&signature,0,&plan)) return 0;
    const XL6_IrType* source=xl6IrTypeGet(&module->types,id);
    const XL6_IrType* value=xl6IrTypeGet(&module->types,plan.valueType);
    if (!source || !value || !value->typeValueId ||
        (value->kind!=XL6_IR_TYPE_BUILTIN && value->kind!=XL6_IR_TYPE_NUMERIC_ENUM) ||
        source->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
        value->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
        (!xl6IrRuntimeCallableScalarAbi(value->cAbi) && value->cAbi!=XL6_IR_C_ABI_RAW_POINTER) ||
        plan.destroyAbi!=XL6_IR_ASYNC_FRAME_DESTROY_NONE) return 0;
    if (output) *output=plan;
    return 1;
}
const XL6_IrCallableBridgeResult* xl6IrDynamicMethodScalarCodecGet(
    const XL6_IrModule* module,size_t index)
{
    return module && module->stage==XL6_IR_STAGE_LOWERED && index &&
        index<=module->dynamicMethodScalarCodecCount && module->dynamicMethodScalarCodecs &&
        module->dynamicMethodScalarCodecs[index-1].decodeAbi!=XL6_IR_CALLABLE_BRIDGE_INVALID
        ? &module->dynamicMethodScalarCodecs[index-1] : NULL;
}
static int xl6IrAppendDynamicMethodScalarCodec(XL6_IrModule* module,
    const XL6_IrCallableBridgeResult* plan)
{
    if (!xl6IrGrow((void**)&module->dynamicMethodScalarCodecs,
        &module->dynamicMethodScalarCodecCapacity,module->dynamicMethodScalarCodecCount,
        sizeof(*module->dynamicMethodScalarCodecs))) return 0;
    module->dynamicMethodScalarCodecs[module->dynamicMethodScalarCodecCount++]=*plan;
    return 1;
}
int xl6IrBuildDynamicMethodScalarCodecs(XL6_IrModule* module,XL6_DiagList* diags)
{
    xrtFree(module->dynamicMethodScalarCodecs); module->dynamicMethodScalarCodecs=NULL;
    module->dynamicMethodScalarCodecCount=module->dynamicMethodScalarCodecCapacity=0;
    for(size_t p=0;p<module->dynamicMethodParameterCount;++p)
        module->dynamicMethodParameters[p].scalarCodec=0;
    for(size_t m=0;m<module->dynamicMethodCount;++m) {
        XL6_IrDynamicMethod* method=&module->dynamicMethods[m];
        method->firstScalarResultCodec=method->scalarResultCodecCount=0;
        if(method->adapterAbi!=XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED) continue;
        for(size_t p=0;p<method->parameterCount;++p) {
            XL6_IrDynamicMethodParameter* item=&module->dynamicMethodParameters[method->firstParameter+p];
            XL6_IrCallableBridgeResult plan;
            if(item->matchAbi!=XL6_IR_DYNAMIC_METHOD_MATCH_SCALAR_CODEC)continue;
            if(!xl6IrDynamicMethodScalarPlanForType(module,item->type,&plan) ||
                !xl6IrAppendDynamicMethodScalarCodec(module,&plan)) goto failed;
            item->scalarCodec=module->dynamicMethodScalarCodecCount;
        }
        method->firstScalarResultCodec=module->dynamicMethodScalarCodecCount;
        const XL6_IrType* product=xl6IrTypeGet(&module->types,method->returnType);
        if(method->returnCount>1 && (!product || product->cAbi!=XL6_IR_C_ABI_PRODUCT ||
            !product->params || product->paramCount!=method->returnCount))goto failed;
        for(size_t r=0;r<method->returnCount;++r) {
            XL6_IrTypeId id=method->returnCount==1 ? method->returnType : product->params[r];
            XL6_IrCallableBridgeResult plan={0};
            (void)xl6IrDynamicMethodScalarPlanForType(module,id,&plan);
            plan.result=r; plan.type=id;
            if(!xl6IrAppendDynamicMethodScalarCodec(module,&plan)) goto failed;
            ++method->scalarResultCodecCount;
        }
    }
    return 1;
failed:
    return xl6IrDiag(diags,NULL,"XLIR dynamic scalar codecs could not be materialized");
}
static int xl6IrDynamicScalarCodecEqual(const XL6_IrCallableBridgeResult* a,
    const XL6_IrCallableBridgeResult* b)
{
    return a->result==b->result && a->type==b->type && a->valueType==b->valueType &&
        a->presenceAbi==b->presenceAbi && a->decodeAbi==b->decodeAbi && a->encodeAbi==b->encodeAbi &&
        xl6IrCallableBridgeIntegerDomainEqual(a->integerDomain,b->integerDomain) &&
        a->nativeScalarProfile==b->nativeScalarProfile && a->destroyAbi==b->destroyAbi;
}
int xl6IrVerifyDynamicMethodScalarCodecs(const XL6_IrModule* module,XL6_DiagList* diags)
{
    size_t next=0;
    if(module->dynamicMethodScalarCodecCount && !module->dynamicMethodScalarCodecs)goto malformed;
    for(size_t m=0;m<module->dynamicMethodCount;++m) {
        const XL6_IrDynamicMethod* method=&module->dynamicMethods[m];
        int linked=module->stage==XL6_IR_STAGE_LOWERED && method->adapterAbi==XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED;
        size_t groupCount=0;
        if(linked && method->kind==XL6_IR_DYNAMIC_CALL_METHOD)
            for(size_t other=0;other<module->dynamicMethodCount;++other) {
                const XL6_IrDynamicMethod* candidate=&module->dynamicMethods[other];
                if(candidate->kind==XL6_IR_DYNAMIC_CALL_METHOD && candidate->runtimeTypeTag==method->runtimeTypeTag &&
                    candidate->name && method->name && !strcmp(candidate->name,method->name))++groupCount;
            }
        if(groupCount==1) {
            const XL6_IrType* signature=xl6IrTypeGet(&module->types,method->boundSignatureType);
            const XL6_IrNode* declaration=xl6IrNodeGet(module,method->declaration);
            const XL6_IrNode* parameters=declaration && declaration->childCount>2 ?
                xl6IrNodeGet(module,module->children[declaration->firstChild+1]):NULL;
            const XL6_IrNode* returns=declaration && declaration->childCount>2 ?
                xl6IrNodeGet(module,module->children[declaration->firstChild+2]):NULL;
            XL6_IrCallableErasureAbi erasure=method->variadicAbi==XL6_IR_CALL_VARIADIC_ARRAY ? XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC :
                method->variadicAbi==XL6_IR_CALL_VARIADIC_DICT ? XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC : XL6_IR_CALLABLE_ERASURE_FIXED;
            if(!signature || !signature->typeValueId || signature->paramCount!=method->parameterCount ||
                !parameters || !returns || !xl6IrNamedCallableSignatureMatches(module,method->boundSignatureType,parameters,returns,erasure))goto malformed;
        } else if(method->boundSignatureType)goto malformed;
        for(size_t p=0;p<method->parameterCount;++p) {
            const XL6_IrDynamicMethodParameter* item=&module->dynamicMethodParameters[method->firstParameter+p];
            if(linked && item->matchAbi==XL6_IR_DYNAMIC_METHOD_MATCH_SCALAR_CODEC) {
                XL6_IrCallableBridgeResult expected;
                if(item->scalarCodec!=next+1 || next>=module->dynamicMethodScalarCodecCount ||
                    !xl6IrDynamicMethodScalarPlanForType(module,item->type,&expected) ||
                    !xl6IrDynamicScalarCodecEqual(&module->dynamicMethodScalarCodecs[next++],&expected))goto malformed;
            } else if(item->scalarCodec)goto malformed;
        }
        if(!linked) { if(method->firstScalarResultCodec || method->scalarResultCodecCount)goto malformed; continue; }
        if(method->firstScalarResultCodec!=next || method->scalarResultCodecCount!=method->returnCount)goto malformed;
        const XL6_IrType* product=xl6IrTypeGet(&module->types,method->returnType);
        for(size_t r=0;r<method->returnCount;++r) {
            if(method->returnCount>1 && (!product || product->cAbi!=XL6_IR_C_ABI_PRODUCT ||
                !product->params || r>=product->paramCount))goto malformed;
            XL6_IrTypeId id=method->returnCount==1 ? method->returnType : product->params[r];
            XL6_IrCallableBridgeResult expected={0};
            (void)xl6IrDynamicMethodScalarPlanForType(module,id,&expected);
            expected.result=r; expected.type=id;
            if(next>=module->dynamicMethodScalarCodecCount ||
                !xl6IrDynamicScalarCodecEqual(&module->dynamicMethodScalarCodecs[next++],&expected))goto malformed;
        }
    }
    if(next==module->dynamicMethodScalarCodecCount)return 1;
malformed:
    return xl6IrDiag(diags,NULL,"XLIR dynamic scalar projection, presence or result codec is malformed");
}

const XL6_IrCallableBridgeParameter* xl6IrCallableBridgeParameterGet(
    const XL6_IrModule* module, XL6_IrTypeId signatureType, size_t parameter)
{
    const XL6_IrCallableBridge* bridge = xl6IrCallableBridgeGet(module, signatureType);
    if (!bridge || parameter >= bridge->parameterCount ||
        bridge->firstParameter > module->callableBridgeParameterCount ||
        bridge->parameterCount > module->callableBridgeParameterCount - bridge->firstParameter) return NULL;
    return &module->callableBridgeParameters[bridge->firstParameter + parameter];
}

static XL6_IrCollectionCarrierAbi xl6IrCallableVariadicCarrierAbi(const XL6_IrType* signature)
{
    return signature->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC
        ? XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1
        : signature->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC
        ? XL6_IR_COLLECTION_CARRIER_VALUE_V1 : XL6_IR_COLLECTION_CARRIER_NONE;
}

static XL6_IrCallableFrameExpansionAbi xl6IrCallableFrameExpansionAbi(const XL6_IrType* signature)
{
    return signature->callableErasureAbi==XL6_IR_CALLABLE_ERASURE_ARRAY_VARIADIC
        ? XL6_IR_CALLABLE_EXPAND_ARRAY_SNAPSHOT_V1
        : signature->callableErasureAbi==XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC
        ? XL6_IR_CALLABLE_EXPAND_DICT_SNAPSHOT_V1 : XL6_IR_CALLABLE_EXPAND_NONE;
}

int xl6IrBuildCallableBridges(XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t i, p;
    if (!module || module->stage != XL6_IR_STAGE_LOWERED) return 0;
    module->callableBridgeCount = module->callableBridgeParameterCount = module->callableBridgeResultCount = 0;
    for (i = 0; i < module->types.count; ++i) {
        XL6_IrTypeId id = (XL6_IrTypeId)(i + 1);
        const XL6_IrType* signature = &module->types.items[i];
        XL6_IrCallableBridge* bridge;
        if (!xl6IrRuntimeCallableClosureToXValueSignatureSupported(module, id)) continue;
        if (!xl6IrGrow((void**)&module->callableBridges, &module->callableBridgeCapacity,
                module->callableBridgeCount, sizeof(*module->callableBridges))) goto failed;
        bridge = &module->callableBridges[module->callableBridgeCount++];
        memset(bridge, 0, sizeof(*bridge));
        bridge->signatureType = id; bridge->firstParameter = module->callableBridgeParameterCount;
        bridge->parameterCount = signature->paramCount; bridge->erasureAbi = signature->callableErasureAbi;
        bridge->variadicCarrierAbi = xl6IrCallableVariadicCarrierAbi(signature);
        bridge->variadicElementIdentity = bridge->variadicCarrierAbi != XL6_IR_COLLECTION_CARRIER_NONE
            ? module->runtimeTypeMap.fallbackTypeValueId : 0;
        bridge->fixedParameterCount = signature->paramCount -
            (signature->callableErasureAbi != XL6_IR_CALLABLE_ERASURE_FIXED ? 1 : 0);
        bridge->firstResult = module->callableBridgeResultCount;
        bridge->resultCount = signature->returnCount;
        bridge->resultType = signature->callableResultType;
        bridge->resultCommitAbi = XL6_IR_CALLABLE_RESULT_COMMIT_AFTER_CLEANUP;
        bridge->frameCleanupAbi = XL6_IR_CALLABLE_FRAME_CLEANUP_REPLACE_REVERSE;
        bridge->reverseFrameExpansionAbi=xl6IrCallableFrameExpansionAbi(signature);
        bridge->forwardErrorBoundaryAbi = XL6_IR_CALLABLE_ERROR_BOUNDARY_NATIVE_CALLBACK;
        bridge->argumentBindingAbi = XL6_IR_CALLABLE_BIND_PROVIDED_THEN_DEFAULTS;
        bridge->reverseErrorBoundaryAbi = XL6_IR_CALLABLE_ERROR_BOUNDARY_ISOLATED_STATUS;
        bridge->cleanupAbi = XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL;
        bridge->errorAbi = XL6_IR_CALLBACK_ERROR_PRESERVE;
        bridge->reverseSupported = xl6IrRuntimeCallableFieldSignatureSupported(module, id);
        for (p = 0; p < signature->paramCount; ++p) {
            XL6_IrCallableBridgeParameter plan;
            if (!xl6IrCallableBridgeParameterPlan(module, signature, p, &plan) ||
                !xl6IrGrow((void**)&module->callableBridgeParameters, &module->callableBridgeParameterCapacity,
                    module->callableBridgeParameterCount, sizeof(*module->callableBridgeParameters))) goto failed;
            module->callableBridgeParameters[module->callableBridgeParameterCount++] = plan;
        }
        for (p = 0; p < signature->returnCount; ++p) {
            XL6_IrCallableBridgeResult plan;
            if (!xl6IrCallableBridgeResultPlan(module, signature, p, &plan) ||
                !xl6IrGrow((void**)&module->callableBridgeResults, &module->callableBridgeResultCapacity,
                    module->callableBridgeResultCount, sizeof(*module->callableBridgeResults))) goto failed;
            module->callableBridgeResults[module->callableBridgeResultCount++] = plan;
        }
    }
    return 1;
failed:
    return xl6IrDiag(diags, NULL, "XLIR callable bridge parameter planning failed");
}

/* Recompute the complete canonical partition, including absent records and
 * unused signature types. Cgen may not synthesize missing ownership plans. */
static int xl6IrVerifyCallableBridges(const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t i, b = 0, next = 0, nextResult = 0, p;
    if (module->callableBridgeCount > module->callableBridgeCapacity ||
        (module->callableBridgeCount && !module->callableBridges) ||
        module->callableBridgeParameterCount > module->callableBridgeParameterCapacity ||
        (module->callableBridgeParameterCount && !module->callableBridgeParameters) ||
        module->callableBridgeResultCount > module->callableBridgeResultCapacity ||
        (module->callableBridgeResultCount && !module->callableBridgeResults)) goto malformed;
    if (module->stage != XL6_IR_STAGE_LOWERED)
        return (!module->callableBridgeCount && !module->callableBridgeParameterCount && !module->callableBridgeResultCount)
            ? 1 : xl6IrDiag(diags, NULL, "XLIR typed module contains callable bridge plans");
    for (i = 0; i < module->types.count; ++i) {
        XL6_IrTypeId id = (XL6_IrTypeId)(i + 1);
        const XL6_IrType* signature = &module->types.items[i];
        const XL6_IrCallableBridge* bridge;
        if (!xl6IrRuntimeCallableClosureToXValueSignatureSupported(module, id)) continue;
        if (b >= module->callableBridgeCount) goto malformed;
        bridge = &module->callableBridges[b++];
        if (bridge->signatureType != id || bridge->firstParameter != next ||
            bridge->parameterCount != signature->paramCount ||
            bridge->fixedParameterCount != signature->paramCount -
                (signature->callableErasureAbi != XL6_IR_CALLABLE_ERASURE_FIXED ? 1 : 0) ||
            bridge->firstResult != nextResult || bridge->resultCount != signature->returnCount ||
            bridge->resultType != signature->callableResultType ||
            bridge->resultCommitAbi != XL6_IR_CALLABLE_RESULT_COMMIT_AFTER_CLEANUP ||
            bridge->frameCleanupAbi != XL6_IR_CALLABLE_FRAME_CLEANUP_REPLACE_REVERSE ||
            bridge->forwardErrorBoundaryAbi != XL6_IR_CALLABLE_ERROR_BOUNDARY_NATIVE_CALLBACK ||
            bridge->argumentBindingAbi != XL6_IR_CALLABLE_BIND_PROVIDED_THEN_DEFAULTS ||
            bridge->reverseFrameExpansionAbi != xl6IrCallableFrameExpansionAbi(signature) ||
            bridge->reverseErrorBoundaryAbi != XL6_IR_CALLABLE_ERROR_BOUNDARY_ISOLATED_STATUS ||
            bridge->erasureAbi != signature->callableErasureAbi ||
            bridge->variadicCarrierAbi != xl6IrCallableVariadicCarrierAbi(signature) ||
            bridge->variadicElementIdentity != (signature->callableErasureAbi != XL6_IR_CALLABLE_ERASURE_FIXED
                ? module->runtimeTypeMap.fallbackTypeValueId : 0) ||
            bridge->cleanupAbi != XL6_IR_CALLBACK_CLEANUP_TRANSACTIONAL ||
            bridge->errorAbi != XL6_IR_CALLBACK_ERROR_PRESERVE ||
            bridge->reverseSupported != xl6IrRuntimeCallableFieldSignatureSupported(module, id)) goto malformed;
        for (p = 0; p < signature->paramCount; ++p) {
            XL6_IrCallableBridgeParameter expected;
            const XL6_IrCallableBridgeParameter* actual;
            if (next >= module->callableBridgeParameterCount ||
                !xl6IrCallableBridgeParameterPlan(module, signature, p, &expected)) goto malformed;
            actual = &module->callableBridgeParameters[next++];
            if (actual->parameter != expected.parameter || actual->type != expected.type ||
                actual->valueType!=expected.valueType || actual->presenceAbi!=expected.presenceAbi ||
                actual->decodeAbi != expected.decodeAbi || actual->encodeAbi != expected.encodeAbi ||
                !xl6IrCallableBridgeIntegerDomainEqual(actual->integerDomain, expected.integerDomain) ||
                actual->nativeScalarProfile != expected.nativeScalarProfile ||
                actual->invokeAbi != expected.invokeAbi ||
                actual->decodedDestroyAbi != expected.decodedDestroyAbi ||
                actual->inputDestroyAbi != expected.inputDestroyAbi) goto malformed;
        }
        for (p = 0; p < signature->returnCount; ++p) {
            XL6_IrCallableBridgeResult expected;
            const XL6_IrCallableBridgeResult* actual;
            if (nextResult >= module->callableBridgeResultCount ||
                !xl6IrCallableBridgeResultPlan(module, signature, p, &expected)) goto malformed;
            actual = &module->callableBridgeResults[nextResult++];
            if (actual->result != expected.result || actual->type != expected.type ||
                actual->valueType!=expected.valueType || actual->presenceAbi!=expected.presenceAbi ||
                actual->decodeAbi != expected.decodeAbi || actual->encodeAbi != expected.encodeAbi ||
                !xl6IrCallableBridgeIntegerDomainEqual(actual->integerDomain, expected.integerDomain) ||
                actual->nativeScalarProfile != expected.nativeScalarProfile ||
                actual->destroyAbi != expected.destroyAbi) goto malformed;
        }
    }
    if (b == module->callableBridgeCount && next == module->callableBridgeParameterCount &&
        nextResult == module->callableBridgeResultCount) return 1;
malformed:
    return xl6IrDiag(diags, NULL, "XLIR callable bridge decode, invocation or cleanup contract is malformed");
}
