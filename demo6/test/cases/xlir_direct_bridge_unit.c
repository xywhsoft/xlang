static int testOwnedReadResults(void)
{
    XL6_IrModule module;
    XL6_IrNode nodes[2];
    XL6_IrNodeId child = 1;
    XL6_IrType type;
    XL6_IrCAbi abis[] = {XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
        XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_CLOSURE_PTR};
    size_t i, j, k;
    memset(&module, 0, sizeof(module));
    memset(nodes, 0, sizeof(nodes)); memset(&type, 0, sizeof(type));
    module.nodes = nodes; module.nodeCount = 2;
    module.types.items = &type; module.types.count = 1;
    module.children = &child; module.childCount = 1;
    type.ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    nodes[0].storageType = 1; nodes[0].targetSymbol = 7;
    nodes[0].parent = 2;
    for (i = 0; i < sizeof(abis) / sizeof(abis[0]); ++i) {
        type.cAbi = abis[i];
        for (j = 0; j < 2; ++j) {
            nodes[0].kind = j == 0 ? XL6_IR_NODE_MEMBER : XL6_IR_NODE_INDEX;
            for (k = 0; k < 2; ++k) {
                nodes[0].callKind = k == 0 ? XL6_SEM_CALL_PROPERTY_GET : XL6_SEM_CALL_PROTOCOL;
                nodes[0].valueCategory = XL6_IR_VALUE_RVALUE;
                nodes[0].ownership = XL6_IR_OWNERSHIP_MOVE;
                nodes[0].ownershipPlanned = 1;
                if (!xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
                nodes[0].ownership = XL6_IR_OWNERSHIP_BORROW;
                if (xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
                nodes[0].ownership = XL6_IR_OWNERSHIP_MOVE;
                nodes[0].valueCategory = XL6_IR_VALUE_LVALUE;
                if (xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
                nodes[0].valueCategory = XL6_IR_VALUE_RVALUE;
                nodes[0].ownershipPlanned = 0;
                if (xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
                nodes[1].kind = XL6_IR_NODE_ASSIGN; nodes[1].childCount = 1;
                if (!xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
                nodes[1].kind = XL6_IR_NODE_INVALID; nodes[1].childCount = 0;
            }
        }
    }
    nodes[0].kind = XL6_IR_NODE_MEMBER; nodes[0].callKind = XL6_SEM_CALL_NONE;
    nodes[0].ownership = XL6_IR_OWNERSHIP_BORROW;
    nodes[0].valueCategory = XL6_IR_VALUE_LVALUE;
    if (!xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
    nodes[0].kind = XL6_IR_NODE_NAME; type.cAbi = XL6_IR_C_ABI_CLOSURE_PTR;
    nodes[0].namedCallableAbi = XL6_IR_NAMED_CALLABLE_CAPTURE_FREE_ADAPTER;
    if (xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
    nodes[0].valueCategory = XL6_IR_VALUE_RVALUE;
    nodes[0].ownership = XL6_IR_OWNERSHIP_MOVE; nodes[0].ownershipPlanned = 1;
    if (!xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
    nodes[0].namedCallableAbi = XL6_IR_NAMED_CALLABLE_NONE;
    nodes[0].ownership = XL6_IR_OWNERSHIP_BORROW;
    nodes[0].valueCategory = XL6_IR_VALUE_LVALUE;
    if (!xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
    nodes[0].memberAbi = XL6_IR_MEMBER_ABI_CLASS_FIELD_CLOSURE;
    if (xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
    nodes[0].valueCategory = XL6_IR_VALUE_RVALUE;
    nodes[0].ownership = XL6_IR_OWNERSHIP_MOVE;
    if (!xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
    nodes[0].memberAbi = XL6_IR_MEMBER_ABI_NONE;
    nodes[0].ownership = XL6_IR_OWNERSHIP_BORROW;
    nodes[0].valueCategory = XL6_IR_VALUE_LVALUE;
    nodes[0].kind = XL6_IR_NODE_INDEX; nodes[0].sourceKind = XL6_AST_INDEX;
    nodes[0].childCount = 2; type.cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    if (xl6IrVerifyOwnedReadResults(&module, NULL)) return 0;
    nodes[0].valueCategory = XL6_IR_VALUE_RVALUE;
    nodes[0].ownership = XL6_IR_OWNERSHIP_MOVE; nodes[0].ownershipPlanned = 1;
    return xl6IrVerifyOwnedReadResults(&module, NULL) &&
        !xl6IrVerifyOwnedReadResults(NULL, NULL);
}

#include "xlir_dynamic_fields_contract.inc"

static int testDeclarationBindings(void)
{
    XL6_IrModule module;
    XL6_IrNode nodes[4];
    XL6_IrNodeId children[4] = {2, 3, 3, 4};
    XL6_IrType types[3];
    XL6_IrTypeId fields[2] = {1, 2};
    XL6_IrDeclarationBinding binding;
    memset(&module, 0, sizeof(module));
    memset(nodes, 0, sizeof(nodes));
    memset(types, 0, sizeof(types));
    module.nodes = nodes; module.nodeCount = 4;
    module.children = children; module.childCount = 4;
    module.types.items = types; module.types.count = 3;
    types[0].kind = types[1].kind = XL6_IR_TYPE_BUILTIN;
    types[0].cAbi = XL6_IR_C_ABI_I64;
    types[1].cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    types[2].kind = XL6_IR_TYPE_TUPLE;
    types[2].cAbi = XL6_IR_C_ABI_PRODUCT;
    types[2].params = fields; types[2].paramCount = 2;
    nodes[0].kind = XL6_IR_NODE_VALUE_DECL;
    nodes[0].storageType = 1; nodes[0].symbol = 7;
    if (xl6IrDeclarationBindingCount(&module, 1) != 1 ||
        !xl6IrDeclarationBindingAt(&module, 1, 0, &binding) ||
        binding.origin != 1 || binding.symbol != 7 || binding.type != 1 ||
        xl6IrDeclarationBindingAt(&module, 1, 1, &binding)) return 0;
    nodes[0].symbol = 0;
    if (xl6IrDeclarationBindingAt(&module, 1, 0, &binding)) return 0;
    nodes[0].storageType = 3; nodes[0].childCount = 2;
    nodes[1].kind = XL6_IR_NODE_LIST;
    nodes[1].firstChild = 2; nodes[1].childCount = 2;
    nodes[2].kind = nodes[3].kind = XL6_IR_NODE_NAME;
    nodes[2].symbol = 11; nodes[3].text = "_";
    if (xl6IrDeclarationBindingCount(&module, 1) != 2 ||
        !xl6IrDeclarationBindingAt(&module, 1, 0, &binding) ||
        binding.origin != 3 || binding.symbol != 11 || binding.type != 1 ||
        !xl6IrDeclarationBindingAt(&module, 1, 1, &binding) ||
        binding.origin != 4 || binding.symbol != 0 || binding.type != 2 ||
        xl6IrDeclarationBindingAt(&module, 1, 2, &binding)) return 0;
    nodes[3].text = "unresolved";
    if (xl6IrDeclarationBindingAt(&module, 1, 1, &binding)) return 0;
    nodes[3].text = "_";
    nodes[1].firstChild = SIZE_MAX;
    if (xl6IrDeclarationBindingAt(&module, 1, 0, &binding)) return 0;
    nodes[1].firstChild = 2; nodes[1].childCount = 1;
    if (xl6IrDeclarationBindingAt(&module, 1, 0, &binding)) return 0;
    nodes[1].childCount = 2; fields[1] = 99;
    if (xl6IrDeclarationBindingAt(&module, 1, 1, &binding)) return 0;
    fields[1] = 2; module.children = NULL;
    if (xl6IrDeclarationBindingAt(&module, 1, 0, &binding)) return 0;
    return xl6IrDeclarationBindingCount(NULL, 1) == 0 &&
        !xl6IrDeclarationBindingAt(NULL, 1, 0, &binding) &&
        !xl6IrDeclarationBindingAt(&module, 1, 0, NULL);
}

static int testDynamicCompareAbi(void)
{
    XL6_IrModule module;
    XL6_IrType types[2];
    XL6_IrInstruction definitions[2], call, saved;
    XL6_IrValueId operands[2] = {1, 2};
    XL6_IrRuntimeOp operations[] = {XL6_IR_RUNTIME_XVALUE_EQUAL,
        XL6_IR_RUNTIME_XVALUE_ORDER_LT, XL6_IR_RUNTIME_XVALUE_ORDER_LE,
        XL6_IR_RUNTIME_XVALUE_ORDER_GT, XL6_IR_RUNTIME_XVALUE_ORDER_GE,
        XL6_IR_RUNTIME_XVALUE_APPROX};
    size_t i;
    memset(&module, 0, sizeof(module));
    memset(types, 0, sizeof(types));
    memset(definitions, 0, sizeof(definitions));
    memset(&call, 0, sizeof(call));
    types[0].kind = types[1].kind = XL6_IR_TYPE_BUILTIN;
    types[0].cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[0].nullAbi = XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    types[1].cAbi = XL6_IR_C_ABI_BOOL;
    types[1].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    module.types.items = types; module.types.count = 2;
    module.instructions = definitions; module.instructionCount = 2;
    module.operands = operands; module.operandCount = 2;
    definitions[0].type = definitions[1].type = 1;
    definitions[0].result = 1; definitions[1].result = 2;
    call.kind = XL6_IR_INSTR_RUNTIME_CALL;
    call.result = 3; call.type = 2; call.operandCount = 2;
    for (i = 0; i < sizeof(operations) / sizeof(operations[0]); ++i) {
        call.runtimeOp = operations[i];
        if (!xl6IrDynamicCompareRuntimeInstructionCompatible(&module, &call)) return 0;
        saved = call;
#define REJECT_COMPARE_FIELD(field, value) \
        call.field = value; \
        if (xl6IrDynamicCompareRuntimeInstructionCompatible(&module, &call)) return 0; \
        call = saved
        REJECT_COMPARE_FIELD(kind, XL6_IR_INSTR_BINARY);
        REJECT_COMPARE_FIELD(runtimeOp, XL6_IR_RUNTIME_XVALUE_I64);
        REJECT_COMPARE_FIELD(result, 0);
        REJECT_COMPARE_FIELD(type, 1);
        REJECT_COMPARE_FIELD(operandCount, 1);
        REJECT_COMPARE_FIELD(firstOperand, SIZE_MAX);
        REJECT_COMPARE_FIELD(firstOperand, 1);
        REJECT_COMPARE_FIELD(immediateText, "fallback");
        REJECT_COMPARE_FIELD(targetSymbol, 1);
        REJECT_COMPARE_FIELD(targetValue, 1);
        REJECT_COMPARE_FIELD(calleeType, 1);
        REJECT_COMPARE_FIELD(payloadType, 1);
        REJECT_COMPARE_FIELD(runtimeIdentityType, 1);
        REJECT_COMPARE_FIELD(opcode, XL6_IR_OPCODE_EQ);
#undef REJECT_COMPARE_FIELD
        operands[1] = 99;
        if (xl6IrDynamicCompareRuntimeInstructionCompatible(&module, &call)) return 0;
        operands[1] = 2;
        definitions[0].type = 2;
        if (xl6IrDynamicCompareRuntimeInstructionCompatible(&module, &call)) return 0;
        definitions[0].type = 1;
        types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
        if (xl6IrDynamicCompareRuntimeInstructionCompatible(&module, &call)) return 0;
        types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
        types[0].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
        if (xl6IrDynamicCompareRuntimeInstructionCompatible(&module, &call)) return 0;
        types[0].nullAbi = XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    }
    return !xl6IrDynamicCompareRuntimeInstructionCompatible(NULL, &call) &&
           !xl6IrDynamicCompareRuntimeInstructionCompatible(&module, NULL);
}

static int testCollectionReadAbi(void)
{
    XL6_IrModule module;
    XL6_IrType types[2];
    XL6_IrInstruction definitions[2], call;
    XL6_IrValueId operands[2] = {1, 2};
    memset(&module, 0, sizeof(module));
    memset(types, 0, sizeof(types));
    memset(definitions, 0, sizeof(definitions));
    memset(&call, 0, sizeof(call));
    types[0].kind = types[1].kind = XL6_IR_TYPE_BUILTIN;
    types[0].cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[0].nullAbi = XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    types[1].cAbi = XL6_IR_C_ABI_I64;
    types[1].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    module.types.items = types; module.types.count = 2;
    module.instructions = definitions; module.instructionCount = 2;
    module.operands = operands; module.operandCount = 2;
    definitions[0].type = 1; definitions[0].result = 1;
    definitions[1].type = 2; definitions[1].result = 2;
    call.kind = XL6_IR_INSTR_RUNTIME_CALL;
    call.result = 3; call.type = 2; call.operandCount = 1;
    call.runtimeOp = XL6_IR_RUNTIME_XVALUE_COUNT;
    if (!xl6IrCollectionReadRuntimeInstructionCompatible(&module, &call)) return 0;
    call.type = 1;
    if (xl6IrCollectionReadRuntimeInstructionCompatible(&module, &call)) return 0;
    call.runtimeOp = XL6_IR_RUNTIME_XVALUE_ARRAY_AT_I64;
    call.operandCount = 2;
    if (!xl6IrCollectionReadRuntimeInstructionCompatible(&module, &call)) return 0;
    definitions[1].type = 1;
    if (xl6IrCollectionReadRuntimeInstructionCompatible(&module, &call)) return 0;
    definitions[1].type = 2;
    types[0].collectionAbi = XL6_IR_COLLECTION_LIST;
    if (xl6IrCollectionReadRuntimeInstructionCompatible(&module, &call)) return 0;
    types[0].collectionAbi = XL6_IR_COLLECTION_NONE;
    call.firstOperand = SIZE_MAX;
    if (xl6IrCollectionReadRuntimeInstructionCompatible(&module, &call)) return 0;
    call.firstOperand = 0;
    call.payloadType = 1;
    if (xl6IrCollectionReadRuntimeInstructionCompatible(&module, &call)) return 0;
    return 1;
}

static int testDirectBridgeAbi(void)
{
    static const struct {
        char* name;
        XL6_IrTypeId result;
        size_t count;
        XL6_IrTypeId parameters[5];
    } cases[] = {
        { "xl6_xrt_dir_list", 5, 3, {1, 2, 3} },
        { "xl6_xrt_dir_glob", 5, 4, {1, 1, 2, 3} },
        { "xl6_xrt_dir_entries", 5, 5, {1, 2, 2, 1, 3} },
        { "xl6_xrt_dir_is_empty", 2, 1, {1} },
        { "xl6_xrt_dir_count", 4, 3, {1, 2, 3} },
        { "xl6_xrt_dir_size", 4, 2, {1, 2} },
    };
    XL6_IrModule module;
    XL6_IrType types[6];
    XL6_IrCAbi abis[6] = { XL6_IR_C_ABI_MANAGED_STRING, XL6_IR_C_ABI_C_BOOL,
        XL6_IR_C_ABI_C_INT, XL6_IR_C_ABI_I64, XL6_IR_C_ABI_RUNTIME_XVALUE_PTR,
        XL6_IR_C_ABI_RAW_POINTER };
    size_t i, j;
    memset(&module, 0, sizeof(module));
    memset(types, 0, sizeof(types));
    module.types.items = types;
    module.types.count = 6;
    for (i = 0; i < 6; ++i) {
        types[i].kind = XL6_IR_TYPE_BUILTIN;
        types[i].cAbi = abis[i];
        types[i].ownershipAbi = i == 0 || i == 4
            ? XL6_IR_OWNERSHIP_ABI_MANAGED : XL6_IR_OWNERSHIP_ABI_TRIVIAL;
        types[i].nullAbi = i == 4 ? XL6_IR_NULL_ABI_XVALUE_SENTINEL :
            i == 0 || i == 5 ? XL6_IR_NULL_ABI_POINTER_SENTINEL : XL6_IR_NULL_ABI_NONE;
    }
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        XL6_IrCallTarget target;
        XL6_IrTypeId parameters[5];
        XL6_IrParameterAbi ownership[5];
        XL6_IrCallParameterPhysicalAbi physical[5];
        XL6_IrCallParameterConstraintAbi constraints[5];
        XL6_IrNodeId fields[5] = {0};
        memset(&target, 0, sizeof(target));
        target.kind = XL6_IR_CALL_TARGET_EXTERN_C;
        target.cName = cases[i].name;
        target.runtimeBridge = xl6IrRuntimeDirectBridgeKind(target.cName);
        target.returnType = cases[i].result;
        target.parameterCount = cases[i].count;
        target.parameterTypes = parameters;
        target.parameterAbis = ownership;
        target.parameterPhysicalAbis = physical;
        target.parameterConstraintAbis = constraints;
        target.parameterPhysicalFieldDeclarations = fields;
        target.resultPhysicalAbi = XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;
        target.errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NONE;
        target.variadicAbi = XL6_IR_CALL_VARIADIC_NONE;
        for (j = 0; j < cases[i].count; ++j) {
            parameters[j] = cases[i].parameters[j];
            ownership[j] = parameters[j] == 1 ? XL6_IR_PARAMETER_ABI_BORROW : XL6_IR_PARAMETER_ABI_VALUE;
            physical[j] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
            constraints[j] = XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE;
        }
        if (!xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        for (j = 0; j < target.parameterCount; ++j) {
            XL6_IrTypeId savedType = parameters[j];
            XL6_IrParameterAbi savedOwnership = ownership[j];
            parameters[j] = 999;
            if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
            parameters[j] = savedType;
            ownership[j] = XL6_IR_PARAMETER_ABI_OWNED;
            if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
            ownership[j] = savedOwnership;
            fields[j] = 1;
            if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
            fields[j] = 0;
        }
        target.returnType = 1;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        target.returnType = cases[i].result;
        target.parameterCount++;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        target.parameterCount--;
        target.errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        target.errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        target.resultPhysicalDestroySymbol = 7;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        target.resultPhysicalDestroySymbol = 0;
        target.runtimeBridge = XL6_IR_RUNTIME_BRIDGE_NONE;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
    }
    return xl6IrRuntimeDirectBridgeKind("not_a_runtime_bridge") == XL6_IR_RUNTIME_BRIDGE_NONE &&
           testCollectionReadAbi() && testDynamicCompareAbi() && testDeclarationBindings() &&
           testDynamicFieldsBridgeAbi() && testOwnedReadResults();
}
