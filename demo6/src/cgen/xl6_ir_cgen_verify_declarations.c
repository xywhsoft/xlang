/* Validate lowered declaration, linkage, storage and callable ABIs. */
static int xl6IrCgenVerifyDeclarations(
    const XL6_IrModule* module, XL6_DiagList* diags)
{
    size_t i;
    for ( i = 0; i < module->callTargetCount; ++i ) {
        const XL6_IrCallTarget* target = &module->callTargets[i];
        size_t parameter;
        if ( target->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) continue;
        if ( target->resultConversionAbi !=
                 xl6IrCallResultConversionForTarget(module, target) )
            return xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR C emitter foreign result ownership conversion ABI is unresolved");
        if ( target->variadicAbi < XL6_IR_CALL_VARIADIC_NONE ||
             target->variadicAbi > XL6_IR_CALL_VARIADIC_C ||
             (target->variadicAbi == XL6_IR_CALL_VARIADIC_C &&
              (target->kind != XL6_IR_CALL_TARGET_EXTERN_C || target->parameterCount == 0)) )
            return xl6IrCgenDiag(diags, module, target->declaration,
                                 "XLIR C emitter call target has no canonical variadic ABI");
        if ( target->returnType != XL6_IR_TYPE_INVALID && xl6IrCgenType(module, target->returnType) == NULL )
            return xl6IrCgenDiag(diags, module, target->declaration, "XLIR C emitter does not support call return ABI");
        if ( target->resultPhysicalAbi == XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ) {
            if ( target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID ||
                 target->resultPhysicalDestroySymbol != 0 ||
                 target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY )
                return xl6IrCgenDiag(diags, module, target->declaration,
                    "XLIR C emitter direct result carries non-direct physical facts");
        } else if ( xl6IrCgenNativeClassResultField(module, target) == NULL ||
                    xl6IrCgenType(
                        module, xl6IrCgenPhysicalResultType(module, target)) == NULL ) {
            return xl6IrCgenDiag(diags, module, target->declaration,
                "XLIR C emitter native factory result ABI is unresolved");
        }
        for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
            const XL6_IrType* parameterType = target->parameterTypes != NULL
                ? xl6IrTypeGet(&module->types,
                    target->parameterTypes[parameter]) : NULL;
            if ( target->parameterAbis == NULL ||
                  target->parameterPhysicalAbis == NULL ||
                  target->parameterPhysicalFieldDeclarations == NULL ||
                 (target->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_VALUE &&
                  target->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_BORROW &&
                  target->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_INOUT &&
                  target->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_OUT &&
                  target->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_OWNED) )
                return xl6IrCgenCallParameterDiag(diags, module, target, parameter,
                                                   "has no canonical parameter ownership ABI");
            if ( target->parameterPhysicalAbis[parameter] ==
                     XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT ) {
                if ( target->parameterPhysicalFieldDeclarations[parameter] !=
                         XL6_IR_NODE_INVALID ||
                     (target->kind == XL6_IR_CALL_TARGET_EXTERN_C &&
                      parameterType != NULL &&
                      (parameterType->kind == XL6_IR_TYPE_FUNCTION || xl6IrTypeIsCOwned(parameterType))) )
                    return xl6IrCgenCallParameterDiag(diags, module, target,
                        parameter, "uses a direct closure at an extern boundary");
            } else if(target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_C_SPAN_BORROW) {
                if((target->kind!=XL6_IR_CALL_TARGET_EXTERN_C && target->kind!=XL6_IR_CALL_TARGET_NATIVE_FACTORY) || !xl6IrTypeIsCOwned(parameterType) ||
                    target->parameterAbis[parameter]!=XL6_IR_PARAMETER_ABI_BORROW ||
                    (target->runtimeBridge!=XL6_IR_RUNTIME_BRIDGE_NONE && !xl6IrRuntimeDirectBridgeTargetMatches(module,target)) ||
                    target->parameterPhysicalFieldDeclarations[parameter]!=XL6_IR_NODE_INVALID)
                    return xl6IrCgenCallParameterDiag(diags,module,target,parameter,"has no read-only C span ABI");
            } else if(target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_VALUE_ADDRESS_BORROW ||
                      target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD) {
                if(!xl6IrRuntimeDirectBridgeValueAddressParameter(target->cName,parameter) ||
                   !xl6IrRuntimeDirectBridgeTargetMatches(module,target))
                    return xl6IrCgenCallParameterDiag(diags,module,target,parameter,"has no sealed native value-address ABI");
            } else if(target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_RESULT_CLOSURE_BORROW){
                if(!xl6IrRuntimeDirectBridgeResultClosureParameter(target->cName,parameter)||
                    !xl6IrRuntimeDirectBridgeTargetMatches(module,target))
                    return xl6IrCgenCallParameterDiag(diags,module,target,parameter,"has no exact owned-result callback ABI");
            } else if (target->parameterPhysicalAbis[parameter]==XL6_IR_CALL_PARAMETER_PHYSICAL_CHECKED_CLOSURE_BORROW) {
                if ((target->kind!=XL6_IR_CALL_TARGET_EXTERN_C && target->kind!=XL6_IR_CALL_TARGET_NATIVE_FACTORY) ||
                    target->parameterAbis[parameter]!=XL6_IR_PARAMETER_ABI_BORROW ||
                    !parameterType || parameterType->kind!=XL6_IR_TYPE_FUNCTION ||
                    parameterType->cAbi!=XL6_IR_C_ABI_CLOSURE_PTR ||
                    parameterType->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
                    parameterType->nullAbi!=XL6_IR_NULL_ABI_POINTER_SENTINEL ||
                    target->parameterPhysicalFieldDeclarations[parameter]!=XL6_IR_NODE_INVALID)
                    return xl6IrCgenCallParameterDiag(diags,module,target,parameter,"has no checked borrowed closure pointer ABI");
            } else if ( target->parameterPhysicalAbis[parameter] ==
                            XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE ) {
                if ( (target->kind != XL6_IR_CALL_TARGET_EXTERN_C &&
                      target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY) ||
                     target->parameterAbis[parameter] !=
                         XL6_IR_PARAMETER_ABI_BORROW ||
                       !xl6IrCgenLanguageFunctionValueSignatureAbi(
                          module, target->parameterTypes[parameter]) ||
                      target->parameterPhysicalFieldDeclarations[parameter] !=
                          XL6_IR_NODE_INVALID )
                    return xl6IrCgenCallParameterDiag(diags, module, target,
                        parameter, "has no verified XL6_Function adapter ABI");
            } else if ( target->parameterPhysicalAbis[parameter] ==
                            XL6_IR_CALL_PARAMETER_PHYSICAL_RAW_C_CALLBACK ) {
                if ( (target->kind != XL6_IR_CALL_TARGET_EXTERN_C &&
                      target->kind != XL6_IR_CALL_TARGET_NATIVE_FACTORY) ||
                     target->parameterAbis[parameter] !=
                         XL6_IR_PARAMETER_ABI_BORROW ||
                      !xl6IrRawCCallbackSignatureSupported(
                          module, target->parameterTypes[parameter]) ||
                      target->parameterPhysicalFieldDeclarations[parameter] !=
                          XL6_IR_NODE_INVALID )
                    return xl6IrCgenCallParameterDiag(diags, module, target,
                        parameter, "has no verified raw C callback ABI");
            } else if ( target->parameterPhysicalAbis[parameter] ==
                            XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW ) {
                if ( xl6IrCgenNativeClassPhysicalField(
                         module, target, parameter) == NULL )
                    return xl6IrCgenCallParameterDiag(diags, module, target,
                        parameter,
                        "has no exact managed native-class field-borrow ABI");
            } else {
                return xl6IrCgenCallParameterDiag(diags, module, target,
                    parameter, "has no canonical physical ABI");
            }
            if ( !xl6IrCgenIsEntryParameter(module, target->symbol, parameter) &&
                 target->parameterPhysicalAbis[parameter] != XL6_IR_CALL_PARAMETER_PHYSICAL_GENERIC_ZERO_PAYLOAD &&
                 xl6IrCgenType(module, target->parameterTypes[parameter]) == NULL )
                return xl6IrCgenCallParameterDiag(diags, module, target, parameter,
                                                   "has no supported C value ABI");
        }
        if ( (target->kind == XL6_IR_CALL_TARGET_INTERNAL ||
              target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ||
              target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR) &&
              xl6IrCgenTargetIsReachable(module, target->symbol) &&
             xl6IrCgenFindFunction(module, target->symbol) == NULL )
            return xl6IrCgenDiag(diags, module, target->declaration, "XLIR C emitter internal call target has no body ABI");
        if ( (target->kind == XL6_IR_CALL_TARGET_EXTERN_C ||
              target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY) &&
             (target->cName == NULL || target->cName[0] == '\0'))
            return xl6IrCgenDiag(diags, module, target->declaration, "XLIR C emitter extern call target has no C linkage ABI");
        if ( target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC &&
             (!xl6IrCgenAsyncResultAbiMatches(module, target) ||
              (target->workerErrorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
               target->workerErrorAbi != XL6_IR_ERROR_ABI_MAY_RAISE) ||
              (target->workerErrorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
               xl6IrCgenErrorResultType(module, target->workerReturnType) == NULL) ||
               (target->asyncDispatch != XL6_IR_ASYNC_DISPATCH_FIXED_READY_QUEUE &&
                target->asyncDispatch != XL6_IR_ASYNC_DISPATCH_XRT_READY_FUTURE) ||
              (target->asyncDispatch == XL6_IR_ASYNC_DISPATCH_FIXED_READY_QUEUE &&
               xl6IrCgenAsyncMachine(module,target) == NULL) ||
              (target->asyncResultAbi == XL6_IR_ASYNC_RESULT_VOID &&
               target->asyncDispatch != XL6_IR_ASYNC_DISPATCH_FIXED_READY_QUEUE) ||
               (target->parameterCount != 0 && target->asyncCaptureAbis == NULL)))
            return xl6IrCgenDiag(diags, module, target->declaration,
                                 "XLIR C emitter async target ABI is not implemented");
        if ( target->kind == XL6_IR_CALL_TARGET_INTERNAL_ASYNC ) {
            for ( parameter = 0; parameter < target->parameterCount; ++parameter ) {
                const XL6_IrType* type = xl6IrTypeGet(&module->types, target->parameterTypes[parameter]);
                if (type == NULL || target->asyncCaptureAbis[parameter] == XL6_IR_ASYNC_CAPTURE_INVALID ||
                    target->asyncCaptureAbis[parameter] != xl6IrAsyncParameterCapture(
                        module, target->parameterTypes[parameter], target->parameterAbis[parameter]))
                    return xl6IrCgenDiag(diags, module, target->declaration,
                                         "XLIR C emitter async task frame capture ABI is unresolved");
            }
        }
        if ( target->kind == XL6_IR_CALL_TARGET_INTERNAL_GENERATOR ) {
            const XL6_IrGeneratorMachine* machine =
                xl6IrCgenGeneratorMachine(module, target);
            const XL6_IrType* item =
                xl6IrTypeGet(&module->types, target->generatorItemType);
            if ( machine == NULL || item == NULL ||
                 xl6IrCgenType(module, target->generatorItemType) == NULL ||
                 !xl6IrCgenGeneratorMachineProgramSupported(
                     module, target, machine) )
                return xl6IrCgenDiag(diags, module, target->declaration,
                    "XLIR C emitter generator target has no exact machine/item ABI");
        }
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
    }
    for ( i = 0; i < module->exportCount; ++i ) {
        const XL6_IrExport* exported = &module->exports[i];
        const XL6_IrCallTarget* target = xl6IrCallTargetGet(module, exported->symbol);
        size_t parameter;
        if ( target == NULL || target->kind != XL6_IR_CALL_TARGET_INTERNAL ||
             exported->cName == NULL || exported->cName[0] == '\0' )
            return xl6IrCgenDiag(diags, module, exported->declaration,
                                 "XLIR C emitter export linkage ABI is unresolved");
        if ( target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
             xl6IrCgenErrorResultType(module, exported->returnType) == NULL )
            return xl6IrCgenDiag(diags, module, exported->declaration,
                                 "XLIR C emitter export error-result adapter ABI is unresolved");
        if ( exported->returnAbi == XL6_IR_EXPORT_RETURN_INVALID ||
             (exported->returnType != XL6_IR_TYPE_INVALID &&
              xl6IrCgenExportValueType(module, exported->returnType,
                  exported->returnValueAbi) == NULL) )
            return xl6IrCgenDiag(diags, module, exported->declaration,
                                 "XLIR C emitter does not support export return ABI");
        for ( parameter = 0; parameter < exported->parameterCount; ++parameter ) {
            if ( exported->parameterAbis == NULL || exported->parameterTypes == NULL ||
                 exported->parameterValueAbis == NULL ||
                 exported->parameterNames == NULL ||
                 (exported->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_VALUE &&
                  exported->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_BORROW &&
                  exported->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_INOUT &&
                  exported->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_OUT &&
                  exported->parameterAbis[parameter] != XL6_IR_PARAMETER_ABI_OWNED) ||
                  (!xl6IrExportValueAbiIsCallback(exported->parameterValueAbis[parameter]) &&
                   xl6IrCgenExportValueType(module,
                      exported->parameterTypes[parameter],
                      exported->parameterValueAbis[parameter]) == NULL) ||
                  exported->parameterNames[parameter] == NULL )
                return xl6IrCgenDiag(diags, module, exported->declaration,
                                     "XLIR C emitter does not support export parameter ABI");
        }
    }
    for ( i = 0; i < module->globalExportCount; ++i ) {
        const XL6_IrGlobalExport* exported = &module->globalExports[i];
        const XL6_IrStorage* storage = xl6IrStorageGet(module, exported->storageSymbol);
        if ( exported->cName == NULL || exported->cName[0] == '\0' ||
             exported->initializer == NULL || exported->initializer[0] == '\0' ||
             (exported->mutability != XL6_IR_GLOBAL_MUTABLE &&
              exported->mutability != XL6_IR_GLOBAL_CONST) ||
             storage == NULL || storage->kind != XL6_IR_STORAGE_GLOBAL ||
             storage->type != exported->type ||
             xl6IrCgenType(module, exported->type) == NULL )
            return xl6IrCgenDiag(diags, module, exported->declaration,
                                 "XLIR C emitter global export ABI is unresolved");
    }
    for ( i = 0; i < module->externGlobalCount; ++i ) {
        const XL6_IrExternGlobal* imported = &module->externGlobals[i];
        const XL6_IrStorage* storage = xl6IrStorageGet(module, imported->storageSymbol);
        if ( imported->cName == NULL || imported->cName[0] == '\0' ||
             imported->accessAbi != XL6_IR_EXTERN_GLOBAL_READ_WRITE ||
             storage == NULL || storage->kind != XL6_IR_STORAGE_EXTERN_GLOBAL ||
             storage->type != imported->type ||
             !xl6IrExternGlobalTypeSupported(module, imported->type) ||
             xl6IrCgenType(module, imported->type) == NULL )
            return xl6IrCgenDiag(diags, module, imported->declaration,
                                 "XLIR C emitter extern global ABI is unresolved");
    }
    for ( i = 0; i < module->storageCount; ++i ) {
        const XL6_IrStorage* storage = &module->storages[i];
        if ( !xl6IrCgenIsEntryStorage(module, storage) && xl6IrCgenType(module, storage->type) == NULL )
            return xl6IrCgenDiag(diags, module, storage->declaration, "XLIR C emitter does not support storage ABI");
    }
    for ( i = 0; i < module->functionCount; ++i ) {
        const XL6_IrFunction* function = &module->functions[i];
        size_t instructionIndex;
        size_t parameterIndex;
        if ( function->reachability == XL6_IR_REACHABILITY_UNREACHABLE ) continue;
        if ( function->errorAbi != XL6_IR_ERROR_ABI_NO_RAISE &&
             function->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE )
            return xl6IrCgenDiag(diags, module, function->declaration,
                                 "XLIR C emitter function error-result ABI is not implemented");
        if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
              (function->kind == XL6_IR_FUNCTION_USER ||
               function->kind == XL6_IR_FUNCTION_ASYNC_WORKER ||
               function->kind == XL6_IR_FUNCTION_NAMED_ADAPTER ||
               function->kind == XL6_IR_FUNCTION_ARGUMENT_DEFAULT) &&
             xl6IrCgenErrorResultType(module, function->returnType) == NULL )
            return xl6IrCgenDiag(diags, module, function->declaration,
                                 "XLIR C emitter does not support this function error-result ABI");
        if ( (function->kind == XL6_IR_FUNCTION_USER ||
              function->kind == XL6_IR_FUNCTION_ASYNC_WORKER ||
              function->kind == XL6_IR_FUNCTION_NAMED_ADAPTER ||
              function->kind == XL6_IR_FUNCTION_ARGUMENT_DEFAULT) &&
             function->returnType != XL6_IR_TYPE_INVALID && xl6IrCgenType(module, function->returnType) == NULL )
            return xl6IrCgenDiag(diags, module, function->declaration, "XLIR C emitter does not support this function return ABI");
        for ( parameterIndex = 0; parameterIndex < function->parameterCount; ++parameterIndex ) {
            if ( xl6IrCgenFunctionParameterType(module, function, parameterIndex) == NULL )
                return xl6IrCgenDiag(diags, module, function->declaration, "XLIR C emitter does not support a parameter ABI");
        }
        for ( instructionIndex = function->firstInstruction;
              instructionIndex < function->firstInstruction + function->instructionCount; ++instructionIndex ) {
            const XL6_IrInstruction* instruction = &module->instructions[instructionIndex];
            if ( instruction->kind != XL6_IR_INSTR_RETURN ) continue;
            if ( function->kind == XL6_IR_FUNCTION_MODULE_INIT ) {
                if ( instruction->operandCount != 0 )
                    return xl6IrCgenDiag(diags, module, instruction->origin,
                                         "XLIR module initializer return has an invalid ABI");
                continue;
            }
            if ( function->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE &&
                 (xl6IrCgenErrorResultType(module, function->returnType) == NULL ||
                  (function->returnType == XL6_IR_TYPE_INVALID
                    ? instruction->operandCount != 0 : instruction->operandCount != 1)) )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter error-result return ABI is unresolved");
            if ( function->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE &&
                 function->returnType == XL6_IR_TYPE_INVALID && instruction->operandCount != 0 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR void function return carries a value");
            if ( function->errorAbi != XL6_IR_ERROR_ABI_MAY_RAISE &&
                 function->returnType != XL6_IR_TYPE_INVALID && instruction->operandCount != 1 )
                return xl6IrCgenDiag(diags, module, instruction->origin,
                                     "XLIR C emitter requires a single lowered return value");
        }
    }
    return 1;
}
