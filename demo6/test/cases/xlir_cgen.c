#include "src/cgen/xl6_ir_cgen.h"

#include <stdio.h>

static XL6_AstNode* testType(const char* name, XL6_TypeId id, const char* abi)
{
    XL6_AstNode* type = xl6AstNewText(XL6_AST_TYPE, name, strlen(name), NULL);
    type->semanticTypeId = id;
    type->semanticTypeAbiId = xl6TypeAbiIdText(abi);
    return type;
}

static int testRuntimeBridgeOperations(void)
{
    XL6_IrModule module;
    XL6_IrCallTarget target;
    XL6_IrType types[7];
    XL6_IrTypeId payload = 1;
    XL6_IrTypeId parameters[5] = {1, 5, 2, 2, 2};
    XL6_IrParameterAbi ownership[5] = {
        XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_PARAMETER_ABI_BORROW,
        XL6_IR_PARAMETER_ABI_VALUE, XL6_IR_PARAMETER_ABI_VALUE,
        XL6_IR_PARAMETER_ABI_VALUE
    };
    XL6_IrCallParameterPhysicalAbi physical[5] = {
        XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW,
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT,
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT,
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT,
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT
    };
    XL6_IrCallParameterConstraintAbi constraints[5] = {0};
    XL6_IrNodeId fields[5] = {1, 0, 0, 0, 0};
    char* fileNames[] = {
        "xl6_xrt_tcp_stream_send_file_result",
        "xl6_xrt_tcp_stream_send_file_handle_result",
        "xl6_xrt_tcp_stream_send_file",
        "xl6_xrt_tcp_stream_send_file_handle"
    };
    char* receiveNames[] = {
        "xl6_xrt_tcp_stream_recv_owned_async",
        "xl6_xrt_tls_stream_recv_owned_async"
    };
    size_t i;
    memset(&module, 0, sizeof(module));
    memset(&target, 0, sizeof(target));
    memset(types, 0, sizeof(types));
    module.types.items = types;
    module.types.count = sizeof(types) / sizeof(types[0]);
    types[0].kind = XL6_IR_TYPE_CLASS;
    types[0].cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[0].nullAbi = XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    types[1].cAbi = XL6_IR_C_ABI_I64;
    types[2].cAbi = XL6_IR_C_ABI_BOOL;
    types[3].cAbi = XL6_IR_C_ABI_RAW_POINTER;
    types[3].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    types[4].cAbi = XL6_IR_C_ABI_MANAGED_STRING;
    types[4].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[4].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    types[5].cAbi = XL6_IR_C_ABI_C_SIZE;
    for ( i = 1; i < 6; ++i )
        if ( i != 4 ) types[i].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    types[6].cAbi = XL6_IR_C_ABI_RUNTIME_FUTURE_PTR;
    types[6].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[6].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    types[6].params = &payload;
    types[6].paramCount = 1;
    target.kind = XL6_IR_CALL_TARGET_EXTERN_C;
    target.errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
    target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NONE;
    target.resultPhysicalAbi = XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;
    target.parameterTypes = parameters;
    target.parameterAbis = ownership;
    target.parameterPhysicalAbis = physical;
    target.parameterConstraintAbis = constraints;
    target.parameterPhysicalFieldDeclarations = fields;
    target.runtimeBridge = XL6_IR_RUNTIME_BRIDGE_NET_TCP_STREAM;
    for ( i = 0; i < sizeof(fileNames) / sizeof(fileNames[0]); ++i ) {
        XL6_IrTypeId savedParameter;
        target.cName = fileNames[i];
        target.runtimeBridgeOperation = xl6IrRuntimeBridgeOperationForName(target.cName);
        target.parameterCount = i < 2 ? 4 : 5;
        target.returnType = i < 2 ? 2 : 3;
        parameters[1] = i % 2 != 0 ? 4 : 5;
        ownership[1] = i % 2 != 0 ? XL6_IR_PARAMETER_ABI_VALUE : XL6_IR_PARAMETER_ABI_BORROW;
        if ( !xl6IrRuntimeNetTcpBridgeTargetMatches(&module, &target) ) return 0;
        --target.parameterCount;
        if ( xl6IrRuntimeNetTcpBridgeTargetMatches(&module, &target) ) return 0;
        ++target.parameterCount;
        savedParameter = parameters[1];
        parameters[1] = 2;
        if ( xl6IrRuntimeNetTcpBridgeTargetMatches(&module, &target) ) return 0;
        parameters[1] = savedParameter;
        target.returnType = i < 2 ? 3 : 2;
        if ( xl6IrRuntimeNetTcpBridgeTargetMatches(&module, &target) ) return 0;
        target.returnType = i < 2 ? 2 : 3;
        target.runtimeBridgeOperation = XL6_IR_RUNTIME_BRIDGE_OPERATION_NONE;
        if ( xl6IrRuntimeNetTcpBridgeTargetMatches(&module, &target) ) return 0;
    }
    for ( i = 0; i < sizeof(receiveNames) / sizeof(receiveNames[0]); ++i ) {
        target.cName = receiveNames[i];
        target.runtimeBridge = i == 0 ? XL6_IR_RUNTIME_BRIDGE_NET_TCP_STREAM
                                     : XL6_IR_RUNTIME_BRIDGE_NET_TLS_STREAM;
        target.runtimeBridgeOperation = xl6IrRuntimeBridgeOperationForName(target.cName);
        target.parameterCount = 2;
        target.returnType = 7;
        parameters[1] = 6;
        ownership[1] = XL6_IR_PARAMETER_ABI_VALUE;
        if ( !xl6IrRuntimeBridgeOperationTargetMatches(&module, &target) ) return 0;
        payload = 2;
        if ( xl6IrRuntimeBridgeOperationTargetMatches(&module, &target) ) return 0;
        payload = 1;
        parameters[1] = 2;
        if ( xl6IrRuntimeBridgeOperationTargetMatches(&module, &target) ) return 0;
        parameters[1] = 6;
        physical[0] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
        if ( xl6IrRuntimeBridgeOperationTargetMatches(&module, &target) ) return 0;
        physical[0] = XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW;
        ownership[0] = XL6_IR_PARAMETER_ABI_VALUE;
        if ( xl6IrRuntimeBridgeOperationTargetMatches(&module, &target) ) return 0;
        ownership[0] = XL6_IR_PARAMETER_ABI_BORROW;
        target.runtimeBridge = XL6_IR_RUNTIME_BRIDGE_NET_TCP_SERVER;
        if ( xl6IrRuntimeBridgeOperationTargetMatches(&module, &target) ) return 0;
    }
    return 1;
}

static int testSerializationCallbackResultAbi(void)
{
    XL6_IrModule module;
    XL6_IrCallTarget target;
    XL6_IrType types[5];
    XL6_IrTypeId parameters[5] = {1, 5, 3, 3, 3};
    XL6_IrTypeId callbackParameter = 2;
    XL6_IrTypeId callbackResult = 3;
    XL6_IrParameterAbi callbackBorrow = XL6_IR_PARAMETER_ABI_BORROW;
    XL6_IrParameterMode callbackMode = XL6_IR_PARAMETER_MODE_DEFAULT;
    XL6_IrParameterAbi ownership[5] = {
        XL6_IR_PARAMETER_ABI_BORROW, XL6_IR_PARAMETER_ABI_BORROW,
        XL6_IR_PARAMETER_ABI_VALUE, XL6_IR_PARAMETER_ABI_VALUE, XL6_IR_PARAMETER_ABI_VALUE
    };
    XL6_IrCallParameterPhysicalAbi physical[5] = {
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT,
        XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE,
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT, XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT,
        XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT
    };
    XL6_IrCallParameterConstraintAbi constraints[5] = {0};
    XL6_IrNodeId fields[5] = {0};
    char* names[2] = {"xl6_xrt_json_visit", "xl6_xrt_xson_visit_options"};
    size_t i;
    memset(&module, 0, sizeof(module));
    memset(&target, 0, sizeof(target));
    memset(types, 0, sizeof(types));
    module.types.items = types;
    module.types.count = 5;
    for (i = 0; i < 5; ++i) types[i].kind = XL6_IR_TYPE_BUILTIN;
    types[0].cAbi = XL6_IR_C_ABI_MANAGED_STRING;
    types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[0].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    types[1].cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
    types[1].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[1].nullAbi = XL6_IR_NULL_ABI_XVALUE_SENTINEL;
    types[2].cAbi = XL6_IR_C_ABI_BOOL;
    types[2].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    types[2].nullAbi = XL6_IR_NULL_ABI_NONE;
    types[3].cAbi = XL6_IR_C_ABI_C_INT;
    types[3].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    types[3].nullAbi = XL6_IR_NULL_ABI_NONE;
    types[4].kind = XL6_IR_TYPE_FUNCTION;
    types[4].cAbi = XL6_IR_C_ABI_CLOSURE_PTR;
    types[4].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[4].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    types[4].paramCount = types[4].returnCount = 1;
    types[4].params = &callbackParameter;
    types[4].parameterAbis = &callbackBorrow;
    types[4].parameterModes = &callbackMode;
    types[4].returns = &callbackResult;
    types[4].callableResultType = callbackResult;
    target.kind = XL6_IR_CALL_TARGET_EXTERN_C;
    target.returnType = 4;
    target.resultPhysicalAbi = XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;
    target.parameterTypes = parameters;
    target.parameterAbis = ownership;
    target.parameterPhysicalAbis = physical;
    target.parameterConstraintAbis = constraints;
    target.parameterPhysicalFieldDeclarations = fields;
    target.variadicAbi = XL6_IR_CALL_VARIADIC_NONE;
    for (i = 0; i < 2; ++i) {
        target.cName = names[i];
        target.runtimeBridge = xl6IrRuntimeSerializationBridgeKind(names[i]);
        target.parameterCount = i == 0 ? 2 : 5;
        target.errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER;
        if (!xl6IrRuntimeSerializationBridgeHasCallbackResult(target.runtimeBridge) ||
            !xl6IrRuntimeSerializationBridgeTargetMatches(&module, &target)) return 0;
        target.errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
        target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NONE;
        if (xl6IrRuntimeSerializationBridgeTargetMatches(&module, &target)) return 0;
        target.errorAbi = XL6_IR_ERROR_ABI_MAY_RAISE;
        target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;
        if (xl6IrRuntimeSerializationBridgeTargetMatches(&module, &target)) return 0;
        target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_RESULT_CARRIER;
        physical[1] = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
        if (xl6IrRuntimeSerializationBridgeTargetMatches(&module, &target)) return 0;
        physical[1] = XL6_IR_CALL_PARAMETER_PHYSICAL_LANGUAGE_FUNCTION_VALUE;
        callbackResult = types[4].callableResultType = 4;
        if (xl6IrRuntimeSerializationBridgeTargetMatches(&module, &target)) return 0;
        callbackResult = types[4].callableResultType = 3;
        callbackBorrow = XL6_IR_PARAMETER_ABI_OWNED;
        if (xl6IrRuntimeSerializationBridgeTargetMatches(&module, &target)) return 0;
        callbackBorrow = XL6_IR_PARAMETER_ABI_BORROW;
    }
    return 1;
}

static int testErasedCollectionAbi(void)
{
    XL6_IrModule module;
    XL6_IrType types[9];
    XL6_IrInstruction definitions[3];
    XL6_IrInstruction call;
    XL6_IrValueId values[3] = {1, 2, 3};
    XL6_IrTypeId scalarElement = 6;
    XL6_IrTypeId operands[2] = {6, 8};
    size_t i;
    memset(&module, 0, sizeof(module));
    memset(types, 0, sizeof(types));
    memset(definitions, 0, sizeof(definitions));
    memset(&call, 0, sizeof(call));
    module.types.items = types;
    module.types.count = sizeof(types) / sizeof(types[0]);
    module.runtimeTypeMap.fallbackTypeValueId = 100;
    for ( i = 0; i < 5; ++i ) {
        types[i].kind = XL6_IR_TYPE_BUILTIN;
        types[i].cAbi = XL6_IR_C_ABI_RUNTIME_XVALUE_PTR;
        types[i].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
        types[i].nullAbi = XL6_IR_NULL_ABI_XVALUE_SENTINEL;
        types[i].typeValueId = i < 4 ? (uint64)i + 1 : 100;
        types[i].collectionAbi = i < 4
            ? (XL6_IrCollectionAbi)(XL6_IR_COLLECTION_ARRAY + i)
            : XL6_IR_COLLECTION_NONE;
    }
    types[5].kind = types[6].kind = types[7].kind = XL6_IR_TYPE_BUILTIN;
    types[5].cAbi = XL6_IR_C_ABI_I64;
    types[6].cAbi = XL6_IR_C_ABI_BOOL;
    types[7].cAbi = XL6_IR_C_ABI_MANAGED_STRING;
    types[7].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    types[7].nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    types[8] = types[0];
    types[8].kind = XL6_IR_TYPE_GENERIC_INSTANCE;
    types[8].params = &scalarElement;
    types[8].paramCount = 1;
    for ( i = 0; i < 4; ++i ) {
        XL6_IrTypeId receiver = (XL6_IrTypeId)i + 1;
        if ( !xl6IrTypeIsStaticallyShapedErasedCollection(&module, receiver) ||
             xl6IrCollectionElementType(&module, receiver) != 5 ||
             xl6IrCollectionBoxedElementType(&module, receiver) != 5 ||
             !xl6IrTypedCollectionSignatureCompatible(&module,
                 XL6_IR_TYPED_COLLECTION_CLEAR, types[i].collectionAbi,
                 receiver, 7, NULL, 0) ||
             !xl6IrTypedCollectionRuntimeSignatureCompatible(&module,
                 XL6_IR_TYPED_COLLECTION_CLEAR, types[i].collectionAbi,
                 receiver, 7, NULL, 0) ) return 0;
    }
    if ( xl6IrCollectionElementType(&module, 5) != XL6_IR_TYPE_INVALID ||
         xl6IrCollectionElementType(&module, 9) != 6 ||
         xl6IrCollectionBoxedElementType(&module, 9) != 9 ||
         !xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_POP, XL6_IR_COLLECTION_ARRAY,
             1, 5, NULL, 0) ||
         xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_POP, XL6_IR_COLLECTION_ARRAY,
             1, 1, NULL, 0) ||
         xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_POP, XL6_IR_COLLECTION_ARRAY,
             1, 6, NULL, 0) ||
         xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_CLEAR, XL6_IR_COLLECTION_ARRAY,
             5, 7, NULL, 0) ||
         xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_CLEAR, XL6_IR_COLLECTION_DICT,
             1, 7, NULL, 0) ||
         xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_TAKE, XL6_IR_COLLECTION_DICT,
             4, 5, operands, 1) ||
         xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_INDEX_OF, XL6_IR_COLLECTION_ARRAY,
             1, 6, NULL, 1) ||
         !xl6IrTypedCollectionSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_INSERT, XL6_IR_COLLECTION_ARRAY,
             1, 7, operands, 2) ||
         xl6IrTypedCollectionRuntimeSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_INSERT, XL6_IR_COLLECTION_ARRAY,
             1, 7, operands, 2) ||
         !xl6IrCollectionContainsOperandCompatible(&module, 4, 8) ||
         xl6IrCollectionContainsOperandCompatible(&module, 4, 6) ||
         !xl6IrCollectionElementCompatible(&module, 1, 8) ||
         xl6IrCollectionElementCompatible(&module, 9, 8) ) return 0;
    operands[1] = 5;
    if ( !xl6IrTypedCollectionRuntimeSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_INSERT, XL6_IR_COLLECTION_ARRAY,
             1, 7, operands, 2) ||
         xl6IrTypedCollectionRuntimeSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_INSERT, XL6_IR_COLLECTION_ARRAY,
             9, 7, operands, 2) ) return 0;
    operands[1] = 9;
    if ( !xl6IrTypedCollectionRuntimeSignatureCompatible(&module,
             XL6_IR_TYPED_COLLECTION_INSERT, XL6_IR_COLLECTION_ARRAY,
             9, 7, operands, 2) ) return 0;
    types[0].kind = XL6_IR_TYPE_GENERIC_INSTANCE;
    if ( xl6IrCollectionElementType(&module, 1) != 5 ) return 0;
    types[0].nominalSymbol = 1;
    if ( xl6IrCollectionElementType(&module, 1) != XL6_IR_TYPE_INVALID ) return 0;
    types[0].nominalSymbol = 0;
    types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    if ( xl6IrCollectionElementType(&module, 1) != XL6_IR_TYPE_INVALID ) return 0;
    types[0].ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    module.instructions = definitions;
    module.instructionCount = 3;
    module.operands = values;
    module.operandCount = 3;
    for ( i = 0; i < 3; ++i ) definitions[i].result = (XL6_IrValueId)i + 1;
    definitions[0].type = 1;
    definitions[1].type = 6;
    definitions[2].type = 5;
    call.kind = XL6_IR_INSTR_RUNTIME_CALL;
    call.runtimeOp = XL6_IR_RUNTIME_XVALUE_TYPED_COLLECTION_METHOD;
    call.typedCollectionOp = XL6_IR_TYPED_COLLECTION_INSERT;
    call.collectionAbi = XL6_IR_COLLECTION_ARRAY;
    call.type = 7;
    call.result = 4;
    call.operandCount = 3;
    if ( !xl6IrCollectionMemberRuntimeInstructionCompatible(&module, &call) ) return 0;
    definitions[2].type = 1;
    if ( xl6IrCollectionMemberRuntimeInstructionCompatible(&module, &call) ) return 0;
    definitions[2].type = 5;
    call.targetSymbol = 1;
    if ( xl6IrCollectionMemberRuntimeInstructionCompatible(&module, &call) ) return 0;
    call.targetSymbol = 0;
    call.firstOperand = SIZE_MAX;
    if ( xl6IrCollectionMemberRuntimeInstructionCompatible(&module, &call) ) return 0;
    call.firstOperand = 0;
    call.type = 6;
    if ( xl6IrCollectionMemberRuntimeInstructionCompatible(&module, &call) ) return 0;
    return 1;
}

static int testTextOutputAbi(void)
{
    XL6_IrModule module;
    XL6_IrCallTarget target;
    XL6_IrType stringType;
    XL6_IrTypeId parameter = 1;
    XL6_IrParameterAbi borrow = XL6_IR_PARAMETER_ABI_BORROW;
    XL6_IrCallParameterPhysicalAbi direct = XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT;
    XL6_IrCallParameterConstraintAbi constraint = XL6_IR_CALL_PARAMETER_CONSTRAINT_NONE;
    XL6_IrNodeId field = XL6_IR_NODE_INVALID;
    char* names[2] = {"xl6_xrt_write", "xl6_xrt_writeln"};
    size_t i;
    memset(&module, 0, sizeof(module));
    memset(&target, 0, sizeof(target));
    memset(&stringType, 0, sizeof(stringType));
    stringType.kind = XL6_IR_TYPE_BUILTIN;
    stringType.cAbi = XL6_IR_C_ABI_MANAGED_STRING;
    stringType.ownershipAbi = XL6_IR_OWNERSHIP_ABI_MANAGED;
    stringType.nullAbi = XL6_IR_NULL_ABI_POINTER_SENTINEL;
    module.types.items = &stringType;
    module.types.count = 1;
    target.kind = XL6_IR_CALL_TARGET_EXTERN_C;
    target.resultPhysicalAbi = XL6_IR_CALL_RESULT_PHYSICAL_DIRECT;
    target.parameterCount = 1;
    target.parameterTypes = &parameter;
    target.parameterAbis = &borrow;
    target.parameterPhysicalAbis = &direct;
    target.parameterConstraintAbis = &constraint;
    target.parameterPhysicalFieldDeclarations = &field;
    target.variadicAbi = XL6_IR_CALL_VARIADIC_NONE;
    target.errorAbi = XL6_IR_ERROR_ABI_NO_RAISE;
    target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NONE;
    for (i = 0; i < 2; ++i) {
        target.cName = names[i];
        target.runtimeBridge = xl6IrRuntimeDirectBridgeKind(names[i]);
        if (!xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        borrow = XL6_IR_PARAMETER_ABI_OWNED;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        borrow = XL6_IR_PARAMETER_ABI_BORROW;
        target.returnType = 1;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        target.returnType = XL6_IR_TYPE_INVALID;
        stringType.cAbi = XL6_IR_C_ABI_RAW_POINTER;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        stringType.cAbi = XL6_IR_C_ABI_MANAGED_STRING;
        target.parameterCount = 2;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        target.parameterCount = 1;
        target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_XRT_THREAD_ERROR;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
        target.errorPhysicalAbi = XL6_IR_CALL_ERROR_PHYSICAL_NONE;
        target.runtimeBridge = XL6_IR_RUNTIME_BRIDGE_NONE;
        if (xl6IrRuntimeDirectBridgeTargetMatches(&module, &target)) return 0;
    }
    return 1;
}

#include "test/cases/xlir_direct_bridge_unit.c"

int main(void)
{
    XL6_IrModule ir;
    XL6_DiagList diags;
    XL6_Output out;
    XL6_Output repeatOut;
    XL6_AstNode* root = xl6AstNew(XL6_AST_MODULE, NULL);
    /* `answer` is the Cgen root; its two helpers remain reachable by calls. */
    XL6_AstNode* func = xl6AstNew(XL6_AST_EXPORT_FUNC, NULL);
    XL6_AstNode* identity = xl6AstNew(XL6_AST_FUNC, NULL);
    XL6_AstNode* noop = xl6AstNew(XL6_AST_FUNC, NULL);
    char* text;
    char* repeat;
    int ok;

    if ( !testRuntimeBridgeOperations() ) {
        fputs("XLIR runtime bridge operation ABI test failed\n", stderr);
        return 1;
    }
    if ( !testSerializationCallbackResultAbi() ) {
        fputs("XLIR serialization callback result ABI test failed\n", stderr);
        return 1;
    }
    if ( !testErasedCollectionAbi() ) {
        fprintf(stderr, "XLIR erased collection ABI test failed\n");
        return 1;
    }
    if ( !testDirectBridgeAbi() ) {
        fputs("XLIR direct bridge ABI test failed\n", stderr);
        return 1;
    }
    if ( !testTextOutputAbi() ) {
        fputs("XLIR text output ABI test failed\n", stderr);
        return 1;
    }
    memset(&diags, 0, sizeof(diags)); memset(&out, 0, sizeof(out)); memset(&repeatOut, 0, sizeof(repeatOut));
    xl6IrModuleInit(&ir);
    if ( root == NULL || func == NULL || identity == NULL || noop == NULL || !xl6OutputOpenMemory(&out) ) return 1;
    noop->semanticSymbolId = 17;
    xl6AstAddChild(noop, xl6AstNewText(XL6_AST_NAME, "noop", 4, NULL));
    noop->children[0]->inferredType = testType("int", 1, "xrt.int64");
    xl6AstAddChild(noop, xl6AstNew(XL6_AST_LIST, NULL));
    xl6AstAddChild(noop, xl6AstNew(XL6_AST_RETURN_TYPES, NULL));
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
        ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
        xl6AstAddChild(body, ret); xl6AstAddChild(noop, body);
    }
    identity->semanticSymbolId = 15;
    xl6AstAddChild(identity, xl6AstNewText(XL6_AST_NAME, "identity", 8, NULL));
    identity->children[0]->inferredType = testType("int", 1, "xrt.int64");
    {
        XL6_AstNode* params = xl6AstNew(XL6_AST_LIST, NULL);
        XL6_AstNode* param = xl6AstNewText(XL6_AST_PARAM, "x", 1, NULL);
        param->semanticSymbolId = 16;
        xl6AstAddChild(param, testType("int", 1, "xrt.int64"));
        xl6AstAddChild(params, param);
        xl6AstAddChild(identity, params);
    }
    {
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        xl6AstAddChild(returns, testType("int", 1, "xrt.int64"));
        xl6AstAddChild(identity, returns);
    }
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
        XL6_AstNode* name = xl6AstNewText(XL6_AST_NAME, "x", 1, NULL);
        name->inferredType = testType("int", 1, "xrt.int64");
        name->semanticTargetSymbolId = 16;
        name->semanticOwnership = XL6_SEM_OWNERSHIP_BORROW;
        ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
        xl6AstAddChild(ret, name); xl6AstAddChild(body, ret); xl6AstAddChild(identity, body);
    }
    func->semanticSymbolId = 14;
    xl6AstAddChild(func, xl6AstNewText(XL6_AST_NAME, "answer", 6, NULL));
    func->children[0]->inferredType = testType("int", 1, "xrt.int64");
    xl6AstAddChild(func, xl6AstNew(XL6_AST_LIST, NULL));
    {
        XL6_AstNode* returns = xl6AstNew(XL6_AST_RETURN_TYPES, NULL);
        /* The semantic layer permits this ABI-equivalent alias while the
         * expression remains inferred as `int`.  Direct XLIR construction
         * must canonicalize both spellings before its return ABI verifier. */
        xl6AstAddChild(returns, testType("int64", 2, "xrt.int64"));
        xl6AstAddChild(func, returns);
    }
    {
        XL6_AstNode* body = xl6AstNew(XL6_AST_BLOCK, NULL);
        XL6_AstNode* voidCall = xl6AstNew(XL6_AST_CALL, NULL);
        XL6_AstNode* voidCallee = xl6AstNewText(XL6_AST_NAME, "noop", 4, NULL);
        XL6_AstNode* voidArgs = xl6AstNew(XL6_AST_LIST, NULL);
        XL6_AstNode* local = xl6AstNewText(XL6_AST_VAR_DECL, "temp", 4, NULL);
        XL6_AstNode* localType = xl6AstNewText(XL6_AST_TYPE, "auto", 4, NULL);
        XL6_AstNode* ret = xl6AstNew(XL6_AST_RETURN, NULL);
        XL6_AstNode* value = xl6AstNew(XL6_AST_CALL, NULL);
        XL6_AstNode* callee = xl6AstNewText(XL6_AST_NAME, "identity", 8, NULL);
        XL6_AstNode* args = xl6AstNew(XL6_AST_LIST, NULL);
        XL6_AstNode* argument = xl6AstNew(XL6_AST_ARG, NULL);
        XL6_AstNode* binary = xl6AstNewText(XL6_AST_BINARY, "+", 1, NULL);
        XL6_AstNode* left = xl6AstNewText(XL6_AST_INT, "40", 2, NULL);
        XL6_AstNode* right = xl6AstNewText(XL6_AST_INT, "2", 1, NULL);
        XL6_AstNode* localName = xl6AstNewText(XL6_AST_NAME, "temp", 4, NULL);
        local->semanticSymbolId = 18;
        local->inferredType = testType("int", 1, "xrt.int64");
        voidCallee->inferredType = testType("void", 3, "xrt.void");
        /* AST preserves the semantic void type; lowering normalizes it to a
         * CALL without XLIR result value. */
        voidCall->inferredType = testType("void", 3, "xrt.void");
        voidCall->semanticCallKind = XL6_SEM_CALL_DIRECT;
        voidCall->semanticTargetSymbolId = 17;
        left->inferredType = testType("int", 1, "xrt.int64");
        right->inferredType = testType("int", 1, "xrt.int64");
        left->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        right->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        binary->inferredType = testType("int", 1, "xrt.int64");
        binary->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        xl6AstAddChild(binary, left); xl6AstAddChild(binary, right);
        callee->inferredType = testType("int", 1, "xrt.int64");
        value->inferredType = testType("int", 1, "xrt.int64");
        value->semanticOwnership = XL6_SEM_OWNERSHIP_COPY;
        value->semanticCallKind = XL6_SEM_CALL_DIRECT;
        value->semanticTargetSymbolId = 15;
        localName->inferredType = testType("int", 1, "xrt.int64");
        localName->semanticTargetSymbolId = 18;
        localName->semanticOwnership = XL6_SEM_OWNERSHIP_BORROW;
        xl6AstAddChild(local, localType); xl6AstAddChild(local, binary);
        xl6AstAddChild(voidCall, voidCallee); xl6AstAddChild(voidCall, voidArgs);
        xl6AstAddChild(argument, localName); xl6AstAddChild(args, argument);
        xl6AstAddChild(value, callee); xl6AstAddChild(value, args);
        ret->semanticExitKind = XL6_SEM_EXIT_RETURN;
        xl6AstAddChild(ret, value); xl6AstAddChild(body, voidCall); xl6AstAddChild(body, local); xl6AstAddChild(body, ret); xl6AstAddChild(func, body);
    }
    xl6AstAddChild(root, noop);
    xl6AstAddChild(root, identity);
    xl6AstAddChild(root, func);
    ok = xl6IrBuildModuleFromAst(&ir, root, &diags) &&
         xl6IrLowerModule(&ir, &diags) && xl6IrCgenEmitC(&out, &ir, &diags);
    text = xl6OutputTakeMemoryText(&out, NULL);
    ok = ok && text != NULL && strstr(text, "static void xlir_fn_17(void)") != NULL &&
         strstr(text, "static int64_t xlir_fn_15(int64_t s16)") != NULL &&
         strstr(text, "static int64_t xlir_fn_14(void)") != NULL &&
         strstr(text, "    int64_t s18;\n") != NULL &&
         strstr(text, "    xlir_fn_17();\n") != NULL &&
         strstr(text, "v2 = 40LL;") != NULL && strstr(text, "v3 = 2LL;") != NULL &&
         strstr(text, "v4 = v2 + v3;") != NULL && strstr(text, "s18 = v4;") != NULL &&
         strstr(text, "v5 = s18;") != NULL && strstr(text, "v6 = xlir_fn_15(v5);") != NULL &&
         strstr(text, "return v6;") != NULL && strstr(text, "    return;\n") != NULL;
    repeat = NULL;
    if ( ok && xl6OutputOpenMemory(&repeatOut) ) {
        ok = xl6IrCgenEmitC(&repeatOut, &ir, &diags);
        repeat = xl6OutputTakeMemoryText(&repeatOut, NULL);
        ok = ok && repeat != NULL && strcmp(text, repeat) == 0;
    } else {
        ok = 0;
    }
    if ( ok ) {
        FILE* generated = fopen("build/test/xlir_cgen_generated.c", "wb");
        if ( generated == NULL || fputs(text, generated) < 0 || fclose(generated) != 0 ) ok = 0;
    }
    if ( ok ) {
        size_t index;
        XL6_IrNodeId origin = XL6_IR_NODE_INVALID;
        char* savedText;
        XL6_Output opcodeOut;
        char* opcodeText;
        memset(&opcodeOut, 0, sizeof(opcodeOut));
        for ( index = ir.functions[3].firstInstruction;
              index < ir.functions[3].firstInstruction + ir.functions[3].instructionCount; ++index ) {
            if ( ir.instructions[index].kind == XL6_IR_INSTR_BINARY ) {
                origin = ir.instructions[index].origin;
                break;
            }
        }
        savedText = origin != XL6_IR_NODE_INVALID ? ir.nodes[origin - 1].text : NULL;
        if ( origin != XL6_IR_NODE_INVALID ) ir.nodes[origin - 1].text = "source-text-must-not-drive-cgen";
        ok = origin != XL6_IR_NODE_INVALID && xl6OutputOpenMemory(&opcodeOut) &&
             xl6IrCgenEmitC(&opcodeOut, &ir, &diags);
        opcodeText = xl6OutputTakeMemoryText(&opcodeOut, NULL);
        ok = ok && opcodeText != NULL && strstr(opcodeText, "v4 = v2 + v3;") != NULL;
        xrtFree(opcodeText);
        if ( origin != XL6_IR_NODE_INVALID ) ir.nodes[origin - 1].text = savedText;
    }
    if ( ok ) {
        size_t index;
        XL6_IrNodeId origin = XL6_IR_NODE_INVALID;
        char* savedText;
        XL6_Output immediateOut;
        char* immediateText;
        memset(&immediateOut, 0, sizeof(immediateOut));
        for ( index = ir.functions[3].firstInstruction;
              index < ir.functions[3].firstInstruction + ir.functions[3].instructionCount; ++index ) {
            if ( ir.instructions[index].kind == XL6_IR_INSTR_CONST ) {
                origin = ir.instructions[index].origin;
                break;
            }
        }
        savedText = origin != XL6_IR_NODE_INVALID ? ir.nodes[origin - 1].text : NULL;
        if ( origin != XL6_IR_NODE_INVALID ) ir.nodes[origin - 1].text = "999";
        ok = origin != XL6_IR_NODE_INVALID && xl6OutputOpenMemory(&immediateOut) &&
             xl6IrCgenEmitC(&immediateOut, &ir, &diags);
        immediateText = xl6OutputTakeMemoryText(&immediateOut, NULL);
        ok = ok && immediateText != NULL && strstr(immediateText, "v2 = 40LL;") != NULL &&
             strstr(immediateText, "v2 = 999;") == NULL;
        xrtFree(immediateText);
        if ( origin != XL6_IR_NODE_INVALID ) ir.nodes[origin - 1].text = savedText;
    }
    if ( ok ) {
        XL6_Output invalidOut;
        XL6_IrInstruction saved = ir.instructions[ir.functions[3].firstInstruction];
        char* ignored;
        memset(&invalidOut, 0, sizeof(invalidOut));
        ir.instructions[ir.functions[3].firstInstruction].kind = XL6_IR_INSTR_CALL;
        ir.instructions[ir.functions[3].firstInstruction].targetSymbol = 999;
        ir.instructions[ir.functions[3].firstInstruction].operandCount = 0;
        ok = xl6OutputOpenMemory(&invalidOut) && !xl6IrCgenEmitC(&invalidOut, &ir, &diags) && diags.count > 0;
        ignored = xl6OutputTakeMemoryText(&invalidOut, NULL);
        xrtFree(ignored);
        ir.instructions[ir.functions[3].firstInstruction] = saved;
    }
    if ( ok ) {
        XL6_Output invalidOut;
        XL6_IrCleanupAction cleanup;
        XL6_IrCleanupAction* savedActions = ir.cleanupActions;
        size_t savedCount = ir.cleanupActionCount;
        size_t savedCapacity = ir.cleanupActionCapacity;
        XL6_IrCleanupAction* extendedActions;
        const XL6_IrStorage* storage = xl6IrStorageGet(&ir, 18);
        const XL6_IrNode* cleanupOwner = xl6IrSymbolNodeGet(&ir, 14);
        char* ignored;
        memset(&cleanup, 0, sizeof(cleanup));
        memset(&invalidOut, 0, sizeof(invalidOut));
        cleanup.kind = XL6_IR_CLEANUP_DROP;
        cleanup.type = storage != NULL ? storage->type : XL6_IR_TYPE_INVALID;
        cleanup.targetSymbol = 18;
        cleanup.ownerDeclaration = cleanupOwner != NULL
            ? (XL6_IrNodeId)(cleanupOwner - ir.nodes + 1)
            : XL6_IR_NODE_INVALID;
        cleanup.origin = storage != NULL ? storage->declaration : XL6_IR_NODE_INVALID;
        /* Preserve canonical scope slices. Replacing the entire arena now
         * breaks the plan contract first and cannot test materialization. */
        extendedActions = (XL6_IrCleanupAction*)xrtMalloc(
            (savedCount + 1) * sizeof(*extendedActions));
        ok = extendedActions != NULL;
        if (extendedActions != NULL) {
            if (savedCount != 0) memcpy(extendedActions, savedActions,
                savedCount * sizeof(*extendedActions));
            extendedActions[savedCount] = cleanup;
            ir.cleanupActions = extendedActions;
            ir.cleanupActionCount = ir.cleanupActionCapacity = savedCount + 1;
            ok = storage != NULL && cleanupOwner != NULL && xl6OutputOpenMemory(&invalidOut) &&
                 !xl6IrCgenEmitC(&invalidOut, &ir, &diags) && diags.count > 0 &&
                 strstr(diags.items[diags.count - 1].message,
                        "XLIR lowered cleanup action has no materialized ownership instruction") != NULL;
            ignored = xl6OutputTakeMemoryText(&invalidOut, NULL);
            ok = ok && (ignored == NULL || ignored[0] == '\0');
            xrtFree(ignored);
            ir.cleanupActions = savedActions;
            ir.cleanupActionCount = savedCount;
            ir.cleanupActionCapacity = savedCapacity;
            xrtFree(extendedActions);
            ok = ok && xl6IrVerifyModule(&ir, NULL);
        }
    }
    if ( ok ) {
        XL6_Output invalidOut;
        XL6_IrTypeId unsupportedType = ir.functions[3].returnType;
        XL6_IrType* unsupported = unsupportedType != XL6_IR_TYPE_INVALID ?
            &ir.types.items[unsupportedType - 1] : NULL;
        XL6_IrCAbi savedAbi = unsupported != NULL ? unsupported->cAbi : XL6_IR_C_ABI_UNSUPPORTED;
        char* ignored;
        memset(&invalidOut, 0, sizeof(invalidOut));
        if ( unsupported != NULL ) unsupported->cAbi = XL6_IR_C_ABI_UNSUPPORTED;
        ok = unsupported != NULL && xl6OutputOpenMemory(&invalidOut) && !xl6IrCgenEmitC(&invalidOut, &ir, &diags) &&
             diags.count > 0 && strstr(diags.items[diags.count - 1].message,
                                       "XLIR type pool is malformed") != NULL;
        ignored = xl6OutputTakeMemoryText(&invalidOut, NULL);
        xrtFree(ignored);
        if ( unsupported != NULL ) unsupported->cAbi = savedAbi;
    }
    if ( ok ) {
        size_t index;
        XL6_IrInstruction saved;
        int found = 0;
        for ( index = ir.functions[3].firstInstruction;
              index < ir.functions[3].firstInstruction + ir.functions[3].instructionCount; ++index ) {
            if ( ir.instructions[index].kind == XL6_IR_INSTR_LOAD ) {
                saved = ir.instructions[index];
                ir.instructions[index].targetSymbol = 16; /* identity's parameter, not answer's local. */
                found = 1;
                break;
            }
        }
        ok = found && !xl6IrVerifyModule(&ir, NULL);
        if ( found ) ir.instructions[index] = saved;
    }
    if ( ok ) {
        size_t index;
        XL6_IrTypeId saved = XL6_IR_TYPE_INVALID;
        XL6_IrTypeId voidType = ir.instructions[ir.functions[3].firstInstruction].type;
        int found = 0;
        for ( index = 0; index < ir.storageCount; ++index ) {
            if ( ir.storages[index].symbol == 16 ) {
                saved = ir.storages[index].type;
                ir.storages[index].type = voidType;
                found = 1;
                break;
            }
        }
        ok = found && !xl6IrVerifyModule(&ir, NULL);
        if ( found ) ir.storages[index].type = saved;
    }
    if ( ok ) {
        XL6_IrInstruction saved = ir.instructions[ir.functions[3].firstInstruction];
        ir.instructions[ir.functions[3].firstInstruction].result = 1;
        ok = !xl6IrVerifyModule(&ir, NULL);
        ir.instructions[ir.functions[3].firstInstruction] = saved;
    }
    if ( ok ) {
        XL6_IrRuntimeBridgeOperation saved = ir.callTargets[0].runtimeBridgeOperation;
        ir.callTargets[0].runtimeBridgeOperation =
            XL6_IR_RUNTIME_BRIDGE_OPERATION_NET_TCP_FILE_TRANSFER;
        ok = !xl6IrVerifyModule(&ir, NULL);
        ir.callTargets[0].runtimeBridgeOperation = saved;
        ok = ok && xl6IrVerifyModule(&ir, NULL);
    }
    if ( !ok ) fprintf(stderr, "XLIR C emitter test failed: %s output=%s\n",
                       diags.count > 0 ? diags.items[diags.count - 1].message : "<none>",
                       text != NULL ? text : "<null>");
    xrtFree(repeat); xrtFree(text); xl6IrModuleFree(&ir); xl6AstFree(root); xrtFree(diags.items);
    if ( !ok ) return 1;
    puts("XLIR C emitter test ok");
    return 0;
}
