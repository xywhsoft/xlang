static int xl6IrCgenDynamicMethodNeeded(const XL6_IrModule*,size_t);

static int xl6IrCgenPointActionSignature(const XL6_IrModule* module,XL6_IrTypeId id)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    const XL6_IrCallableBridge* bridge=xl6IrCallableBridgeGet(module,id);
    if(!type || !bridge || !type->typeValueId || type->paramCount!=1 || !type->params ||
       type->returnCount || bridge->erasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED)return 0;
    const XL6_IrType* parameter=xl6IrTypeGet(&module->types,type->params[0]);
    return parameter && parameter->cAbi==XL6_IR_C_ABI_RAW_POINTER;
}
static int xl6IrCgenHasPointActionSignature(const XL6_IrModule* module)
{
    for(size_t i=0;i<module->types.count;++i)
        if(xl6IrCgenPointActionSignature(module,(XL6_IrTypeId)(i+1)))return 1;
    return 0;
}

static int xl6IrCgenRuntimeCallableSignatureAbi(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    return xl6IrRuntimeCallableClosureToXValueSignatureSupported(module, type);
}

static int xl6IrCgenRuntimeCallableSlotIsErrorRef(
    const XL6_IrType* type)
{
    return type != NULL &&
        type->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
        type->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
        type->typeValueId != 0;
}

static int xl6IrCgenRuntimeCallableSlotIsRawPointer(
    const XL6_IrType* type)
{
    return type != NULL &&
        type->cAbi == XL6_IR_C_ABI_RAW_POINTER &&
        type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
        type->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL;
}

static int xl6IrCgenRuntimeCallableHasErrorParameter(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrType* signature = module != NULL
        ? xl6IrTypeGet(&module->types, type) : NULL;
    size_t i;
    if ( signature == NULL ) return 0;
    for ( i = 0; i < signature->paramCount; ++i )
        if ( xl6IrCgenRuntimeCallableSlotIsErrorRef(
                 xl6IrTypeGet(&module->types, signature->params[i])) )
            return 1;
    return 0;
}

static int xl6IrCgenRuntimeCallableHasErrorResult(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrType* signature = module != NULL
        ? xl6IrTypeGet(&module->types, type) : NULL;
    size_t i;
    if ( signature == NULL ) return 0;
    for ( i = 0; i < signature->returnCount; ++i )
        if ( xl6IrCgenRuntimeCallableSlotIsErrorRef(
                 xl6IrTypeGet(&module->types, signature->returns[i])) )
            return 1;
    return 0;
}

static int xl6IrCgenRuntimeCallableAdapterDirectlyNeeded(
    const XL6_IrModule* module, XL6_IrTypeId type, int reverse)
{
    size_t i;
    const XL6_IrType* signature = module != NULL ? xl6IrTypeGet(&module->types, type) : NULL;
    if ( signature == NULL || signature->kind != XL6_IR_TYPE_FUNCTION ) return 0;
    for (i=0;i<module->callTargetCount;++i) {
        const XL6_IrCallTarget* target=&module->callTargets[i];
        if(target->reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        XL6_IrNativeCallValuePlan value={0};
        if(xl6IrNativeCallValuePlanBuild(module,target,&value) &&
            value.value.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 &&
            value.value.baseElementType==type &&
            (reverse ? value.output : value.inputParameter!=0))return 1;
    }
    /* Dynamic adapters are producers/consumers of the same callable codec.
     * Reachability, not all declared function types, selects either bridge. */
    for ( i = 0; i < module->dynamicMethodCount; ++i ) {
        const XL6_IrDynamicMethod* method = &module->dynamicMethods[i];
        if ( !xl6IrCgenDynamicMethodNeeded(module, i) ) continue;
        if ( reverse ) {
            for ( size_t p = 0; p < method->parameterCount; ++p )
                if (xl6IrRuntimeCallableFieldSourceSignature(module,module->dynamicMethodParameters[method->firstParameter+p].type) == type) return 1;
        } else {
            const XL6_IrType* result = xl6IrTypeGet(&module->types, method->returnType);
            if (xl6IrRuntimeCallableFieldSourceSignature(module,method->returnType) == type) return 1;
            if ( result && result->cAbi == XL6_IR_C_ABI_PRODUCT )
                for ( size_t p = 0; p < result->paramCount; ++p )
                    if (xl6IrRuntimeCallableFieldSourceSignature(module,result->params[p]) == type) return 1;
        }
    }
    for ( i = 0; i < module->instructionCount; ++i ) {
        const XL6_IrInstruction* instruction = &module->instructions[i];
        XL6_IrTypeId signatureType;
        int aggregateClosure;
        if ( !xl6IrCgenInstructionIsReachable(module, i) ||
             instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ) continue;
        XL6_IrTypeId channelId=xl6IrChannelInstructionType(module,instruction);
        if (channelId) {
            XL6_IrChannelValuePlan message={0};
            if (xl6IrChannelValuePlanBuild(module,channelId,&message) &&
                message.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 && message.baseElementType==type &&
                (reverse ? xl6IrChannelReceiveSpecRuntime(instruction->runtimeOp)!=NULL :
                    xl6IrChannelSendSpecRuntime(instruction->runtimeOp)!=NULL)) return 1;
        }
        XL6_IrTypeId queueId=xl6IrConcurrentQueueInstructionType(module,instruction);
        if (queueId) {
            const XL6_IrType* queue=xl6IrTypeGet(&module->types,queueId);
            const XL6_IrType* schema=xl6IrTypeGet(&module->types,queue->messageSchemaType);
            if (schema && schema->sequenceLayout.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 &&
                schema->sequenceLayout.baseElementType==type &&
                (reverse ? xl6IrWaitQueueReceiveRuntime(instruction->runtimeOp)!=NULL :
                    instruction->runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_TRY_PUSH)) return 1;
        }
        if(xl6IrCgenStackRuntimeOpIs(instruction->runtimeOp) && instruction->runtimeOp!=XL6_IR_RUNTIME_STACK_CREATE &&
            instruction->operandCount && instruction->firstOperand<module->operandCount) {
            const XL6_IrInstruction* owner=xl6IrCgenValueDefinition(module,module->operands[instruction->firstOperand],NULL);
            const XL6_IrType* stack=owner ? xl6IrTypeGet(&module->types,owner->type) : NULL;
            if(stack && stack->sequenceLayout.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1 &&
                stack->sequenceLayout.baseElementType==type) {
                if(!reverse && instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PUSH)return 1;
                if(reverse && (instruction->runtimeOp==XL6_IR_RUNTIME_STACK_POP || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK_AT || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_POP ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK_AT))return 1;
            }
        }
        aggregateClosure =
            (instruction->runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ||
             instruction->runtimeOp ==
                 XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY) &&
            instruction->asyncResultAbi ==
                XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE;
        signatureType = aggregateClosure
            ? xl6IrRuntimeCallableFieldSourceSignature(module, instruction->payloadType) : instruction->calleeType;
        if ( reverse ) {
            if ( (instruction->runtimeOp ==
                      XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE ||
                  instruction->runtimeOp ==
                      XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_CLOSURE_COPY ||
                  instruction->runtimeOp ==
                      XL6_IR_RUNTIME_XVALUE_PROJECT_CLOSURE_COPY ||
                  instruction->runtimeOp ==
                      XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED) &&
                 signatureType == type ) return 1;
        } else if ( (instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX ||
                     instruction->runtimeOp ==
                         XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE ||
                     aggregateClosure) && signatureType == type ) return 1;
    }
    return 0;
}

static int xl6IrCgenRuntimeCallableAdapterNeededDepth(
    const XL6_IrModule* module, XL6_IrTypeId type, int reverse, size_t depth)
{
    size_t parentIndex;
    if ( module == NULL || depth > module->types.count ) return 0;
    if ( xl6IrCgenRuntimeCallableAdapterDirectlyNeeded(
             module, type, reverse) ) return 1;
    for ( parentIndex = 0; parentIndex < module->callableBridgeCount; ++parentIndex ) {
        const XL6_IrCallableBridge* parent = &module->callableBridges[parentIndex];
        XL6_IrTypeId parentType = parent->signatureType;
        size_t resultIndex;
        for ( resultIndex = 0; resultIndex < parent->resultCount; ++resultIndex ) {
            const XL6_IrCallableBridgeResult* slot=&module->callableBridgeResults[parent->firstResult+resultIndex];
            if ( (!reverse || parent->reverseSupported) && slot->valueType == type &&
                 slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY &&
                 xl6IrCgenRuntimeCallableAdapterNeededDepth(
                     module, parentType, reverse, depth + 1) ) return 1;
        }
        /* A parameter reverses the bridge direction: native input encoding
         * needs the child's forward entry; erased input decoding its reverse.
         * Nullable carriers share the same exact underlying signature. */
        for (size_t p = 0; p < parent->parameterCount; ++p) {
            const XL6_IrCallableBridgeParameter* slot=&module->callableBridgeParameters[parent->firstParameter+p];
            if ((reverse || parent->reverseSupported) && slot->valueType == type &&
                slot->decodeAbi == XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY &&
                xl6IrCgenRuntimeCallableAdapterNeededDepth(module,parentType,!reverse,depth+1)) return 1;
        }
    }
    return 0;
}

static int xl6IrCgenRuntimeCallableAdapterNeeded(
    const XL6_IrModule* module, XL6_IrTypeId type, int reverse)
{
    return xl6IrCgenRuntimeCallableAdapterNeededDepth(
        module, type, reverse, 0);
}

static int xl6IrCgenRuntimeCallableHasClosureResultDepth(
    const XL6_IrModule* module, XL6_IrTypeId type, size_t depth)
{
    const XL6_IrType* signature;
    size_t i;
    if ( module == NULL || depth > module->types.count ||
         !xl6IrRuntimeCallableFieldSignatureSupported(module, type) ||
         (signature = xl6IrTypeGet(&module->types, type)) == NULL ) return 0;
    for ( i = 0; i < signature->returnCount; ++i ) {
        const XL6_IrType* result = xl6IrTypeGet(
            &module->types, signature->returns[i]);
        if ( result != NULL && result->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
            return 1;
        if ( result != NULL && result->kind == XL6_IR_TYPE_FUNCTION &&
             xl6IrCgenRuntimeCallableHasClosureResultDepth(
                 module, signature->returns[i], depth + 1) ) return 1;
    }
    return 0;
}

static int xl6IrCgenRuntimeCallableHasClosureResult(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    return xl6IrCgenRuntimeCallableHasClosureResultDepth(module, type, 0);
}

static int xl6IrCgenProductDropDirectlyNeeded(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    size_t i;
    if ( module == NULL ) return 0;
    for ( i = 0; i < module->instructionCount; ++i ) {
        const XL6_IrInstruction* instruction = &module->instructions[i];
        if (!xl6IrCgenInstructionIsReachable(module, i) || instruction->type != type) continue;
        if (instruction->kind == XL6_IR_INSTR_RELEASE) return 1;
        /* Checked native results may own a received prefix even on failure.
         * STATUS_OUTPUT empties that result before the language error edge,
         * so its rollback owns the whole product independently of RELEASE. */
        if (xl6IrCallInstructionErrorPhysicalAbi(module, instruction) ==
            XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT)
            return 1;
    }
    /* Export transactions also own a pending result until cleanup commits.
     * That root requires the same recursive product drop dependency as an
     * explicit RELEASE instruction, even when the body only returns it. */
    for (i = 0; i < module->exportCount; ++i) {
        const XL6_IrCallTarget* body = xl6IrCallTargetGet(module, module->exports[i].symbol);
        if (module->exports[i].resultBridgeAbi == XL6_IR_EXPORT_RESULT_C_OWNED_COPY &&
            body && body->returnType == type) return 1;
        if (module->exports[i].errorAbi == XL6_IR_EXPORT_ERROR_SHARED_SCOPE_TRANSACTION &&
            module->exports[i].returnAbi == XL6_IR_EXPORT_RETURN_OWNED &&
            module->exports[i].returnType == type) return 1;
    }
    /* A forward runtime-callable adapter owns the whole result product until
     * transport commits, including the not-yet-boxed suffix on failure. */
    for ( i = 0; i < module->types.count; ++i ) {
        const XL6_IrType* signature = &module->types.items[i];
        if ( signature->kind == XL6_IR_TYPE_FUNCTION &&
             signature->callableResultType == type &&
             xl6IrCgenRuntimeCallableAdapterNeeded(
                 module, (XL6_IrTypeId)(i + 1), 0) ) return 1;
    }
    /* A reachable erased method adapter owns the typed return product until
     * every field has been boxed, even when the method body only returns it. */
    for(i=0;i<module->dynamicMethodCount;++i)
        if(module->dynamicMethods[i].kind==XL6_IR_DYNAMIC_CALL_METHOD &&
           module->dynamicMethods[i].returnType==type &&
           xl6IrCgenDynamicMethodNeeded(module,i))return 1;
    return 0;
}

static int xl6IrCgenProductDropNeededDepth(
    const XL6_IrModule* module, XL6_IrTypeId type, size_t depth)
{
    const XL6_IrType* product = module != NULL
        ? xl6IrTypeGet(&module->types, type) : NULL;
    size_t parentIndex;
    if ( module == NULL || depth > module->types.count ||
         product == NULL || product->kind != XL6_IR_TYPE_TUPLE ||
         product->cAbi != XL6_IR_C_ABI_PRODUCT ||
         !xl6IrTypeHasStorageReleaseAbi(module, type) ) return 0;
    if ( xl6IrCgenProductDropDirectlyNeeded(module, type) ) return 1;
    for ( parentIndex = 0; parentIndex < module->types.count;
          ++parentIndex ) {
        XL6_IrTypeId parentType = (XL6_IrTypeId)(parentIndex + 1);
        const XL6_IrType* parent = xl6IrTypeGet(
            &module->types, parentType);
        size_t element;
        if ( parent == NULL || parent->kind != XL6_IR_TYPE_TUPLE ||
             parent->cAbi != XL6_IR_C_ABI_PRODUCT ||
             !xl6IrTypeHasStorageReleaseAbi(module, parentType) ) continue;
        for ( element = 0; element < parent->paramCount; ++element )
            if ( parent->params[element] == type &&
                 xl6IrCgenProductDropNeededDepth(
                     module, parentType, depth + 1) ) return 1;
    }
    return 0;
}

static int xl6IrCgenProductDropNeeded(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    return xl6IrCgenProductDropNeededDepth(module, type, 0);
}

static const char* xl6IrCgenStorageReleaseFunction(
    const XL6_IrType* type)
{
    if ( type == NULL || type->ownershipAbi !=
             XL6_IR_OWNERSHIP_ABI_MANAGED ) return NULL;
    switch ( type->cAbi ) {
    case XL6_IR_C_ABI_MANAGED_STRING: return "xlir_string_drop";
    case XL6_IR_C_ABI_C_STRING:
    case XL6_IR_C_ABI_C_BYTES: return "xl6COwnedDropValueV1";
    case XL6_IR_C_ABI_MANAGED_BYTES: return "xrtValueRelease";
    case XL6_IR_C_ABI_NATIVE_VALUE: {
        const XL6_IrNativeValuePlan* plan=xl6IrNativeValuePlanForType(type);
        return plan ? plan->dropSymbol : NULL;
    }
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR: return "xrtValueRelease";
    case XL6_IR_C_ABI_RUNTIME_ERROR_PTR: return "xrtErrorFree";
    case XL6_IR_C_ABI_RUNTIME_FUTURE_PTR: return "xrtFutureDestroy";
    case XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR: return "xlir_generator_destroy";
    case XL6_IR_C_ABI_CLOSURE_PTR: return "xlir_closure_release";
    case XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR:
        return "xlir_xvalue_iterator_destroy";
    default: return NULL;
    }
}

static int xl6IrCgenEmitProductDropPrototype(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    const char* cType = xl6IrCgenType(module, type);
    if ( !xl6IrCgenProductDropNeeded(module, type) || cType == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter product destructor prototype is unresolved");
    xl6OutputPrintf(out, "static void xlir_product_drop_t%u(%s*);\n",
        (unsigned int)type, cType);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenEmitProductDrop(
    XL6_Output* out, const XL6_IrModule* module, XL6_IrTypeId type,
    XL6_DiagList* diags)
{
    const XL6_IrType* product = xl6IrTypeGet(&module->types, type);
    const char* cType = xl6IrCgenType(module, type);
    size_t reverse;
    if ( !xl6IrCgenProductDropNeeded(module, type) || product == NULL ||
         cType == NULL )
        return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
            "XLIR C emitter product destructor is unresolved");
    xl6OutputPrintf(out,
        "static void xlir_product_drop_t%u(%s* owner)\n{\n"
        "    %s detached; %s* value = &detached;\n"
        "    if (owner == NULL) return;\n"
        "    detached = *owner; *owner = (%s){0};\n",
        (unsigned int)type, cType, cType, cType, cType);
    for ( reverse = product->paramCount; reverse != 0; --reverse ) {
        size_t element = reverse - 1;
        XL6_IrTypeId elementTypeId = product->params[element];
        const XL6_IrType* elementType = xl6IrTypeGet(
            &module->types, elementTypeId);
        const char* release;
        if ( elementType == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter product destructor element is unresolved");
        if ( elementType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL )
            continue;
        if ( elementType->kind == XL6_IR_TYPE_TUPLE &&
             elementType->cAbi == XL6_IR_C_ABI_PRODUCT ) {
            if ( !xl6IrCgenProductDropNeeded(module, elementTypeId) )
                return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                    "XLIR C emitter nested product destructor is unreachable");
            xl6OutputPrintf(out,
                "    xlir_product_drop_t%u(&value->e%lu);\n",
                (unsigned int)elementTypeId, (unsigned long)element);
            continue;
        }
        release = xl6IrCgenStorageReleaseFunction(elementType);
        if ( release == NULL )
            return xl6IrCgenDiag(diags, module, XL6_IR_NODE_INVALID,
                "XLIR C emitter product element destructor ABI is unresolved");
        if ( xl6IrTypeIsCOwned(elementType) ||
             elementType->cAbi == XL6_IR_C_ABI_MANAGED_STRING ||
             elementType->cAbi == XL6_IR_C_ABI_NATIVE_VALUE )
            xl6OutputPrintf(out,
                "    %s(value->e%lu); value->e%lu = (%s){0};\n",
                release, (unsigned long)element, (unsigned long)element,
                xl6IrCgenType(module,elementTypeId));
        else
            xl6OutputPrintf(out,
                "    %s(value->e%lu); value->e%lu = NULL;\n",
                release, (unsigned long)element, (unsigned long)element);
    }
    xl6OutputPuts("}\n", out);
    return !xl6OutputHasFailed(out);
}

static int xl6IrCgenRuntimeCallableFromClosureAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* closure;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL ||
         !xl6IrCgenRuntimeCallableSignatureAbi(module, instruction->calleeType) ||
         instruction->runtimeIdentityType != instruction->calleeType ||
         xl6IrTypeGet(&module->types,
             instruction->runtimeIdentityType)->typeValueId == 0 ||
         xl6IrTypeGet(&module->types, instruction->calleeType)->callableErasureAbi !=
             instruction->callableErasureAbi ||
         xl6IrTypeGet(&module->types, instruction->type) == NULL ||
         xl6IrTypeGet(&module->types, instruction->type)->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) return 0;
    closure = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    return closure != NULL && closure->type == instruction->calleeType;
}

static int xl6IrCgenRuntimeNullableStringBoxAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* nullable;
    const XL6_IrType* element;
    const XL6_IrType* result;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    nullable = source != NULL ? xl6IrTypeGet(&module->types, source->type) : NULL;
    element = nullable != NULL && nullable->paramCount == 1 && nullable->params != NULL
        ? xl6IrTypeGet(&module->types, nullable->params[0]) : NULL;
    result = xl6IrTypeGet(&module->types, instruction->type);
    return nullable != NULL && nullable->kind == XL6_IR_TYPE_NULLABLE &&
           nullable->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
           nullable->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
           nullable->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           element != NULL && element->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
           element->nullAbi == nullable->nullAbi &&
           element->ownershipAbi == nullable->ownershipAbi &&
           result != NULL && result->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
}

static int xl6IrCgenRuntimeNullableStringCopyAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* nullable;
    const XL6_IrType* element;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    nullable = xl6IrTypeGet(&module->types, instruction->type);
    element = nullable != NULL && nullable->paramCount == 1 && nullable->params != NULL
        ? xl6IrTypeGet(&module->types, nullable->params[0]) : NULL;
    return source != NULL && source->type == instruction->type &&
           nullable != NULL && nullable->kind == XL6_IR_TYPE_NULLABLE &&
           nullable->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
           nullable->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
           nullable->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           element != NULL && element->cAbi == XL6_IR_C_ABI_MANAGED_STRING &&
           element->nullAbi == nullable->nullAbi &&
           element->ownershipAbi == nullable->ownershipAbi;
}

static int xl6IrCgenRuntimeSafeIntegerIndexReadAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction,
    XL6_IrCollectionAbi collectionAbi)
{
    const XL6_IrInstruction* receiver;
    const XL6_IrInstruction* index;
    const XL6_IrType* receiverType;
    const XL6_IrType* collectionType;
    const XL6_IrType* indexType;
    const XL6_IrType* resultType;
    int dynamicResult;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 2 ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount > module->operandCount - instruction->firstOperand ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    receiver = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    index = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand + 1], NULL);
    receiverType = receiver != NULL
        ? xl6IrTypeGet(&module->types, receiver->type) : NULL;
    collectionType = receiverType;
    if ( receiverType != NULL &&
         receiverType->kind == XL6_IR_TYPE_NULLABLE &&
         receiverType->paramCount == 1 && receiverType->params != NULL )
        collectionType = xl6IrTypeGet(
            &module->types, receiverType->params[0]);
    indexType = index != NULL ? xl6IrTypeGet(&module->types, index->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    dynamicResult = resultType != NULL &&
        resultType->kind == XL6_IR_TYPE_BUILTIN &&
        resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
        resultType->collectionAbi == XL6_IR_COLLECTION_NONE &&
        resultType->paramCount == 0 &&
        resultType->nominalSymbol == XL6_SYMBOL_ID_INVALID &&
        resultType->typeValueId == module->runtimeTypeMap.fallbackTypeValueId;
    return receiverType != NULL && collectionType != NULL &&
           indexType != NULL && resultType != NULL &&
           receiverType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           collectionType->collectionAbi == collectionAbi &&
           indexType->cAbi == XL6_IR_C_ABI_I64 &&
           resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           dynamicResult;
}

static int xl6IrCgenRuntimeNullableClosureBoxAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* nullable;
    const XL6_IrType* signature;
    const XL6_IrType* result;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         !xl6IrCgenRuntimeCallableSignatureAbi(module, instruction->calleeType) ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    nullable = source != NULL ? xl6IrTypeGet(&module->types, source->type) : NULL;
    signature = xl6IrTypeGet(&module->types, instruction->calleeType);
    result = xl6IrTypeGet(&module->types, instruction->type);
    return nullable != NULL && nullable->kind == XL6_IR_TYPE_NULLABLE &&
           nullable->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
           nullable->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
           nullable->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           nullable->paramCount == 1 && nullable->params != NULL &&
           nullable->params[0] == instruction->calleeType &&
           instruction->runtimeIdentityType == instruction->calleeType &&
           signature != NULL && instruction->callableErasureAbi ==
               signature->callableErasureAbi && signature->typeValueId != 0 &&
           result != NULL && result->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
}

static XL6_IrTypeId xl6IrCgenRuntimeCallableSourceSignature(
    const XL6_IrModule* module, XL6_IrTypeId typeId)
{
    const XL6_IrType* source = module != NULL
        ? xl6IrTypeGet(&module->types, typeId) : NULL;
    if ( xl6IrCgenRuntimeCallableSignatureAbi(module, typeId) ) return typeId;
    if ( source == NULL || source->kind != XL6_IR_TYPE_NULLABLE ||
         source->cAbi != XL6_IR_C_ABI_CLOSURE_PTR ||
         source->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
         source->nullAbi != XL6_IR_NULL_ABI_POINTER_SENTINEL ||
         source->paramCount != 1 || source->params == NULL ||
         !xl6IrCgenRuntimeCallableSignatureAbi(module, source->params[0]) )
        return XL6_IR_TYPE_INVALID;
    return source->params[0];
}

static int xl6IrCgenRuntimeObjectClosureFieldAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction, int isStore)
{
    const XL6_IrInstruction* object;
    const XL6_IrInstruction* closure = NULL;
    const XL6_IrType* result;
    XL6_IrTypeId signature;
    if ( module == NULL || instruction == NULL ||
         instruction->operandCount != (isStore ? 2u : 1u) ||
         instruction->immediateText == NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         !xl6IrRuntimeCallableFieldSignatureSupported(
             module, instruction->calleeType) )
        return 0;
    object = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    if ( object == NULL || xl6IrTypeGet(&module->types, object->type) == NULL ||
         xl6IrTypeGet(&module->types, object->type)->cAbi !=
             XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) return 0;
    result = xl6IrTypeGet(&module->types, instruction->type);
    if ( isStore ) {
        closure = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 1], NULL);
        signature = closure != NULL
            ? xl6IrRuntimeCallableFieldSourceSignature(module, closure->type)
            : XL6_IR_TYPE_INVALID;
        return result != NULL && result->cAbi == XL6_IR_C_ABI_BOOL &&
               signature == instruction->calleeType &&
               instruction->runtimeIdentityType == signature &&
               xl6IrTypeGet(&module->types, signature) != NULL &&
               xl6IrTypeGet(&module->types, signature)->typeValueId != 0;
    }
    signature = xl6IrRuntimeCallableFieldSourceSignature(
        module, instruction->type);
    return signature == instruction->calleeType && result != NULL &&
           result->cAbi == XL6_IR_C_ABI_CLOSURE_PTR;
}

static const XL6_IrType* xl6IrCgenTaggedNullableElement(
    const XL6_IrModule* module, XL6_IrTypeId typeId)
{
    const XL6_IrType* tagged = module != NULL
        ? xl6IrTypeGet(&module->types, typeId) : NULL;
    const XL6_IrType* element = tagged != NULL &&
        tagged->kind == XL6_IR_TYPE_NULLABLE && tagged->paramCount == 1 &&
        tagged->params != NULL
            ? xl6IrTypeGet(&module->types, tagged->params[0]) : NULL;
    if ( tagged == NULL || tagged->cAbi != XL6_IR_C_ABI_TAGGED_NULLABLE ||
         tagged->nullAbi != XL6_IR_NULL_ABI_TAGGED_VALUE ||
         tagged->cName[0] == '\0' || element == NULL ||
         !xl6IrTaggedNullableScalarSupported(element->cAbi) ) return NULL;
    return element;
}

static int xl6IrCgenRuntimeTaggedNullableXValueAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* operand;
    const XL6_IrType* operandType;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    operand = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    operandType = operand != NULL
        ? xl6IrTypeGet(&module->types, operand->type) : NULL;
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX ) {
        const XL6_IrType* resultType = xl6IrTypeGet(&module->types, instruction->type);
        return instruction->immediateText == NULL &&
               resultType != NULL &&
               resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
               (xl6IrCgenTaggedNullableElement(module, operand != NULL
                   ? operand->type : XL6_IR_TYPE_INVALID) != NULL ||
                (operand && xl6IrNullableUnionElementType(module,operand->type)));
    }
    if ( instruction->runtimeOp ==
         XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE )
        return instruction->immediateText != NULL &&
               operandType != NULL && operandType->kind == XL6_IR_TYPE_CLASS &&
               operandType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
               xl6IrCgenTaggedNullableElement(module, instruction->type) != NULL;
    return 0;
}

static int xl6IrCgenRuntimeUnionBoxAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    const XL6_IrUnionLayout* layout;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_UNION_BOX ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->payloadType == XL6_IR_TYPE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    layout = xl6IrUnionLayoutGet(module, instruction->payloadType);
    return sourceType != NULL && source->type == instruction->payloadType &&
           sourceType->kind == XL6_IR_TYPE_UNION &&
           sourceType->cAbi == XL6_IR_C_ABI_UNION &&
           sourceType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
           sourceType->typeValueId != 0 && sourceType->cName[0] != '\0' &&
           layout != NULL && layout->type == source->type &&
           resultType != NULL &&
           resultType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED;
}

static int xl6IrCgenRuntimeUnionUnboxAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    const XL6_IrUnionLayout* layout;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp != XL6_IR_RUNTIME_XVALUE_UNION_UNBOX ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->payloadType != instruction->type ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    layout = xl6IrUnionLayoutGet(module, instruction->payloadType);
    return sourceType != NULL &&
           sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           resultType != NULL && resultType->kind == XL6_IR_TYPE_UNION &&
           resultType->cAbi == XL6_IR_C_ABI_UNION &&
           resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
           resultType->typeValueId != 0 && resultType->cName[0] != '\0' &&
           layout != NULL && layout->type == instruction->type;
}

/* Source-level composite casts are deliberately stricter than invariant-only
 * projections.  They consume one borrowed open xvalue, carry the exact target
 * identity in payloadType, and own a recoverable Cast ABI. */
static int xl6IrCgenRuntimeCheckedUnionCastAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    const XL6_IrUnionLayout* layout;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp !=
             XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->payloadType != instruction->type ||
         instruction->castAbi != XL6_IR_CAST_XVALUE_TO_UNION_CHECKED ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    layout = xl6IrUnionLayoutGet(module, instruction->payloadType);
    return xl6IrCgenTypeIsOpenDynamicBuiltin(module, sourceType) &&
           resultType != NULL && resultType->kind == XL6_IR_TYPE_UNION &&
           resultType->cAbi == XL6_IR_C_ABI_UNION &&
           resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
           resultType->typeValueId != 0 && resultType->cName[0] != '\0' &&
           layout != NULL && layout->type == instruction->type;
}

static int xl6IrCgenRuntimeCheckedClosureCastAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL ||
         instruction->runtimeOp !=
             XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED ||
         instruction->operandCount != 1 || instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->payloadType != instruction->type ||
         instruction->calleeType != instruction->type ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->castAbi != XL6_IR_CAST_XVALUE_TO_CLOSURE_CHECKED ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL
        ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return xl6IrCgenTypeIsOpenDynamicBuiltin(module, sourceType) &&
           resultType != NULL && resultType->kind == XL6_IR_TYPE_FUNCTION &&
           resultType->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
           resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           resultType->nullAbi == XL6_IR_NULL_ABI_POINTER_SENTINEL &&
           resultType->typeValueId != 0 &&
           xl6IrRuntimeCallableFieldSignatureSupported(
               module, instruction->calleeType);
}

static int xl6IrCgenRuntimeDynamicCallAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrDynamicCallSite* site;
    size_t i;
    if ( module == NULL || instruction == NULL || instruction->operandCount < 1 ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->dynamicCallSite >= module->dynamicCallSiteCount ||
         xl6IrTypeGet(&module->types, instruction->type) == NULL ||
         !(xl6IrTypeGet(&module->types, instruction->type)->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
           (instruction->dynamicCallResultAbi==XL6_IR_DYNAMIC_CALL_RESULT_OWNED_PRODUCT &&
            xl6IrDynamicProductType(module,instruction->type))) ) return 0;
    site = &module->dynamicCallSites[instruction->dynamicCallSite];
    if ( site->resultType != instruction->type ||
         !(site->kind == XL6_IR_DYNAMIC_CALL_MEMBER_SET
             ? site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_OWNED_UNIT_VALUE
             : site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_OWNED_FIRST_VALUE ||
               ((site->kind == XL6_IR_DYNAMIC_CALL_FUNCTION || site->kind==XL6_IR_DYNAMIC_CALL_METHOD) && site->resultAbi==XL6_IR_DYNAMIC_CALL_RESULT_OWNED_PRODUCT) ||
               (site->kind == XL6_IR_DYNAMIC_CALL_FUNCTION &&
                site->resultAbi == XL6_IR_DYNAMIC_CALL_RESULT_DISCARD_TO_OWNED_UNIT)) ||
         site->evaluationOrder != (site->kind == XL6_IR_DYNAMIC_CALL_MEMBER_SET
             ? XL6_IR_DYNAMIC_CALL_ARGUMENTS_FIRST : XL6_IR_DYNAMIC_CALL_SUBJECT_FIRST) ||
         instruction->dynamicCallResultAbi != site->resultAbi ||
         site->argumentCount + 1u != instruction->operandCount ||
         site->positionalCount + site->keywordCount != site->argumentCount ||
         site->fallbackCollectionOp != instruction->dynamicCollectionOp ||
         ((instruction->runtimeOp == XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL) !=
          (site->kind == XL6_IR_DYNAMIC_CALL_FUNCTION)) ||
         ((instruction->runtimeOp ==
               XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL) !=
          (site->kind == XL6_IR_DYNAMIC_CALL_METHOD || site->kind == XL6_IR_DYNAMIC_CALL_MEMBER_GET || site->kind == XL6_IR_DYNAMIC_CALL_MEMBER_SET)) ) return 0;
    for ( i = 0; i < instruction->operandCount; ++i ) {
        const XL6_IrInstruction* value = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + i], NULL);
        const XL6_IrType* type = value != NULL
            ? xl6IrTypeGet(&module->types, value->type) : NULL;
        if ( type == NULL || type->cAbi != XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) return 0;
    }
    if ( site->kind == XL6_IR_DYNAMIC_CALL_FUNCTION ) {
        if ( site->fallbackCollectionOp !=
             XL6_IR_DYNAMIC_COLLECTION_INVALID ) return 0;
    } else if ( site->kind == XL6_IR_DYNAMIC_CALL_METHOD || site->kind == XL6_IR_DYNAMIC_CALL_MEMBER_GET || site->kind == XL6_IR_DYNAMIC_CALL_MEMBER_SET ) {
        size_t method;
        size_t candidateCount = 0;
        if ( site->memberName == NULL || site->memberName[0] == '\0' ) return 0;
        if ( site->fallbackCollectionOp !=
                 XL6_IR_DYNAMIC_COLLECTION_INVALID &&
             (site->keywordCount != 0 ||
              site->positionalCount != site->argumentCount ||
              !xl6IrDynamicCollectionOpOperandCountValid(
                  site->fallbackCollectionOp, site->argumentCount) ||
              xl6IrCgenDynamicCollectionOpToken(
                  site->fallbackCollectionOp) == NULL) ) return 0;
        for ( method = 0; method < module->dynamicMethodCount; ++method ) {
            const XL6_IrDynamicMethod* candidate =
                &module->dynamicMethods[method];
            const XL6_IrCallTarget* target;
            if ( !xl6IrDynamicMethodMatchesCallSite(site, candidate) ) continue;
            /* The registry deliberately retains unsupported rows so Typed and
             * Lowered XLIR preserve the complete source method inventory.
             * Runtime dispatch is the closed subset of VERIFIED adapters;
             * unrelated same-name unsupported rows are not candidates and
             * must not invalidate an otherwise exact call site. */
            if ( candidate->adapterAbi !=
                     XL6_IR_DYNAMIC_METHOD_ADAPTER_VERIFIED ) continue;
            if ( (candidate->targetErrorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
                  candidate->targetErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) ||
                 (target = xl6IrCallTargetGet(
                      module, candidate->targetSymbol)) == NULL ||
                 !xl6IrDynamicMethodCallTargetSupported(module, target) ||
                 target->reachability == XL6_IR_REACHABILITY_UNREACHABLE )
                return 0;
            candidateCount += 1u;
        }
        /* A verified collection fallback is itself a closed dispatch target.
         * Unsupported class rows do not erase that target; only a method with
         * neither a verified adapter nor an exact fallback is unresolved. */
        if ( site->kind == XL6_IR_DYNAMIC_CALL_METHOD && candidateCount == 0 &&
             site->fallbackCollectionOp == XL6_IR_DYNAMIC_COLLECTION_INVALID ) return 0;
        if ( site->kind == XL6_IR_DYNAMIC_CALL_MEMBER_GET &&
             (site->argumentCount != 0 || site->fallbackCollectionOp != XL6_IR_DYNAMIC_COLLECTION_INVALID)) return 0;
    }
    return 1;
}

static int xl6IrCgenRuntimeFutureCancelAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* source;
    const XL6_IrType* sourceType;
    const XL6_IrType* resultType;
    if ( module == NULL || instruction == NULL || instruction->operandCount != 1 ||
         instruction->immediateText != NULL || instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ) return 0;
    source = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    sourceType = source != NULL ? xl6IrTypeGet(&module->types, source->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    return sourceType != NULL &&
           sourceType->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR &&
           sourceType->paramCount == 1 && resultType != NULL &&
           resultType->cAbi == XL6_IR_C_ABI_BOOL;
}

static const char* xl6IrCgenFutureMemberHelper(XL6_IrRuntimeOp operation)
{
    switch ( operation ) {
    case XL6_IR_RUNTIME_FUTURE_WAIT: return "xlir_future_wait";
    case XL6_IR_RUNTIME_FUTURE_WAIT_TIMEOUT:
        return "xlir_future_wait_timeout";
    case XL6_IR_RUNTIME_FUTURE_WAIT_UNTIL:
        return "xlir_future_wait_until";
    case XL6_IR_RUNTIME_FUTURE_IS_DONE: return "xlir_future_is_done";
    case XL6_IR_RUNTIME_FUTURE_IS_PENDING: return "xlir_future_is_pending";
    case XL6_IR_RUNTIME_FUTURE_IS_CANCELLED:
        return "xlir_future_is_cancelled";
    case XL6_IR_RUNTIME_FUTURE_IS_FAILED: return "xlir_future_is_failed";
    case XL6_IR_RUNTIME_FUTURE_STATE: return "xlir_future_state";
    case XL6_IR_RUNTIME_FUTURE_STATUS: return "xlir_future_status";
    case XL6_IR_RUNTIME_FUTURE_ERROR_REF: return "xlir_future_error_ref";
    case XL6_IR_RUNTIME_FUTURE_SET_DEBUG_NAME:
        return "xl6FutureDebugNameSetV2";
    case XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY:
        return "xl6FutureDebugNameCopyV2";
    case XL6_IR_RUNTIME_FUTURE_CREATE_TIME_MS:
        return "xl6FutureFirstObservedMsV2";
    case XL6_IR_RUNTIME_FUTURE_COMPLETE_TIME_MS:
        return "xl6FutureFirstTerminalObservedMsV2";
    case XL6_IR_RUNTIME_FUTURE_PENDING_CONTINUATIONS:
        return "xl6FuturePendingWatchesV2";
    default: return NULL;
    }
}

/* Feature selection must not depend on enum adjacency: RuntimeOp values are
 * append-only stable identities and later Future operations may be separated
 * from the original observation family. */
