/* Compiler-embedding bridge contract.  The standard library exposes these
 * helpers as ordinary extern declarations, but their implementation is a
 * compiler-owned generated component.  Resolve and validate that fact before
 * Cgen so no backend has to rediscover it from source names. */
int xl6IrRuntimeXlangBridgeName(const char* name)
{
    static const char* const names[] = {
        "xl6_xlang_context_create",
        "xl6_xlang_context_destroy",
        "xl6_xlang_context_last_error_copy",
        "xl6_xlang_context_set_work_dir",
        "xl6_xlang_context_set_temp_dir",
        "xl6_xlang_context_set_save_temps",
        "xl6_xlang_context_run_source",
        "xl6_xlang_context_run_file",
        "xl6_xlang_context_run_source_args",
        "xl6_xlang_context_run_file_args",
        "xl6_xlang_context_clear_last_error",
        "xl6_xlang_handler_wrap",
        "xl6_xlang_handler_create",
        "xl6_xlang_handler_destroy",
        "xl6_xlang_handler_run_args",
        "xl6_xlang_handler_last_error_copy",
        "xl6_xlang_run_source",
        "xl6_xlang_run_file"
    };
    size_t i;
    if ( name == NULL ) return 0;
    for ( i = 0; i < sizeof(names) / sizeof(names[0]); ++i )
        if ( strcmp(name, names[i]) == 0 ) return 1;
    return 0;
}

XL6_IrErrorAbi xl6IrRuntimeXlangBridgeErrorAbi(const char* name)
{
    if ( !xl6IrRuntimeXlangBridgeName(name) )
        return XL6_IR_ERROR_ABI_INVALID;
    /* Handle wrapping and finalization have no observable failure channel.
     * Allocation, compilation, invocation and diagnostic copying can report
     * an XRT-thread failure and therefore keep a checked exceptional edge. */
    return strcmp(name, "xl6_xlang_handler_wrap") == 0 ||
           strcmp(name, "xl6_xlang_context_destroy") == 0 ||
           strcmp(name, "xl6_xlang_handler_destroy") == 0 ||
           strcmp(name, "xl6_xlang_context_clear_last_error") == 0
        ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE;
}

static int xl6IrRuntimeXlangDirectParameterMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target, size_t index,
    XL6_IrCAbi cAbi, XL6_IrParameterAbi parameterAbi)
{
    return target->parameterConstraintAbis != NULL &&
           target->parameterPhysicalFieldDeclarations != NULL &&
           target->parameterConstraintAbis[index] ==
               XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE &&
           target->parameterPhysicalFieldDeclarations[index] ==
               XL6_IR_NODE_INVALID &&
           xl6IrBufferRuntimeBridgeParameterMatches(
               module, target, index, cAbi, parameterAbi,
               XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT);
}

static int xl6IrRuntimeXlangReceiverMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target, size_t index)
{
    const XL6_IrType* receiver;
    if ( target->parameterConstraintAbis == NULL ||
         target->parameterConstraintAbis[index] !=
             XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ||
         xl6IrCallTargetNativeClassPhysicalField(module, target, index) == NULL )
        return 0;
    receiver = xl6IrTypeGet(&module->types, target->parameterTypes[index]);
    return receiver != NULL && receiver->kind == XL6_IR_TYPE_CLASS &&
           receiver->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           receiver->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           receiver->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL;
}

static int xl6IrRuntimeXlangFactoryMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const XL6_IrType* result = xl6IrTypeGet(
        &module->types, target->returnType);
    return target->kind == XL6_IR_CALL_TARGET_NATIVE_FACTORY &&
           result != NULL && result->kind == XL6_IR_TYPE_CLASS &&
           result->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           result->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           target->resultPhysicalAbi ==
               XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE &&
           xl6IrCallTargetNativeClassResultField(module, target) != NULL;
}

int xl6IrRuntimeXlangBridgeTargetMatches(
    const XL6_IrModule* module, const XL6_IrCallTarget* target)
{
    const char* name;
    const XL6_IrType* result;
    size_t i;
    if ( module == NULL || target == NULL ||
         (name = target->cName) == NULL ||
         !xl6IrRuntimeXlangBridgeName(name) ||
         target->runtimeBridge != XL6_IR_RUNTIME_BRIDGE_XLANG ||
         target->runtimeBridgeOperation !=
             XL6_IR_RUNTIME_BRIDGE_OPERATION_NONE ||
         target->variadicAbi != XL6_IR_CALL_VARIADIC_NONE ||
         target->errorAbi != xl6IrRuntimeXlangBridgeErrorAbi(name) ||
         (target->errorPhysicalAbi != XL6_IR_CALL_ERROR_PHYSICAL_INVALID &&
          target->errorPhysicalAbi !=
              (target->errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
                  ? XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR
                  : XL6_IR_CALL_ERROR_PHYSICAL_NONE)) ||
         (target->parameterCount != 0 &&
          (target->parameterTypes == NULL || target->parameterAbis == NULL ||
           target->parameterPhysicalAbis == NULL ||
           target->parameterConstraintAbis == NULL ||
           target->parameterPhysicalFieldDeclarations == NULL)) ) return 0;
    for ( i = 0; i < target->parameterCount; ++i )
        if ( target->parameterConstraintAbis[i] !=
                 XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE ) return 0;

    if ( strcmp(name, "xl6_xlang_context_create") == 0 )
        return target->parameterCount == 0 &&
               xl6IrRuntimeXlangFactoryMatches(module, target);
    if ( strcmp(name, "xl6_xlang_handler_wrap") == 0 )
        return target->parameterCount == 1 &&
               xl6IrRuntimeXlangFactoryMatches(module, target) &&
               xl6IrRuntimeXlangDirectParameterMatches(
                   module, target, 0, XL6_IR_C_ABI_RAW_POINTER,
                   XL6_IR_PARAMETER_ABI_VALUE);
    if ( strcmp(name, "xl6_xlang_handler_create") == 0 )
        return target->parameterCount == 2 &&
               xl6IrRuntimeXlangFactoryMatches(module, target) &&
               xl6IrRuntimeXlangDirectParameterMatches(
                   module, target, 0, XL6_IR_C_ABI_MANAGED_STRING,
                   XL6_IR_PARAMETER_ABI_BORROW) &&
               xl6IrRuntimeXlangDirectParameterMatches(
                   module, target, 1, XL6_IR_C_ABI_MANAGED_STRING,
                   XL6_IR_PARAMETER_ABI_BORROW);

    if ( target->kind != XL6_IR_CALL_TARGET_EXTERN_C ||
         target->resultPhysicalAbi != XL6_IR_CALL_RESULT_PHYSICAL_DIRECT ||
         target->resultPhysicalFieldDeclaration != XL6_IR_NODE_INVALID ||
         target->resultPhysicalDestroySymbol != 0 ) return 0;
    result = target->returnType != XL6_IR_TYPE_INVALID
        ? xl6IrTypeGet(&module->types, target->returnType) : NULL;
    if ( strcmp(name, "xl6_xlang_run_source") == 0 ||
         strcmp(name, "xl6_xlang_run_file") == 0 )
        return target->parameterCount == 1 &&
               xl6IrBufferRuntimeBridgeResultMatches(
                   result, XL6_IR_C_ABI_C_INT) &&
               xl6IrRuntimeXlangDirectParameterMatches(
                   module, target, 0, XL6_IR_C_ABI_MANAGED_STRING,
                   XL6_IR_PARAMETER_ABI_BORROW);
    if ( target->parameterCount == 0 ||
         !xl6IrRuntimeXlangReceiverMatches(module, target, 0) ) return 0;
    if ( strcmp(name, "xl6_xlang_context_destroy") == 0 ||
         strcmp(name, "xl6_xlang_handler_destroy") == 0 )
        return target->parameterCount == 1 && result == NULL;
    if ( strcmp(name, "xl6_xlang_context_clear_last_error") == 0 )
        return target->parameterCount == 1 && result == NULL;
    if ( strcmp(name, "xl6_xlang_context_last_error_copy") == 0 ||
         strcmp(name, "xl6_xlang_handler_last_error_copy") == 0 )
        return target->parameterCount == 1 &&
               xl6IrBufferRuntimeBridgeResultMatches(
                   result, XL6_IR_C_ABI_RAW_POINTER);
    if ( strcmp(name, "xl6_xlang_handler_run_args") == 0 ) {
        const XL6_IrType* args = target->parameterCount == 2
            ? xl6IrTypeGet(&module->types, target->parameterTypes[1]) : NULL;
        return args != NULL && args->kind == XL6_IR_TYPE_BUILTIN &&
               args->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
               target->parameterPhysicalFieldDeclarations[1] ==
                   XL6_IR_NODE_INVALID &&
               xl6IrBufferRuntimeBridgeResultMatches(
                   result, XL6_IR_C_ABI_BOOL) &&
               xl6IrBufferRuntimeBridgeParameterMatches(
                   module, target, 1, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
                   XL6_IR_PARAMETER_ABI_BORROW,
                   XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT);
    }
    if ( strcmp(name, "xl6_xlang_context_set_work_dir") == 0 ||
         strcmp(name, "xl6_xlang_context_set_temp_dir") == 0 ||
         strcmp(name, "xl6_xlang_context_run_source") == 0 ||
         strcmp(name, "xl6_xlang_context_run_file") == 0 )
        return target->parameterCount == 2 &&
               xl6IrBufferRuntimeBridgeResultMatches(
                   result, XL6_IR_C_ABI_BOOL) &&
               xl6IrRuntimeXlangDirectParameterMatches(
                   module, target, 1, XL6_IR_C_ABI_MANAGED_STRING,
                   XL6_IR_PARAMETER_ABI_BORROW);
    if ( strcmp(name, "xl6_xlang_context_set_save_temps") == 0 )
        return target->parameterCount == 2 &&
               xl6IrBufferRuntimeBridgeResultMatches(
                   result, XL6_IR_C_ABI_BOOL) &&
               xl6IrRuntimeXlangDirectParameterMatches(
                   module, target, 1, XL6_IR_C_ABI_BOOL,
                   XL6_IR_PARAMETER_ABI_VALUE);
    if ( strcmp(name, "xl6_xlang_context_run_source_args") == 0 ||
         strcmp(name, "xl6_xlang_context_run_file_args") == 0 ) {
        const XL6_IrType* args = target->parameterCount == 3
            ? xl6IrTypeGet(&module->types, target->parameterTypes[2]) : NULL;
        return args != NULL && args->kind == XL6_IR_TYPE_BUILTIN &&
               args->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
               target->parameterPhysicalFieldDeclarations[2] ==
                   XL6_IR_NODE_INVALID &&
               xl6IrBufferRuntimeBridgeResultMatches(
                   result, XL6_IR_C_ABI_BOOL) &&
               xl6IrRuntimeXlangDirectParameterMatches(
                   module, target, 1, XL6_IR_C_ABI_MANAGED_STRING,
                   XL6_IR_PARAMETER_ABI_BORROW) &&
               xl6IrBufferRuntimeBridgeParameterMatches(
                   module, target, 2, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
                   XL6_IR_PARAMETER_ABI_BORROW,
                   XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT);
    }
    return 0;
}
