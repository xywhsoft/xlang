/* Derive all generated-runtime dependencies from reachable lowered IR. */
static int xl6IrCgenAnalyzeSequenceRecipeNeeds(const XL6_IrModule* module,XL6_IrTypeId id,
    XL6_IrCgenNeeds* needs,size_t depth)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if (!type || depth>64 || !xl6IrSequenceConstructionRecipe(module,id,type->sequenceLayout.role,NULL)) return 0;
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_NOMINAL_V2) needs->needsNominalIdentityRuntime=1;
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_SEMAPHORE_REFERENCE_V1) {
        needs->needsSemaphoreResourceRuntime=1;
        needs->needsForeignErrorRuntime=1;
    }
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_THREAD_REFERENCE_V1) {
        needs->needsThreadResourceRuntime=1;
        needs->needsForeignErrorRuntime=1;
    }
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_MUTEX_REFERENCE_V1 ||
        type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_COND_REFERENCE_V1 ||
        type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_RWLOCK_REFERENCE_V1) {
        needs->needsSyncResourceRuntime=1;
        needs->needsForeignErrorRuntime=1;
    }
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_SEQUENCE_REFERENCE_V1)
        return xl6IrCgenAnalyzeSequenceRecipeNeeds(module,type->sequenceLayout.baseElementType,needs,depth+1);
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_LIST_REFERENCE_V1 ||
        type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_SET_REFERENCE_V1 ||
        type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_DICT_REFERENCE_V1) {
        XL6_IrValueCollectionRecipe child={0};
        XL6_IrCollectionAbi family=xl6IrCgenValueCollectionFamily(type->sequenceLayout.abi);
        if (family==XL6_IR_COLLECTION_LIST) needs->needsListCreateRuntime=1;
        else if (family==XL6_IR_COLLECTION_SET) needs->needsSetCreateRuntime=1;
        else if (family==XL6_IR_COLLECTION_DICT) needs->needsDictCreateRuntime=1;
        else return 0;
        needs->needsArrayCreateRuntime=1;
        if (family==XL6_IR_COLLECTION_LIST && needs->needsArrayImportRuntime) needs->needsListImportRuntime=1;
        if (family==XL6_IR_COLLECTION_SET && needs->needsArrayImportRuntime) needs->needsSetImportRuntime=1;
        if (family==XL6_IR_COLLECTION_DICT && needs->needsArrayImportRuntime) needs->needsDictImportRuntime=1;
        return xl6IrValueCollectionConstructionRecipe(module,type->sequenceLayout.baseElementType,&child) &&
            child.family==family &&
            xl6IrCgenAnalyzeSequenceRecipeNeeds(module,child.valueSchemaType,needs,depth+1);
    }
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_TREE_REFERENCE_V1) {
        XL6_IrTypeId key=0,value=0;
        needs->needsAvlTreeResourceRuntime=1;
        needs->needsForeignErrorRuntime=1;
        return xl6IrAvlConstructionRecipe(module,type->sequenceLayout.baseElementType,&key,&value,NULL,NULL) &&
            xl6IrCgenAnalyzeSequenceRecipeNeeds(module,key,needs,depth+1) &&
            xl6IrCgenAnalyzeSequenceRecipeNeeds(module,value,needs,depth+1);
    }
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_CHANNEL_REFERENCE_V1 ||
        type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_SLOT_MAP_REFERENCE_V1) {
        const XL6_IrType* resource=xl6IrTypeGet(&module->types,type->sequenceLayout.baseElementType);
        if(resource) {
            XL6_IrTypeId child=type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_CHANNEL_REFERENCE_V1
                ? resource->messageSchemaType : resource->collectionValueSchemaType;
            return xl6IrCgenAnalyzeSequenceRecipeNeeds(module,child,needs,depth+1);
        }
        return 0;
    }
    if (type->sequenceLayout.abi==XL6_IR_SEQUENCE_LAYOUT_QUEUE_REFERENCE_V1) {
        XL6_IrRuntimeResourceAbi family=XL6_IR_RUNTIME_RESOURCE_NONE;
        XL6_IrTypeId schema=0;
        if (!xl6IrConcurrentQueueConstructionRecipe(module,type->sequenceLayout.baseElementType,&family,&schema,NULL)) return 0;
        switch (family) {
        case XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE: needs->needsSpscQueueRuntime=1; break;
        case XL6_IR_RUNTIME_RESOURCE_MPSC_QUEUE: needs->needsMpscQueueRuntime=1; break;
        case XL6_IR_RUNTIME_RESOURCE_MPMC_QUEUE: needs->needsMpmcQueueRuntime=1; break;
        case XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE: needs->needsWaitQueueRuntime=1; break;
        default: return 0;
        }
        needs->needsForeignErrorRuntime=1;
        return xl6IrCgenAnalyzeSequenceRecipeNeeds(module,schema,needs,depth+1);
    }
    return 1;
}
/* Physical release support belongs to the carrier's closed ABI. A product
 * returned by an exported function may need destruction even when no scalar
 * RELEASE instruction of that member type occurs in its body. Follow the
 * same emitted-product predicate as definitions/prototypes; do not select
 * libraries from a source name or pull in every unused signature. */
static int xl6IrCgenAnalyzeReleaseNeeds(
    const XL6_IrModule* module,XL6_IrTypeId id,XL6_IrCgenNeeds* needs,size_t depth)
{
    const XL6_IrType* type=xl6IrTypeGet(&module->types,id);
    if(!type || depth>module->types.count)return 0;
    if(type->kind==XL6_IR_TYPE_TUPLE && type->cAbi==XL6_IR_C_ABI_PRODUCT) {
        for(size_t n=0;n<type->paramCount;++n)
            if(!xl6IrCgenAnalyzeReleaseNeeds(module,type->params[n],needs,depth+1))return 0;
        return 1;
    }
    if(type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED)return 1;
    switch(type->cAbi) {
    case XL6_IR_C_ABI_MANAGED_STRING: needs->needsStringRuntime=1;return 1;
    case XL6_IR_C_ABI_C_STRING:
    case XL6_IR_C_ABI_C_BYTES: needs->needsCOwnedRuntime=1;return 1;
    case XL6_IR_C_ABI_MANAGED_BYTES:
        needs->needsBytesRuntime=needs->needsXValueRuntime=1;return 1;
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR: needs->needsXValueRuntime=1;return 1;
    case XL6_IR_C_ABI_RUNTIME_ERROR_PTR: needs->needsErrorRuntime=1;return 1;
    case XL6_IR_C_ABI_RUNTIME_FUTURE_PTR: needs->needsFutureRuntime=1;return 1;
    case XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR: needs->needsGeneratorRuntime=1;return 1;
    case XL6_IR_C_ABI_CLOSURE_PTR: needs->needsClosureRuntime=1;return 1;
    case XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR:
        needs->needsXValueIteratorRuntime=needs->needsXValueRuntime=needs->needsRuntimeTrap=1;return 1;
    case XL6_IR_C_ABI_NATIVE_VALUE: {
        const XL6_IrNativeValuePlan* plan=xl6IrNativeValuePlanForType(type);
        return plan && plan->dropSymbol;
    }
    default:return 0;
    }
}

#include "src/cgen/xl6_ir_cgen_needs_instructions.c"

static int xl6IrCgenAnalyzePrimaryNeeds(
    const XL6_IrModule* module, XL6_IrCgenNeeds* needs,
    XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
    for(i=0;i<module->types.count;++i)
        if(xl6IrCgenProductDropNeeded(module,(XL6_IrTypeId)(i+1)) &&
            !xl6IrCgenAnalyzeReleaseNeeds(module,(XL6_IrTypeId)(i+1),needs,0))
            return xl6IrCgenDiag(diags,module,XL6_IR_NODE_INVALID,
                "XLIR emitted product release support has no closed physical ABI");
    for (i = 0; i < module->callableBridgeCount; ++i) {
        const XL6_IrCallableBridge* bridge=&module->callableBridges[i];
        if (!xl6IrCgenRuntimeCallableAdapterNeeded(module,bridge->signatureType,0) &&
            !xl6IrCgenRuntimeCallableAdapterNeeded(module,bridge->signatureType,1)) continue;
        for (parameter=0;parameter<bridge->parameterCount;++parameter)
            if (module->callableBridgeParameters[bridge->firstParameter+parameter].decodeAbi==XL6_IR_CALLABLE_BRIDGE_FUTURE_COPY)
                needs->needsXValueFutureReader=needs->needsXValueFutureRuntime=needs->needsFutureRuntime=1;
        for (parameter=0;parameter<bridge->resultCount;++parameter)
            if (module->callableBridgeResults[bridge->firstResult+parameter].decodeAbi==XL6_IR_CALLABLE_BRIDGE_FUTURE_COPY)
                needs->needsXValueFutureReader=needs->needsXValueFutureRuntime=needs->needsFutureRuntime=1;
        for (parameter=0; parameter<bridge->parameterCount; ++parameter)
            if (module->callableBridgeParameters[bridge->firstParameter+parameter].decodeAbi==XL6_IR_CALLABLE_BRIDGE_CLOSURE_COPY) {
                needs->needsXValueClosureProjectionRuntime=needs->needsXValueClosureBridgeRuntime=1;
                needs->needsClosureRuntime=needs->needsXCallRuntime=needs->needsXValueRuntime=1;
                needs->needsXValueTypeIdentityRuntime=1;
            }
    }
    for ( i = 0; i < module->dynamicMethodCount; ++i ) {
        const XL6_IrDynamicMethod* method = &module->dynamicMethods[i];
        const XL6_IrCallTarget* target;
        if ( !xl6IrCgenDynamicMethodNeeded(module, i) ) continue;
        target = xl6IrCallTargetGet(module, method->targetSymbol);
        for(size_t p=0;p<method->parameterCount+method->scalarResultCodecCount;++p) {
            int argument=p<method->parameterCount;
            size_t index=argument ? module->dynamicMethodParameters[method->firstParameter+p].scalarCodec :
                method->firstScalarResultCodec+(p-method->parameterCount)+1;
            const XL6_IrCallableBridgeResult* codec=xl6IrDynamicMethodScalarCodecGet(module,index);
            if(!codec)continue;
            needs->needsXValueTypeIdentityRuntime=1;
            if(codec->nativeScalarProfile!=XL6_NATIVE_SCALAR_NONE) {
                needs->needsCallableNativeScalarCodec=1;
                if(argument)needs->needsDynamicNativeScalarAdmission=1;
            }
            if(codec->integerDomain.rule!=XL6_VALUE_ADMIT_NONE)needs->needsCallableIntegerCodec=1;
            if(codec->decodeAbi==XL6_IR_CALLABLE_BRIDGE_F32 && argument)needs->needsCallableF32Reader=1;
        }
        for ( parameter = 0; parameter < method->parameterCount; ++parameter ) {
            const XL6_IrType* type = xl6IrTypeGet(&module->types,
                module->dynamicMethodParameters[method->firstParameter + parameter].type);
            if ( type && type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR ) {
                needs->needsXValueClosureProjectionRuntime = needs->needsXValueClosureBridgeRuntime = 1;
                needs->needsClosureRuntime = needs->needsXCallRuntime = needs->needsXValueRuntime = 1;
            }
            if (type && type->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR)
                needs->needsXValueFutureReader=needs->needsFutureRuntime=needs->needsXValueRuntime=1;
        }
        const XL6_IrType* result = xl6IrTypeGet(&module->types, method->returnType);
        if ( result && result->cAbi == XL6_IR_C_ABI_PRODUCT ) {
            for ( size_t p = 0; p < result->paramCount; ++p ) {
                const XL6_IrType* field = xl6IrTypeGet(&module->types, result->params[p]);
                if ( field && field->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
                    needs->needsClosureRuntime = needs->needsXCallRuntime = needs->needsXValueRuntime = 1;
                if (field && field->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR)
                    needs->needsXValueFutureRuntime=needs->needsFutureRuntime=needs->needsXValueRuntime=1;
            }
        } else if ( result && result->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
            needs->needsClosureRuntime = needs->needsXCallRuntime = needs->needsXValueRuntime = 1;
        if (result && result->cAbi==XL6_IR_C_ABI_RUNTIME_FUTURE_PTR)
            needs->needsXValueFutureRuntime=needs->needsFutureRuntime=needs->needsXValueRuntime=1;
        if ( target->kind != XL6_IR_CALL_TARGET_EXTERN_C ) continue;
        if ( !xl6IrCgenRequireErrorResult(module, needs, method->returnType, method->declaration, diags) ) return 0;
        needs->needsForeignErrorRuntime = needs->needsErrorRuntime = 1;
        for ( parameter = 0; parameter < target->parameterCount; ++parameter )
            if ( target->parameterPhysicalAbis[parameter] == XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW )
                needs->needsXValueHandleRuntime = 1;
    }
    if (module->abi.taskPoolAbi == XL6_IR_MODULE_TASK_POOL_XRT_GUARDED_QUIESCE) {
        needs->needsAsyncTaskRuntime = 1;
        needs->needsRuntimeTrap = 1;
        needs->needsFutureRuntime = 1;
        needs->needsErrorRuntime = 1;
        needs->needsForeignErrorRuntime = 1;
        needs->needsResultVoid = 1;
    }
    for ( i = 0; i < module->exportCount; ++i )
        for ( parameter = 0; parameter < module->exports[i].parameterCount; ++parameter )
            if ( xl6IrExportValueAbiIsCallback(module->exports[i].parameterValueAbis[parameter]) ) {
                const XL6_IrType* signature = xl6IrTypeGet(&module->types,
                    xl6IrExportCallbackSignatureType(module, module->exports[i].parameterTypes[parameter]));
                const XL6_IrType* result = xl6IrTypeGet(&module->types, signature->callableResultType);
                size_t p;
                needs->needsRawCallbackRuntime = 1;
                needs->needsClosureRuntime = 1;
                needs->needsErrorRuntime = 1;
                if ( result != NULL && result->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR )
                    needs->needsXValueRuntime = 1;
                if ( result != NULL && result->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) {
                    needs->needsStringRuntime = 1;
                    needs->needsStringViews = 1;
                }
                for ( p = 0; p < signature->paramCount; ++p ) {
                    const XL6_IrType* item = xl6IrTypeGet(&module->types, signature->params[p]);
                    if ( item->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) needs->needsXValueRuntime = 1;
                    if ( item->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) {
                        needs->needsStringRuntime = 1;
                        needs->needsStringViews = 1;
                    }
                }
            }
    if (!xl6IrCgenAnalyzeInstructionNeeds(module,needs,diags)) return 0;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        if (module->callTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
            (module->callTargets[i].kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
             module->callTargets[i].kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR)) {
            /* Frame drops and pre-frame rollback preserve the first failure. */
            needs->needsErrorRuntime = 1;
            needs->needsForeignErrorRuntime = 1;
        }
        if ( module->callTargets[i].reachability != XL6_IR_REACHABILITY_UNREACHABLE &&
             module->callTargets[i].kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ) {
            const XL6_IrType* worker = xl6IrTypeGet(&module->types, module->callTargets[i].workerReturnType);
            if ( module->callTargets[i].asyncMachine < module->asyncMachineCount &&
                 module->asyncMachines[module->callTargets[i].asyncMachine].status ==
                    XL6_IR_ASYNC_MACHINE_ANALYZED ) {
                needs->needsAsyncMachineRuntime = 1;
                /* A direct machine callback always imports failed Future
                 * results into the canonical error channel before executing
                 * its verified resume CFG.  The original AWAIT may no longer
                 * be reachable from the ordinary synchronous block walk, so
                 * this dependency belongs to the machine admission itself. */
                needs->needsErrorRuntime = 1;
            }
            if ( module->callTargets[i].asyncDispatch == XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE ) {
                needs->needsAsyncReadyRuntime = 1;
                if ( module->callTargets[i].workerErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE )
                    needs->needsAsyncReadyErrorRuntime = 1;
            }
            if ( worker != NULL && worker->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ) {
                needs->needsAsyncXValueResult = 1;
                needs->needsXValueRuntime = 1;
            }
            if ( worker != NULL && worker->cAbi == XL6_IR_C_ABI_MANAGED_STRING ) {
                needs->needsAsyncStringResult = 1;
                needs->needsStringRuntime = 1;
            }
            if ( worker != NULL && worker->cAbi == XL6_IR_C_ABI_CLOSURE_PTR ) {
                needs->needsAsyncClosureResult = 1;
                needs->needsClosureRuntime = 1;
            }
            if ( worker != NULL && worker->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR ) {
                needs->needsAsyncErrorResult = 1;
                needs->needsErrorRuntime = 1;
            }
            if ( worker != NULL && worker->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR ) {
                needs->needsAsyncFutureResult = 1;
                needs->needsFutureRuntime = 1;
            }
            if ( worker != NULL && worker->cAbi == XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR )
                needs->needsGeneratorRuntime = 1;
            for ( parameter = 0; parameter < module->callTargets[i].parameterCount; ++parameter ) {
                if ( module->callTargets[i].asyncCaptureAbis[parameter] == XL6_IR_ASYNC_CAPTURE_COPY_STRING ) {
                    needs->needsStringRuntime = 1;
                } else if ( module->callTargets[i].asyncCaptureAbis[parameter] == XL6_IR_ASYNC_CAPTURE_RETAIN_XVALUE ) {
                    needs->needsXValueRuntime = 1;
                } else if ( module->callTargets[i].asyncCaptureAbis[parameter] == XL6_IR_ASYNC_CAPTURE_RETAIN_ERROR ) {
                    needs->needsErrorRuntime = 1;
                }
            }
            needs->needsFutureRuntime = 1;
        }
    }
    for ( i = 0; i < module->functionCount; ++i ) {
        const XL6_IrFunction* function = &module->functions[i];
        if ( function->reachability == XL6_IR_REACHABILITY_UNREACHABLE ||
             xl6IrCgenFunctionIsCanonicalMachineProgram(module, function) ||
             (function->kind != XL6_IR_FUNCTION_USER &&
              function->kind != XL6_IR_FUNCTION_ASYNC_WORKER &&
              function->kind != XL6_IR_FUNCTION_NAMED_ADAPTER &&
              function->kind != XL6_IR_FUNCTION_MODULE_FINI &&
              function->kind != XL6_IR_FUNCTION_CLASS_FINI &&
              function->kind != XL6_IR_FUNCTION_ARGUMENT_DEFAULT &&
              function->kind != XL6_IR_FUNCTION_CLASS_DEFAULT) ||
             function->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE ) continue;
        if ( !xl6IrCgenRequireErrorResult(module, needs, function->returnType,
                 function->declaration, diags) ) return 0;
    }
    for (i = 0; i < module->types.count; ++i) {
        const XL6_IrType* type = &module->types.items[i];
        if (type->persistenceRestoreCarrierAbi != XL6_IR_COLLECTION_CARRIER_SEQUENCE_V1 ||
            (!xl6IrCgenPersistenceTypeUsed(module, (XL6_IrTypeId)(i + 1), XL6_IR_PERSIST_FORMAT_JSON, 1) &&
             !xl6IrCgenPersistenceTypeUsed(module, (XL6_IrTypeId)(i + 1), XL6_IR_PERSIST_FORMAT_XLON, 1))) continue;
        needs->needsArrayCreateRuntime = needs->needsArrayImportRuntime = 1;
        needs->needsNominalIdentityRuntime = needs->needsXValueRuntime = 1;
        needs->needsXValueHandleRuntime = needs->needsErrorRuntime = 1;
    }
    /* The same verified closure selects both the literal declarations and
     * their outer emission gate, including restore-only seed factories. */
    for (i = 0; i < module->nominalIdentitySetCount; ++i) {
        if (xl6IrCgenNominalIdentitySetUsed(module, &module->nominalIdentitySets[i])) {
            needs->needsNominalIdentityRuntime = needs->needsXValueRuntime = 1;
            break;
        }
    }
    return 1;
}
