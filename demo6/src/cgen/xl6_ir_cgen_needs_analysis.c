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
static int xl6IrCgenAnalyzePrimaryNeeds(
    const XL6_IrModule* module, XL6_IrCgenNeeds* needs,
    XL6_DiagList* diags)
{
    size_t i;
    size_t parameter;
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
    for ( i = 0; i < module->instructionCount; ++i ) {
        const XL6_IrType* instructionType;
        if ( !xl6IrCgenInstructionIsReachable(module, i) ) continue;
        instructionType = xl6IrTypeGet(&module->types, module->instructions[i].type);
        if(module->instructions[i].asyncResultAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE ||
            module->instructions[i].sourceAsyncResultAbi==XL6_IR_ASYNC_RESULT_OWNED_NATIVE_VALUE)
            needs->needsNativeFuturePayloadRuntime=1;
        if ( module->instructions[i].asyncResultAbi ==
                 XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR ||
             module->instructions[i].sourceAsyncResultAbi ==
                 XL6_IR_ASYNC_RESULT_RETAINED_GENERATOR )
            needs->needsGeneratorRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_I64_POW )
            needs->needsI64PowRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_TIME_APPROX )
            needs->needsTimeApproxRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_WEAK_CREATE ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_WEAK_LOCK) ) {
            needs->needsXValueWeakRuntime = 1;
            needs->needsXValueRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenPersistenceRuntimeFormat(
                 module->instructions[i].runtimeOp) !=
                 XL6_IR_PERSIST_FORMAT_NONE ) {
            needs->needsPersistenceFormats |= xl6IrCgenPersistenceRuntimeFormat(
                module->instructions[i].runtimeOp);
            if (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_READ ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_READ)
                needs->needsPersistenceReadFormats |= xl6IrCgenPersistenceRuntimeFormat(module->instructions[i].runtimeOp);
            if (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_PARSE ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_PARSE ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_READ ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_READ)
                needs->needsPersistenceRestoreFormats |= xl6IrCgenPersistenceRuntimeFormat(module->instructions[i].runtimeOp);
            if (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_JSON_READ ||
                module->instructions[i].runtimeOp == XL6_IR_RUNTIME_PERSISTENCE_XLON_READ)
                needs->needsPersistenceFileFormats |= xl6IrCgenPersistenceRuntimeFormat(module->instructions[i].runtimeOp);
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
        }
        if ( instructionType != NULL &&
             instructionType->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR )
            needs->needsFutureRuntime = 1;
        if ( instructionType != NULL &&
             instructionType->cAbi == XL6_IR_C_ABI_RUNTIME_GENERATOR_PTR )
            needs->needsGeneratorRuntime = 1;
        if ( instructionType != NULL &&
             instructionType->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
             module->instructions[i].kind == XL6_IR_INSTR_RELEASE )
            needs->needsClosureRuntime = 1;
        if ( (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              (xl6IrCgenXValueIteratorRuntimeOpIs(module->instructions[i].runtimeOp) ||
               xl6IrSlotMapIteratorOpIs(module->instructions[i].runtimeOp))) ||
             ((module->instructions[i].kind == XL6_IR_INSTR_RELEASE ||
               module->instructions[i].kind == XL6_IR_INSTR_VALUE_RELEASE) &&
              instructionType != NULL && instructionType->cAbi ==
                  XL6_IR_C_ABI_RUNTIME_VALUE_ITERATOR_PTR) ) {
            needs->needsXValueIteratorRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_ITERATOR_ITEM_STRING_COPY ||
              module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_STRING_COPY) ) {
            needs->needsXValueIteratorStringRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_SAFE_CLASS_FIELD_GET_LITERAL ) {
            needs->needsXValueSafeClassFieldRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_STRING_COPY ) {
            needs->needsXValueIteratorNullableStringRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_TAGGED_NULLABLE_COPY ) {
            needs->needsXValueTaggedNullableRuntime = 1;
            needs->needsXValueRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_CLOSURE_COPY ) {
            needs->needsXValueIteratorNullableClosureRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueClosureBridgeRuntime = 1;
            needs->needsClosureRuntime = 1;
            needs->needsXCallRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_PROJECT_CLOSURE_COPY ) {
            needs->needsXValueClosureProjectionRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueClosureBridgeRuntime = 1;
            needs->needsClosureRuntime = 1;
            needs->needsXCallRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_CLOSURE_CAST_CHECKED ) {
            needs->needsXValueClosureCheckedCastRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueClosureBridgeRuntime = 1;
            needs->needsClosureRuntime = 1;
            needs->needsXCallRuntime = 1;
            needs->needsErrorRuntime = 1;
            if ( xl6IrCgenRuntimeCallableHasClosureResult(
                     module, module->instructions[i].calleeType) )
                needs->needsXValueClosureProjectionRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_NATIVE_HANDLE_BORROW ) {
            needs->needsXValueNativeHandleProjectionRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_XVALUE_RETAIN ) {
            needs->needsXValueNullableRetainProjectionRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_XVALUE_RETAIN ) {
            needs->needsXValueRetainProjectionRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_SCALAR_COPY ) {
            needs->needsXValueScalarProjectionRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_RAW_POINTER_BORROW ) {
            needs->needsXValueRawPointerProjectionRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_STRING_COPY ) {
            needs->needsXValueStringProjectionRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_CAST_CHECKED ) {
            needs->needsXValueCheckedCastRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsErrorRuntime = 1;
            if ( module->instructions[i].castAbi ==
                     XL6_IR_CAST_XVALUE_TO_ERROR_CHECKED )
                needs->needsXValueHandleRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_TAGGED ) {
            needs->needsXValueTaggedNullableRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_CAST_NULLABLE_STRING ) {
            needs->needsXValueNullableStringCastRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_NOMINAL_TEST ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_NULLABLE) ) {
            const XL6_IrNominalIdentitySet* set =
                xl6IrNominalIdentitySetGet(module,
                    module->instructions[i].payloadType, NULL);
            if ( set == NULL )
                return xl6IrCgenDiag(diags, module,
                    module->instructions[i].origin,
                    "XLIR C emitter nominal identity set is unresolved");
            needs->needsNominalIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            if ( module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_NOMINAL_CAST_CHECKED )
                needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_ITERATOR_KEY_DYNAMIC_XVALUE ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_ITERATOR_DYNAMIC_LOGICAL_XVALUE) ) {
            needs->needsXValueIteratorDynamicRuntime = 1;
            needs->needsStringViews = 1;
            if ( module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_ITERATOR_CREATE_CHECKED )
                needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_CAST ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_COLLECTION_CHECKED_REBIND) ) {
            needs->needsXValueCollectionCheckedRebindRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsErrorRuntime = 1;
            if (module->instructions[i].collectionAdmissionAbi==XL6_IR_COLLECTION_ADMISSION_NOMINAL)
                needs->needsNominalIdentityRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenCheckedIntegerRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            needs->needsCheckedIntegerArithmeticRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( instructionType != NULL &&
             (module->instructions[i].kind == XL6_IR_INSTR_RETAIN ||
              module->instructions[i].kind == XL6_IR_INSTR_PRODUCT_DROP ||
              module->instructions[i].kind == XL6_IR_INSTR_VALUE_RELEASE) ) {
            if ( instructionType->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
                needs->needsStringRuntime = 1;
            else if ( instructionType->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR )
                needs->needsXValueRuntime = 1;
            else if ( instructionType->cAbi == XL6_IR_C_ABI_RUNTIME_ERROR_PTR )
                needs->needsErrorRuntime = 1;
            else if ( instructionType->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR )
                needs->needsFutureRuntime = 1;
            else if ( instructionType->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
                needs->needsClosureRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_UNION_BOX ||
              module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_UNION_UNBOX ||
              module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_PROJECT_NULLABLE_UNION_COPY ||
              module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED) ) {
            needs->needsXValueUnionRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            if ( module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_UNION_CAST_CHECKED ) {
                needs->needsXValueUnionCheckedCastRuntime = 1;
                needs->needsErrorRuntime = 1;
            } else {
                needs->needsRuntimeTrap = 1;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_FOREIGN_SCALAR ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_ERROR_REF) ) {
            needs->needsXValueRuntime = 1;
            if ( module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_ERROR_REF ) {
                /* The native scalar provider binds identity internally. Only
                 * the older error projection needs a generated bind helper. */
                needs->needsXValueTypeIdentityRuntime = 1;
                needs->needsRuntimeTrap = 1;
                needs->needsXValueErrorBoxRuntime = 1;
                needs->needsXValueHandleRuntime = 1;
                needs->needsErrorRuntime = 1;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_PROJECT_ERROR_REF ) {
            needs->needsXValueErrorProjectionRuntime = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsErrorRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              (module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_TYPE_BIND ||
               module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_TYPE_REBIND_OWNED ||
               module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_TYPE_ID) ) {
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_TRAP_IF_FALSE ) needs->needsRuntimeTrap = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_VIRTUAL_CALL ||
             module->instructions[i].kind == XL6_IR_INSTR_PROTOCOL_CALL ) {
            needs->needsRuntimeTrap = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsXValueObjectLiteralI64Runtime = 1;
            if (xl6IrCallInstructionErrorAbi(module, &module->instructions[i]) == XL6_IR_ERROR_ABI_MAY_RAISE &&
                !xl6IrCgenRequireErrorResult(module, needs, module->instructions[i].type,
                    module->instructions[i].origin, diags)) return 0;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_IS ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CLASS_AS) ) {
            needs->needsRuntimeTrap = 1;
            needs->needsXValueRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsXValueObjectRuntime = 1;
            needs->needsXValueObjectLiteralI64Runtime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_CALLABLE_FROM_CLOSURE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_NULLABLE_CLOSURE_BOX ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_SET_LITERAL_CLOSURE ||
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE) ) {
            needs->needsXCallRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsErrorRuntime = 1;
            needs->needsStringViews = 1;
            if ( module->instructions[i].runtimeOp !=
                 XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL )
                needs->needsClosureRuntime = 1;
            if ( module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE )
                needs->needsRuntimeTrap = 1;
            if ( module->instructions[i].runtimeOp !=
                     XL6_IR_RUNTIME_XVALUE_DYNAMIC_CALL &&
                 module->instructions[i].runtimeOp !=
                     XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_CLOSURE &&
                 xl6IrTypeGet(&module->types,
                     module->instructions[i].calleeType) != NULL &&
                 xl6IrTypeGet(&module->types,
                     module->instructions[i].calleeType)->callableErasureAbi ==
                     XL6_IR_CALLABLE_ERASURE_DICT_VARIADIC )
                needs->needsXValueObjectRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_DYNAMIC_METHOD_CALL ) {
            needs->needsDynamicMethodRuntime = 1;
            if (module->instructions[i].dynamicCallSite < module->dynamicCallSiteCount &&
                (module->dynamicCallSites[module->instructions[i].dynamicCallSite].kind == XL6_IR_DYNAMIC_CALL_MEMBER_GET ||
                 module->dynamicCallSites[module->instructions[i].dynamicCallSite].kind == XL6_IR_DYNAMIC_CALL_MEMBER_SET)) {
                needs->namedValueOperations|=module->dynamicCallSites[module->instructions[i].dynamicCallSite].kind==
                    XL6_IR_DYNAMIC_CALL_MEMBER_GET ? 1u : 2u;
                if (module->dynamicCallSites[module->instructions[i].dynamicCallSite].memberBindingAbi ==
                        XL6_IR_DYNAMIC_MEMBER_BINDING_RETAIN_RECEIVER) {
                    needs->needsXValueDynamicFieldRuntime = 1;
                    needs->needsXCallRuntime = 1;
                    needs->needsClosureRuntime = 1;
                    needs->needsBoundMethodRuntime = 1;
                }
            }
            needs->needsXValueRuntime = 1;
            needs->needsErrorRuntime = 1;
            needs->needsStringViews = 1;
            needs->needsStringRuntime = 1;
            needs->needsXValueArrayRuntime = 1;
            needs->needsXValueObjectRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_NULLABLE_STRING_BOX ) {
            needs->needsXValueRuntime = 1;
            needs->needsStringRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_FUTURE_REF ) {
            needs->needsXValueFutureRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsFutureRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                XL6_IR_RUNTIME_XVALUE_NATIVE_HANDLE_TAKE ) {
            needs->needsNativeHandleTakeRuntime = 1;
            {
                const XL6_IrCallTarget* destroy = xl6IrCallTargetGet(
                    module, module->instructions[i].targetSymbol);
                if (destroy != NULL && destroy->nativeHandlePolicyAbi ==
                        XL6_IR_NATIVE_HANDLE_POLICY_RESIDENT_V1)
                    needs->needsNativeHandlePolicyRuntime = 1;
            }
            needs->needsForeignErrorRuntime = 1;
            needs->needsErrorRuntime = 1;
            needs->needsRuntimeTrap = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenThreadRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            needs->needsThreadResourceRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsClosureRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenChannelRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            if (!xl6IrChannelInstructionCompatible(module,&module->instructions[i])) return 0;
            needs->needsNativeChannelRuntime = 1;
            XL6_IrTypeId channelId=xl6IrChannelInstructionType(module,&module->instructions[i]);
            const XL6_IrType* channel=xl6IrTypeGet(&module->types,channelId);
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_CHANNEL_CREATE &&
                !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,channel->messageSchemaType,needs,0)) return 0;
            needs->needsForeignErrorRuntime=1;
            needs->needsNominalIdentityRuntime=1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenSpecialQueueRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            XL6_IrTypeId queueId=xl6IrConcurrentQueueInstructionType(module,&module->instructions[i]);
            if (queueId) {
                const XL6_IrType* queue=xl6IrTypeGet(&module->types,queueId);
                if (queue->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE) needs->needsSpscQueueRuntime=1;
                else if (queue->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_MPSC_QUEUE) needs->needsMpscQueueRuntime=1;
                else if(queue->runtimeResourceAbi==XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE)needs->needsWaitQueueRuntime=1;
                else needs->needsMpmcQueueRuntime=1;
                const XL6_IrType* schema=xl6IrTypeGet(&module->types,queue->messageSchemaType);
                if (!schema) return 0;
                if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE &&
                    !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,queue->messageSchemaType,needs,0)) return 0;
                if (schema->sequenceLayout.codecAbi==XL6_IR_SEQUENCE_CODEC_CLOSURE_VALUE_V1) {
                    needs->needsXValueClosureBridgeRuntime=1;
                    needs->needsXValueClosureProjectionRuntime=1;
                    needs->needsClosureRuntime=1;
                    needs->needsXCallRuntime=1;
                }
                needs->needsForeignErrorRuntime=1;
                needs->needsNominalIdentityRuntime=1;
            } else return 0; /* No legacy generated queue fallback. */
            needs->needsXValueRuntime = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_LIST_CREATE ||
             module->instructions[i].runtimeOp==XL6_IR_RUNTIME_DICT_CREATE ||
             module->instructions[i].runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
             module->instructions[i].runtimeOp==XL6_IR_RUNTIME_XVALUE_SET_NEW ||
             module->instructions[i].runtimeOp==XL6_IR_RUNTIME_SET_IMPORT_CHAIN ||
             module->instructions[i].runtimeOp==XL6_IR_RUNTIME_DICT_IMPORT_CHAIN)) {
            XL6_IrValueCollectionRecipe recipe={0};
            if (!xl6IrValueCollectionConstructionRecipe(module,module->instructions[i].type,&recipe) ||
                !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,recipe.valueSchemaType,needs,0)) return 0;
            if (recipe.family==XL6_IR_COLLECTION_LIST) needs->needsListCreateRuntime=1;
            else if (recipe.family==XL6_IR_COLLECTION_SET) needs->needsSetCreateRuntime=1;
            else if (recipe.family==XL6_IR_COLLECTION_DICT) needs->needsDictCreateRuntime=1;
            else return 0;
            needs->needsArrayCreateRuntime=1; /* Exact resident value-schema projection. */
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_LIST_IMPORT_CHAIN ||
                module->instructions[i].runtimeOp==XL6_IR_RUNTIME_SET_IMPORT_CHAIN ||
                module->instructions[i].runtimeOp==XL6_IR_RUNTIME_DICT_IMPORT_CHAIN) {
                XL6_IrTypeId chain[65]; size_t count=0;
                if (!xl6IrCollectionImportChainRecipe(module,module->instructions[i].type,chain,65,&count)) return 0;
                for (size_t j=0;j<count;++j) {
                    XL6_IrCollectionAbi family=xl6IrTypeGet(&module->types,chain[j])->collectionAbi;
                    if (family==XL6_IR_COLLECTION_LIST) needs->needsListImportRuntime=1;
                    if (family==XL6_IR_COLLECTION_SET) needs->needsSetImportRuntime=1;
                    if (family==XL6_IR_COLLECTION_DICT) needs->needsDictImportRuntime=1;
                }
                needs->needsArrayImportRuntime=1;
            }
            needs->needsXValueRuntime=1;
            needs->needsXValueHandleRuntime=1;
            needs->needsErrorRuntime=1;
            needs->needsForeignErrorRuntime=1;
            needs->needsNominalIdentityRuntime=1;
        }
        if (module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_ARRAY_IMPORT_CHAIN ||
             module->instructions[i].runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE)) {
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_ARRAY_CREATE) needs->needsArrayCreateRuntime=1;
            else needs->needsArrayImportRuntime=1;
            needs->needsXValueRuntime=1;
            needs->needsXValueHandleRuntime=1;
            needs->needsErrorRuntime=1;
            /* Descendant nominal recipes are needed even when the root is a
             * Sequence-reference profile. Never limit this to the root set. */
            needs->needsNominalIdentityRuntime=1;
            if (!xl6IrCgenAnalyzeSequenceRecipeNeeds(module,module->instructions[i].type,needs,0)) return 0;
        }
        if(module->instructions[i].kind==XL6_IR_INSTR_RUNTIME_CALL &&
            xl6IrSlotMapResourceOpIs(xl6IrSlotMapResourceOp(module->instructions[i].runtimeOp))) {
            needs->needsArrayCreateRuntime=needs->needsNominalIdentityRuntime=needs->needsXValueRuntime=1;
            needs->needsXValueHandleRuntime=needs->needsXValueTypeIdentityRuntime=needs->needsErrorRuntime=1;
            if(module->instructions[i].runtimeOp==XL6_IR_RUNTIME_SLOT_MAP_CREATE) {
                XL6_IrValueCollectionRecipe plan={0};
                if(!xl6IrSlotMapConstructionRecipe(module,module->instructions[i].type,&plan) ||
                    !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,plan.valueSchemaType,needs,0)) return 0;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenStackRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            needs->needsStackResourceRuntime = 1;
            const XL6_IrInstruction* instruction=&module->instructions[i];
            const XL6_IrType* payload=xl6IrTypeGet(&module->types,instruction->payloadType);
            if(payload && payload->cAbi==XL6_IR_C_ABI_CLOSURE_PTR) {
                needs->needsClosureRuntime=needs->needsXCallRuntime=needs->needsXValueClosureBridgeRuntime=1;
                needs->needsForeignErrorRuntime=1;
                if(instruction->runtimeOp==XL6_IR_RUNTIME_STACK_POP || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_STACK_PEEK_AT || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_POP ||
                    instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK || instruction->runtimeOp==XL6_IR_RUNTIME_STACK_TRY_PEEK_AT)
                    needs->needsStackCallableRuntime=1;
            }
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_STACK_CREATE &&
                !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,module->instructions[i].type,needs,0)) return 0;
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_STACK_CREATE &&
                module->instructions[i].sequenceAdmissionIdentitySet!=XL6_IR_NOMINAL_IDENTITY_SET_INVALID)
                needs->needsNominalIdentityRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenAvlRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            needs->needsAvlTreeResourceRuntime = 1;
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_AVL_TREE_CREATE) {
                XL6_IrTypeId key=0,value=0;
                if (!xl6IrAvlConstructionRecipe(module,module->instructions[i].type,&key,&value,NULL,NULL) ||
                    !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,key,needs,0) ||
                    !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,value,needs,0)) return 0;
            } else if ((module->instructions[i].runtimeOp==XL6_IR_RUNTIME_AVL_TREE_KEYS ||
                        module->instructions[i].runtimeOp==XL6_IR_RUNTIME_AVL_TREE_VALUES) &&
                !xl6IrCgenAnalyzeSequenceRecipeNeeds(module,module->instructions[i].type,needs,0)) return 0;
            needs->needsForeignErrorRuntime = 1;
            needs->needsXValueRuntime = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsXValueArrayRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenSyncRuntimeOpIs(
                 module->instructions[i].runtimeOp) ) {
            if (xl6IrCgenSyncResourceAbi(module->instructions[i].runtimeOp)==XL6_IR_RUNTIME_RESOURCE_SEM) {
                needs->needsSemaphoreResourceRuntime = 1;
                needs->needsForeignErrorRuntime = 1;
            } else {
                needs->needsSyncResourceRuntime = 1;
                needs->needsForeignErrorRuntime = 1;
            }
            needs->needsXValueRuntime = 1;
            needs->needsXValueHandleRuntime = 1;
            needs->needsXValueTypeIdentityRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_FUTURE_CANCEL ) {
            needs->needsFutureCancelRuntime = 1;
            needs->needsFutureRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             xl6IrCgenFutureMemberHelper(
                 module->instructions[i].runtimeOp) != NULL ) {
            needs->needsFutureMemberRuntime |= xl6IrCgenFutureMemberMask(
                module->instructions[i].runtimeOp);
            needs->needsFutureRuntime = 1;
            if ( module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_FUTURE_DEBUG_NAME_COPY )
                needs->needsStringRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_TASK_SPAWN_CLOSURE ) {
            const XL6_IrType* signature = xl6IrTypeGet(
                &module->types, module->instructions[i].calleeType);
            size_t parameter;
            needs->needsTaskSpawnRuntime = 1;
            needs->needsAsyncTaskRuntime = 1;
            needs->needsFutureRuntime = 1;
            needs->needsClosureRuntime = 1;
            needs->needsErrorRuntime = 1;
            if ( signature != NULL ) {
                for ( parameter = 0;
                      parameter < signature->paramCount; ++parameter ) {
                    const XL6_IrType* type = xl6IrTypeGet(
                        &module->types, signature->params[parameter]);
                    if ( type == NULL ) continue;
                    if ( type->cAbi == XL6_IR_C_ABI_MANAGED_STRING )
                        needs->needsStringRuntime = 1;
                    else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR ||
                              type->cAbi == XL6_IR_C_ABI_MANAGED_BYTES )
                        needs->needsXValueRuntime = 1;
                    else if ( type->cAbi == XL6_IR_C_ABI_RUNTIME_FUTURE_PTR )
                        needs->needsFutureRuntime = 1;
                    else if ( type->cAbi == XL6_IR_C_ABI_CLOSURE_PTR )
                        needs->needsClosureRuntime = 1;
                }
            }
            if ( module->instructions[i].asyncResultAbi ==
                 XL6_IR_ASYNC_RESULT_OWNED_STRING ) {
                needs->needsAsyncStringResult = 1;
                needs->needsStringRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi ==
                        XL6_IR_ASYNC_RESULT_RETAINED_XVALUE ) {
                needs->needsAsyncXValueResult = 1;
                needs->needsXValueRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi ==
                        XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE ) {
                needs->needsAsyncClosureResult = 1;
                needs->needsClosureRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi ==
                        XL6_IR_ASYNC_RESULT_RETAINED_ERROR ) {
                needs->needsAsyncErrorResult = 1;
                needs->needsErrorRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi ==
                        XL6_IR_ASYNC_RESULT_RETAINED_FUTURE ) {
                needs->needsAsyncFutureResult = 1;
                needs->needsFutureRuntime = 1;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_FUTURE_CONTINUE_CLOSURE ) {
            XL6_IrAsyncResultAbi abis[2] = {
                module->instructions[i].sourceAsyncResultAbi,
                module->instructions[i].asyncResultAbi
            };
            size_t abiIndex;
            needs->needsFutureContinuationRuntime = 1;
            needs->needsFutureRuntime = 1;
            needs->needsClosureRuntime = 1;
            needs->needsErrorRuntime = 1;
            for ( abiIndex = 0; abiIndex < 2; ++abiIndex ) {
                if ( abis[abiIndex] == XL6_IR_ASYNC_RESULT_OWNED_STRING ) {
                    needs->needsAsyncStringResult = 1;
                    needs->needsStringRuntime = 1;
                } else if ( abis[abiIndex] ==
                            XL6_IR_ASYNC_RESULT_RETAINED_XVALUE ) {
                    needs->needsAsyncXValueResult = 1;
                    needs->needsXValueRuntime = 1;
                } else if ( abis[abiIndex] ==
                            XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE ) {
                    needs->needsAsyncClosureResult = 1;
                    needs->needsClosureRuntime = 1;
                } else if ( abis[abiIndex] ==
                            XL6_IR_ASYNC_RESULT_RETAINED_ERROR ) {
                    needs->needsAsyncErrorResult = 1;
                    needs->needsErrorRuntime = 1;
                } else if ( abis[abiIndex] ==
                            XL6_IR_ASYNC_RESULT_RETAINED_FUTURE ) {
                    needs->needsAsyncFutureResult = 1;
                    needs->needsFutureRuntime = 1;
                }
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_FUTURE_RACE ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_FUTURE_RACE_ARRAY ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY) ) {
            needs->needsFutureRuntime = 1;
            needs->needsErrorRuntime = 1;
            if ( module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_FUTURE_ALL_XVALUES ||
                 module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY ) {
                needs->needsFutureAggregateAll = 1;
                needs->needsXValueRuntime = 1;
                if ( module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_FUTURE_ALL_XVALUES_ARRAY )
                    needs->needsFutureAggregateAllArray = 1;
                if ( module->instructions[i].asyncResultAbi ==
                     XL6_IR_ASYNC_RESULT_OWNED_STRING ) {
                    needs->needsStringRuntime = 1;
                    needs->needsStringViews = 1;
                } else if ( module->instructions[i].asyncResultAbi ==
                            XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE ) {
                    needs->needsClosureRuntime = 1;
                    needs->needsXCallRuntime = 1;
                } else if ( module->instructions[i].asyncResultAbi ==
                            XL6_IR_ASYNC_RESULT_RETAINED_ERROR ) {
                    needs->needsXValueErrorBoxRuntime = 1;
                    needs->needsXValueHandleRuntime = 1;
                    needs->needsXValueTypeIdentityRuntime = 1;
                } else if ( module->instructions[i].asyncResultAbi ==
                            XL6_IR_ASYNC_RESULT_RETAINED_FUTURE ) {
                    needs->needsXValueFutureRuntime = 1;
                }
            } else if ( module->instructions[i].runtimeOp ==
                            XL6_IR_RUNTIME_FUTURE_RACE ||
                        module->instructions[i].runtimeOp ==
                            XL6_IR_RUNTIME_FUTURE_RACE_ARRAY ) {
                needs->needsFutureAggregateRace = 1;
                if ( module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_FUTURE_RACE_ARRAY ) {
                    needs->needsFutureAggregateRaceArray = 1;
                    needs->needsXValueRuntime = 1;
                }
            } else {
                needs->needsFutureAggregateWhenAny = 1;
                if ( module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_FUTURE_WHEN_ANY_INDEX_ARRAY ) {
                    needs->needsFutureAggregateWhenAnyArray = 1;
                    needs->needsXValueRuntime = 1;
                }
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_I64 ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_U64 ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_BOOL ||
                 module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_F64 ||
                 module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TIME ||
                 module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_COUNT_TAGGED_NULLABLE ||
                 module->instructions[i].runtimeOp ==
                     XL6_IR_RUNTIME_XVALUE_COUNT_OR_NULL ||
                  module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_RAW_POINTER ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_POINTER_BORROW ||
              module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_NATIVE_HANDLE_BORROW ||
              module->instructions[i].runtimeOp ==
                   XL6_IR_RUNTIME_XVALUE_OBJECT_TAKE_LITERAL_NATIVE_HANDLE ||
              xl6IrCgenTypedDictLiteralRuntimeOpIs(
                  module->instructions[i].runtimeOp)) )
            needs->needsRuntimeTrap = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX ||
              module->instructions[i].runtimeOp ==
                  XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE) ) {
            needs->needsXValueTaggedNullableRuntime = 1;
            needs->needsXValueRuntime = 1;
            if (module->instructions[i].runtimeOp==XL6_IR_RUNTIME_XVALUE_TAGGED_NULLABLE_BOX) {
                const XL6_IrInstruction* source=xl6IrCgenValueDefinition(module,
                    module->operands[module->instructions[i].firstOperand],NULL);
                if (source && xl6IrNullableUnionElementType(module,source->type)) {
                    needs->needsXValueUnionRuntime=1;
                    needs->needsXValueTypeIdentityRuntime=1;
                    needs->needsRuntimeTrap=1;
                }
            }
            if ( module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_OBJECT_GET_LITERAL_TAGGED_NULLABLE ) {
                needs->needsXValueObjectRuntime = 1;
                needs->needsStringViews = 1;
                needs->needsRuntimeTrap = 1;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_CELL_NEW ||
             module->instructions[i].kind == XL6_IR_INSTR_CELL_INIT ||
             module->instructions[i].kind == XL6_IR_INSTR_CELL_LOAD ||
             module->instructions[i].kind == XL6_IR_INSTR_CELL_STORE ||
             module->instructions[i].kind == XL6_IR_INSTR_CELL_EXCHANGE ||
             module->instructions[i].kind == XL6_IR_INSTR_CELL_RETAIN ||
             module->instructions[i].kind == XL6_IR_INSTR_CELL_RELEASE ) {
            needs->needsCellRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
        if (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
            xl6IrCallErrorPhysicalUsesForeignScope(xl6IrCallInstructionErrorPhysicalAbi(module, &module->instructions[i]))) {
            needs->needsForeignErrorRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        /* ERROR_TEST itself emits xrtGetError().  Its declaration dependency
         * follows the lowered instruction directly; it must not depend on an
         * unrelated runtime helper happening to request the error API. */
        if ( module->instructions[i].kind == XL6_IR_INSTR_ERROR_TEST )
            needs->needsErrorRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_ERROR_RAISE ||
             module->instructions[i].kind == XL6_IR_INSTR_ERROR_TAKE ||
             module->instructions[i].kind == XL6_IR_INSTR_ERROR_SET_TAKE ||
              module->instructions[i].kind == XL6_IR_INSTR_ERROR_CLEAR ||
              (module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
              (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_TO_F64_CHECKED ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED ||
               module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED)) )
            needs->needsErrorRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             (module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_MESSAGE_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_CODE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_DOMAIN_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_OPERATION_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_DATA_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_SYSTEM_CODE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_CAUSE_REF ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_KIND_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_KIND_IS ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_FILE_COPY ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_LINE ||
              module->instructions[i].runtimeOp == XL6_IR_RUNTIME_ERROR_COLUMN) )
            needs->needsErrorRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_STRING_TO_F64_CHECKED )
            needs->needsStringF64ConvertRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_STRING_TO_TIME_CHECKED ) {
            needs->needsStringTimeConvertRuntime = 1;
            needs->needsStringViews = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_XVALUE_TO_TIME_CHECKED ) {
            needs->needsXValueTimeConvertRuntime = 1;
            needs->needsXValueRuntime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_XVALUE_TIME )
            needs->needsXValueRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED ) {
            if ( module->instructions[i].integerTarget ==
                     XL6_IR_INTEGER_TARGET_UNICODE_SCALAR )
                needs->needsCheckedCharRuntime = 1;
            else if ( module->instructions[i].integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_64 )
                needs->needsCheckedU64Runtime = 1;
            else
                needs->needsCheckedI64Runtime = 1;
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp == XL6_IR_RUNTIME_INTEGER_CONVERT_CHECKED ) {
            const XL6_IrInstruction* conversion = &module->instructions[i];
            if ( conversion->integerSourceAbi == XL6_IR_INTEGER_SOURCE_XVALUE ) {
                needs->needsXValueIntegerConvertRuntime = 1;
                needs->needsXValueRuntime = 1;
                needs->needsStringViews = 1;
                /* The shared dynamic decoder emits exact signed and unsigned
                 * projections; keep both range helper families available. */
                needs->needsCheckedI64Runtime = 1;
            } else if ( conversion->integerSourceAbi ==
                        XL6_IR_INTEGER_SOURCE_MANAGED_STRING ) {
                if ( conversion->integerTarget == XL6_IR_INTEGER_TARGET_UNSIGNED_64 )
                    needs->needsStringU64ConvertRuntime = 1;
                else
                    needs->needsStringI64ConvertRuntime = 1;
            }
        }
        if ( module->instructions[i].kind == XL6_IR_INSTR_RUNTIME_CALL &&
             module->instructions[i].runtimeOp ==
                 XL6_IR_RUNTIME_INTEGER_CONVERT_WRAPPING &&
             module->instructions[i].integerTarget !=
                 XL6_IR_INTEGER_TARGET_UNSIGNED_64 )
            needs->needsWrappingIntegerRuntime = 1;
        if ( module->instructions[i].kind == XL6_IR_INSTR_AWAIT ) {
            needs->needsErrorRuntime = 1;
            needs->needsFutureRuntime = 1;
            needs->needsFutureAwaitRuntime = 1;
            if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID ) needs->needsFutureAwaitVoid = 1;
            else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_BOOL ) needs->needsFutureAwaitBool = 1;
            else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_CHAR ) needs->needsFutureAwaitChar = 1;
            else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_I64 ) needs->needsFutureAwaitI64 = 1;
            else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_U64 ) needs->needsFutureAwaitU64 = 1;
            else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_TYPE_ID ) needs->needsFutureAwaitTypeId = 1;
            else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_F64 ) needs->needsFutureAwaitF64 = 1;
            else if ( module->instructions[i].asyncResultAbi ==
                      XL6_IR_ASYNC_RESULT_COPY_RAW_POINTER )
                needs->needsFutureAwaitRawPointer = 1;
            else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_OWNED_STRING ) {
                needs->needsFutureAwaitString = 1;
                needs->needsStringRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_XVALUE ) {
                needs->needsFutureAwaitXValue = 1;
                needs->needsXValueRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_CLOSURE ) {
                needs->needsFutureAwaitClosure = 1;
                needs->needsClosureRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_ERROR ) {
                needs->needsFutureAwaitError = 1;
                needs->needsAsyncErrorResult = 1;
                needs->needsErrorRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_RETAINED_FUTURE ) {
                needs->needsFutureAwaitFuture = 1;
                needs->needsAsyncFutureResult = 1;
                needs->needsFutureRuntime = 1;
            } else if ( module->instructions[i].asyncResultAbi == XL6_IR_ASYNC_RESULT_COPY_TAGGED )
                needs->needsFutureAwaitTagged = 1;
        }
    }
    for (i = 0; i < module->instructionCount; ++i)
        if (module->instructions[i].kind == XL6_IR_INSTR_ERROR_UNWIND_BEGIN ||
            module->instructions[i].kind == XL6_IR_INSTR_CLEANUP_BEGIN ||
            module->instructions[i].kind == XL6_IR_INSTR_CLEANUP_INHERIT_BEGIN) {
            needs->needsForeignErrorRuntime = 1;
            needs->needsErrorRuntime = 1;
            needs->needsRuntimeTrap = 1;
        }
    if (module->exportCount != 0) {
        needs->needsForeignErrorRuntime = 1;
        needs->needsErrorRuntime = 1;
    }
    if ( !xl6IrCgenAnalyzeCallTargetNeeds(module, needs, diags) )
        return 0;
    if ( needs->needsWebSocketStreamTlsRuntime )
        needs->needsWebSocketStreamRuntime = 1;
    if ( needs->needsWebSocketStreamRuntime ) {
        needs->needsWebSocketStreamConfigRuntime = 1;
        needs->needsWebSocketUpgradeRuntime = 1;
    }
    if ( needs->needsWebSocketUpgradeRuntime ) {
        needs->needsWebSocketHandshakeRuntime = 1;
        needs->needsHttp1Runtime = 1;
    }
    if ( needs->needsHttp1Runtime ) {
        needs->needsBufferBridgeRuntime = 1;
        needs->needsErrorRuntime = 1;
    }
    if ( needs->needsNetTcpRuntime || needs->needsNetDatagramRuntime ) {
        needs->needsBufferBridgeRuntime = 1;
        needs->needsStringRuntime = 1;
        needs->needsStringViews = 1;
        needs->needsXValueRuntime = 1;
        needs->needsXValueObjectRuntime = 1;
        needs->needsXValueClassRuntime = 1;
        needs->needsXValueTypeIdentityRuntime = 1;
        needs->needsFutureRuntime = 1;
        needs->needsErrorRuntime = 1;
    }
    if ( needs->needsNetTlsRuntime ) {
        needs->needsBufferBridgeRuntime = 1;
        needs->needsStringRuntime = 1;
        needs->needsStringViews = 1;
        needs->needsXValueRuntime = 1;
        needs->needsXValueObjectRuntime = 1;
        needs->needsXValueClassRuntime = 1;
        needs->needsXValueTypeIdentityRuntime = 1;
        needs->needsFutureRuntime = 1;
        needs->needsErrorRuntime = 1;
    }
    for ( i = 0; i < module->protocolDispatchImplCount; ++i ) {
        const XL6_IrProtocolDispatchImpl* impl =
            &module->protocolDispatchImpls[i];
        const XL6_IrProtocolDispatchSlot* slot =
            impl->slot < module->protocolDispatchSlotCount
                ? &module->protocolDispatchSlots[impl->slot] : NULL;
        const XL6_IrCallTarget* target =
            xl6IrCallTargetGet(module, impl->implementationSymbol);
        const XL6_IrClassField* field;
        if ( slot == NULL ||
             !xl6IrCgenProtocolDispatchSlotIsReachable(module, impl->slot) ||
             target == NULL || target->kind != XL6_IR_CALL_TARGET_EXTERN_C )
            continue;
        field = xl6IrCgenProtocolExternReceiverField(module, slot, target);
        if ( field == NULL )
            return xl6IrCgenDiag(diags, module, impl->implementationDeclaration,
                "XLIR protocol extern witness adapter feature set is unresolved");
        needs->needsXValueRuntime = 1;
        needs->needsXValueObjectRuntime = 1;
        needs->needsStringViews = 1;
        needs->needsRuntimeTrap = 1;
        if ( field->nativeDestroySymbol != 0 )
            needs->needsXValueObjectLiteralNativeHandleRuntime = 1;
        else
            needs->needsXValueObjectLiteralRawPointerRuntime = 1;
    }
    for ( i = 0; i < module->closureTargetCount; ++i ) {
        const XL6_IrClosureTarget* closure = &module->closureTargets[i];
        if ( closure->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) continue;
        needs->needsClosureRuntime = 1;
        if ( closure->rawCallbackAbi == XL6_IR_CLOSURE_RAW_CALLBACK_FOREIGN_SCOPE_THUNK ) {
            needs->needsForeignErrorRuntime = 1;
            needs->needsErrorRuntime = 1;
        }
        for ( parameter = 0; parameter < closure->captureCount; ++parameter ) {
            const XL6_IrClosureCapture* capture =
                &module->closureCaptures[closure->firstCapture + parameter];
            if ( capture->abi == XL6_IR_CLOSURE_CAPTURE_COPY_STRING ||
                 capture->abi == XL6_IR_CLOSURE_CAPTURE_COPY_NULLABLE_STRING )
                needs->needsStringRuntime = 1;
            else if ( capture->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_XVALUE )
                needs->needsXValueRuntime = 1;
            else if ( capture->abi == XL6_IR_CLOSURE_CAPTURE_RETAIN_FUTURE )
                needs->needsFutureRuntime = 1;
            else if(capture->abi==XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE){
                needs->needsNativeCaptureProfileHeaders=1;
                needs->needsForeignErrorRuntime=1;
            }
        }
    }
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
            if ( module->callTargets[i].asyncDispatch ==
                     XL6_IR_ASYNC_DISPATCH_XRT_MODULE_TASK_POOL &&
                 xl6IrCgenAsyncMachine(module, &module->callTargets[i]) == NULL )
                needs->needsAsyncTaskRuntime = 1;
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
