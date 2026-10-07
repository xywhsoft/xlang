static int testDynamicFieldsBridgeAbi(void)
{
    XL6_IrModule module;
    XL6_IrType types[4];
    XL6_IrTypeId parameters[3] = {1, 2, 3};
    XL6_IrParameterAbi abis[3] = {XL6_IR_PARAMETER_ABI_BORROW,
        XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_PARAMETER_ABI_BORROW};
    XL6_IrCallParameterPhysicalAbi physical[3] = {
        XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW,
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT, XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT};
    XL6_IrNodeId fields[3] = {7, 0, 0};
    XL6_IrCallTarget target, saved;
    const char* names[4] = {"xl6_xrt_dynamic_fields_create", "xl6_xrt_dynamic_fields_destroy",
        "xl6_xrt_dynamic_fields_get", "xl6_xrt_dynamic_fields_set"};
    size_t i;
    memset(&module, 0, sizeof(module)); memset(types, 0, sizeof(types));
    module.types.items = types; module.types.count = 4;
    types[0].kind = XL6_IR_TYPE_CLASS;
    types[1].kind = types[2].kind = types[3].kind = XL6_IR_TYPE_BUILTIN;
    types[0].cAbi = types[2].cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    types[0].ownershipAbi = types[1].ownershipAbi = types[2].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[0].nullAbi = types[2].nullAbi = XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    types[1].cAbi = XL6_IR_C_ABI_MANAGED_STRING; types[1].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    types[3].cAbi = XL6_IR_C_ABI_BOOL; types[3].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    for (i = 0; i < 4; ++i) {
        memset(&target, 0, sizeof(target));
        target.cName = (char*)names[i];
        target.kind = i == 0 ? XL6_IR_CALL_TARGET_NATIVE_FACTORY : XL6_IR_CALL_TARGET_EXTERN_C;
        target.runtimeBridge = XL6_IR_RUNTIME_BRIDGE_DYNAMIC_FIELDS;
        target.errorAbi = xl6IrDynamicFieldsRuntimeBridgeErrorAbi(target.cName);
        target.returnType = i == 0 ? 1 : i == 1 ? 0 : i == 2 ? 3 : 4;
        target.resultPhysicalAbi = i == 0 ? XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_TAKE
            : XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;
        target.resultPhysicalFieldDeclaration = i == 0 ? 7 : 0;
        target.resultPhysicalDestroySymbol = i == 0 ? 9 : 0;
        target.parameterCount = i;
        target.parameterTypes = parameters; target.parameterAbis = abis;
        target.parameterPhysicalAbis = physical; target.parameterPhysicalFieldDeclarations = fields;
        if (!xl6IrDynamicFieldsRuntimeBridgeTargetMatches(&module, &target)) return 0;
        saved = target;
#define REJECT_DYNAMIC_FIELD(field, value) \
        target.field = value; \
        if (xl6IrDynamicFieldsRuntimeBridgeTargetMatches(&module, &target)) return 0; \
        target = saved
        REJECT_DYNAMIC_FIELD(runtimeBridge, XL6_IR_RUNTIME_BRIDGE_NONE);
        REJECT_DYNAMIC_FIELD(cName, "xrtDynamicFieldsCreate");
        REJECT_DYNAMIC_FIELD(parameterCount, 4);
        REJECT_DYNAMIC_FIELD(errorAbi, saved.errorAbi == XL6_IR_ERROR_ABI_MAY_RAISE
            ? XL6_IR_ERROR_ABI_NO_RAISE : XL6_IR_ERROR_ABI_MAY_RAISE);
        REJECT_DYNAMIC_FIELD(variadicAbi, XL6_IR_CALL_VARIADIC_C);
        REJECT_DYNAMIC_FIELD(returnType, 99);
        if (i == 0) {
            REJECT_DYNAMIC_FIELD(resultPhysicalDestroySymbol, 0);
            REJECT_DYNAMIC_FIELD(resultPhysicalFieldDeclaration, 0);
            REJECT_DYNAMIC_FIELD(resultPhysicalAbi, XL6_IR_CALL_RESULT_PHYSICAL_NATIVE_CLASS_FIELD_BORROW);
        } else {
            REJECT_DYNAMIC_FIELD(parameterTypes, NULL);
            REJECT_DYNAMIC_FIELD(parameterPhysicalFieldDeclarations, NULL);
            physical[0] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            if (xl6IrDynamicFieldsRuntimeBridgeTargetMatches(&module, &target)) return 0;
            physical[0] = XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW;
        }
#undef REJECT_DYNAMIC_FIELD
    }
    return xl6IrDynamicFieldsRuntimeBridgeErrorAbi(NULL) == XL6_IR_ERROR_ABI_INVALID &&
        xl6IrDynamicFieldsRuntimeBridgeErrorAbi("not_a_dynamic_fields_bridge") == XL6_IR_ERROR_ABI_INVALID &&
        !xl6IrDynamicFieldsRuntimeBridgeTargetMatches(NULL, &target) &&
        !xl6IrDynamicFieldsRuntimeBridgeTargetMatches(&module, NULL);
}
