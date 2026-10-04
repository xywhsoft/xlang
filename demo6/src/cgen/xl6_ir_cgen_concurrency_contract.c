static const char* xl6IrCgenFutureMemberHelper(XL6_IrRuntimeOp operation);
static unsigned int xl6IrCgenFutureMemberMask(XL6_IrRuntimeOp operation);
static int xl6IrCgenRuntimeFutureMemberAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction);
static int xl6IrCgenEmitFutureMemberRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction);

/* A runtime resource is not an open xvalue.  These checks consume the exact
 * nominal resource identity and callable signature carried by Lowered XLIR;
 * no source callee/member spelling is available or reconstructed here. */
static int xl6IrCgenThreadTypeAbi(const XL6_IrType* type)
{
    return type != NULL &&
           type->runtimeResourceAbi == XL6_IR_RUNTIME_RESOURCE_THREAD &&
           type->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           type->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           type->typeValueId != 0;
}

static int xl6IrCgenThreadCallableAbi(
    const XL6_IrModule* module, XL6_IrTypeId type)
{
    const XL6_IrType* signature = xl6IrTypeGet(&module->types, type);
    const XL6_IrType* result = signature != NULL &&
            signature->returnCount == 1 && signature->returns != NULL
        ? xl6IrTypeGet(&module->types, signature->returns[0]) : NULL;
    return signature != NULL &&
           signature->kind == XL6_IR_TYPE_FUNCTION &&
           signature->cAbi == XL6_IR_C_ABI_CLOSURE_PTR &&
           signature->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           signature->callableErasureAbi == XL6_IR_CALLABLE_ERASURE_FIXED &&
           signature->paramCount == 0 && signature->returnCount == 1 &&
           signature->callableResultType == signature->returns[0] &&
           result != NULL && result->cAbi == XL6_IR_C_ABI_I64 &&
           result->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
           result->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64;
}

static int xl6IrCgenThreadRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation >= XL6_IR_RUNTIME_THREAD_CREATE &&
           operation <= XL6_IR_RUNTIME_THREAD_STOP;
}

static int xl6IrCgenRuntimeThreadAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* receiver;
    const XL6_IrType* receiverType;
    const XL6_IrType* resultType;
    int takesTimeout;
    int returnsBool;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenThreadRuntimeOpIs(instruction->runtimeOp) ||
         instruction->result == XL6_IR_VALUE_INVALID ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ) return 0;
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_THREAD_CREATE ) {
        const XL6_IrInstruction* callable;
        if ( instruction->operandCount != 1 ||
             !xl6IrCgenThreadTypeAbi(
                 xl6IrTypeGet(&module->types, instruction->type)) ||
             instruction->calleeType == XL6_IR_TYPE_INVALID ||
             !xl6IrCgenThreadCallableAbi(module, instruction->calleeType) )
            return 0;
        callable = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand], NULL);
        return callable != NULL && callable->type == instruction->calleeType;
    }
    if ( instruction->calleeType != XL6_IR_TYPE_INVALID ) return 0;
    takesTimeout = instruction->runtimeOp ==
        XL6_IR_RUNTIME_THREAD_WAIT_TIMEOUT;
    if ( instruction->operandCount != (takesTimeout ? 2u : 1u) ) return 0;
    receiver = xl6IrCgenValueDefinition(
        module, module->operands[instruction->firstOperand], NULL);
    receiverType = receiver != NULL
        ? xl6IrTypeGet(&module->types, receiver->type) : NULL;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    returnsBool = instruction->runtimeOp == XL6_IR_RUNTIME_THREAD_WAIT ||
                  instruction->runtimeOp ==
                      XL6_IR_RUNTIME_THREAD_WAIT_TIMEOUT ||
                  instruction->runtimeOp == XL6_IR_RUNTIME_THREAD_STOP;
    if ( receiver == NULL || !xl6IrCgenThreadTypeAbi(receiverType) ||
         resultType == NULL ||
         (returnsBool
            ? (resultType->cAbi != XL6_IR_C_ABI_BOOL ||
               resultType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL)
            : (resultType->cAbi != XL6_IR_C_ABI_I64 ||
               resultType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL ||
               resultType->integerTarget !=
                   XL6_IR_INTEGER_TARGET_SIGNED_64)) ) return 0;
    if ( takesTimeout ) {
        const XL6_IrInstruction* timeout = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand + 1], NULL);
        const XL6_IrType* timeoutType = timeout != NULL
            ? xl6IrTypeGet(&module->types, timeout->type) : NULL;
        if ( timeoutType == NULL || timeoutType->cAbi != XL6_IR_C_ABI_I64 ||
             timeoutType->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL )
            return 0;
    }
    return 1;
}

static int xl6IrCgenEmitThreadRuntime(
    XL6_Output* out, const XL6_IrModule* module,
    const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* receiver;
    const XL6_IrType* threadType;
    XL6_IrValueId first;
    if ( instruction == NULL ||
         !xl6IrCgenThreadRuntimeOpIs(instruction->runtimeOp) ) return 0;
    if ( !xl6IrCgenRuntimeThreadAbi(module, instruction) ) return -1;
    first = module->operands[instruction->firstOperand];
    receiver = instruction->runtimeOp == XL6_IR_RUNTIME_THREAD_CREATE
        ? NULL : xl6IrCgenValueDefinition(module, first, NULL);
    threadType = xl6IrTypeGet(&module->types,
        receiver != NULL ? receiver->type : instruction->type);
    switch ( instruction->runtimeOp ) {
    case XL6_IR_RUNTIME_THREAD_CREATE:
        xl6OutputPrintf(out,
            "    v%u = xlir_thread_create(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_WAIT:
        xl6OutputPrintf(out,
            "    v%u = xlir_thread_wait(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_WAIT_TIMEOUT:
        xl6OutputPrintf(out,
            "    v%u = xlir_thread_wait_timeout(v%u, 0x%016llXULL, v%u);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_THREAD_EXIT_CODE:
        xl6OutputPrintf(out,
            "    v%u = xlir_thread_exit_code(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_STATE:
        xl6OutputPrintf(out,
            "    v%u = xlir_thread_state(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_STOP:
        xl6OutputPrintf(out,
            "    v%u = xlir_thread_stop(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    default:
        return 0;
    }
    return xl6OutputHasFailed(out) ? -1 : 1;
}

static int xl6IrCgenChannelRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation >= XL6_IR_RUNTIME_CHANNEL_CREATE &&
           operation <= XL6_IR_RUNTIME_CHANNEL_COUNT;
}

static int xl6IrCgenSpecialQueueRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return (operation >= XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE &&
            operation <= XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED) ||
           operation == XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH ||
           operation == XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_BATCH_INTO;
}

static int xl6IrCgenStackRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation >= XL6_IR_RUNTIME_STACK_CREATE &&
           operation <= XL6_IR_RUNTIME_STACK_CAPACITY;
}

static int xl6IrCgenAvlRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation >= XL6_IR_RUNTIME_AVL_TREE_CREATE &&
           operation <= XL6_IR_RUNTIME_AVL_TREE_CLONE;
}

static int xl6IrCgenAvlRuntimeOpMayRaise(XL6_IrRuntimeOp operation)
{
    return operation == XL6_IR_RUNTIME_AVL_TREE_CREATE ||
           operation == XL6_IR_RUNTIME_AVL_TREE_SET ||
           operation == XL6_IR_RUNTIME_AVL_TREE_GET_INTO ||
           operation == XL6_IR_RUNTIME_AVL_TREE_FIRST_INTO ||
           operation == XL6_IR_RUNTIME_AVL_TREE_LAST_INTO ||
           operation == XL6_IR_RUNTIME_AVL_TREE_LOWER_BOUND_INTO ||
           operation == XL6_IR_RUNTIME_AVL_TREE_UPPER_BOUND_INTO ||
           operation == XL6_IR_RUNTIME_AVL_TREE_MERGE ||
           operation == XL6_IR_RUNTIME_AVL_TREE_KEYS ||
           operation == XL6_IR_RUNTIME_AVL_TREE_VALUES ||
           operation == XL6_IR_RUNTIME_AVL_TREE_CLONE;
}

static int xl6IrCgenSpecialQueueResourceAbiIs(
    XL6_IrRuntimeResourceAbi abi)
{
    return abi >= XL6_IR_RUNTIME_RESOURCE_SPSC_QUEUE &&
           abi <= XL6_IR_RUNTIME_RESOURCE_WAIT_QUEUE;
}

static XL6_IrRuntimeResourcePayloadAbi xl6IrCgenChannelPayloadAbi(
    const XL6_IrType* type)
{
    return xl6IrTypeResourcePayloadAbi(type);
}

static int xl6IrCgenChannelTypeAbi(
    const XL6_IrModule* module, XL6_IrTypeId typeId,
    const XL6_IrType** channelOut, const XL6_IrType** payloadOut)
{
    const XL6_IrType* channel = module != NULL
        ? xl6IrTypeGet(&module->types, typeId) : NULL;
    const XL6_IrType* payload = channel != NULL &&
            channel->paramCount == 1 && channel->params != NULL
        ? xl6IrTypeGet(&module->types, channel->params[0]) : NULL;
    if ( channelOut != NULL ) *channelOut = channel;
    if ( payloadOut != NULL ) *payloadOut = payload;
    return channel != NULL &&
           channel->kind == XL6_IR_TYPE_GENERIC_INSTANCE &&
           channel->runtimeResourceAbi == XL6_IR_RUNTIME_RESOURCE_CHANNEL &&
           channel->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           channel->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           channel->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           channel->typeValueId != 0 &&
           channel->paramCount == 1 && channel->params != NULL &&
           payload != NULL && payload->typeValueId != 0 &&
           xl6IrCgenChannelPayloadAbi(payload) !=
               XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID &&
           xl6IrCgenType(module, channel->params[0]) != NULL;
}

static int xl6IrCgenSpecialQueueTypeAbi(
    const XL6_IrModule* module, XL6_IrTypeId typeId,
    const XL6_IrType** queueOut, const XL6_IrType** payloadOut)
{
    const XL6_IrType* queue = module != NULL
        ? xl6IrTypeGet(&module->types, typeId) : NULL;
    const XL6_IrType* payload = queue != NULL &&
            queue->paramCount == 1 && queue->params != NULL
        ? xl6IrTypeGet(&module->types, queue->params[0]) : NULL;
    if ( queueOut != NULL ) *queueOut = queue;
    if ( payloadOut != NULL ) *payloadOut = payload;
    return queue != NULL &&
           queue->kind == XL6_IR_TYPE_GENERIC_INSTANCE &&
           xl6IrCgenSpecialQueueResourceAbiIs(
               queue->runtimeResourceAbi) &&
           queue->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           queue->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           queue->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           queue->typeValueId != 0 &&
           queue->paramCount == 1 && queue->params != NULL &&
           payload != NULL && payload->typeValueId != 0 &&
           xl6IrCgenChannelPayloadAbi(payload) !=
               XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID &&
           xl6IrCgenType(module, queue->params[0]) != NULL;
}

static int xl6IrCgenExactArrayPayloadTypeAbi(
    const XL6_IrModule* module, XL6_IrTypeId arrayTypeId,
    XL6_IrTypeId payloadTypeId, const XL6_IrType** arrayOut)
{
    const XL6_IrType* array = module != NULL
        ? xl6IrTypeGet(&module->types, arrayTypeId) : NULL;
    if ( arrayOut != NULL ) *arrayOut = array;
    return array != NULL &&
           array->kind == XL6_IR_TYPE_GENERIC_INSTANCE &&
           array->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           array->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           array->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           array->collectionAbi == XL6_IR_COLLECTION_ARRAY &&
           array->paramCount == 1 && array->params != NULL &&
           array->params[0] == payloadTypeId && array->typeValueId != 0;
}

static int xl6IrCgenStackTypeAbi(
    const XL6_IrModule* module, XL6_IrTypeId typeId,
    const XL6_IrType** stackOut, const XL6_IrType** payloadOut)
{
    const XL6_IrType* stack = module != NULL
        ? xl6IrTypeGet(&module->types, typeId) : NULL;
    const XL6_IrType* payload = stack != NULL &&
            stack->paramCount == 1 && stack->params != NULL
        ? xl6IrTypeGet(&module->types, stack->params[0]) : NULL;
    if ( stackOut != NULL ) *stackOut = stack;
    if ( payloadOut != NULL ) *payloadOut = payload;
    return stack != NULL &&
           stack->kind == XL6_IR_TYPE_GENERIC_INSTANCE &&
           stack->runtimeResourceAbi == XL6_IR_RUNTIME_RESOURCE_STACK &&
           stack->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           stack->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           stack->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           stack->typeValueId != 0 &&
           stack->paramCount == 1 && stack->params != NULL &&
           payload != NULL && payload->typeValueId != 0 &&
           xl6IrCgenChannelPayloadAbi(payload) !=
               XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID &&
           xl6IrCgenType(module, stack->params[0]) != NULL;
}

static XL6_IrRuntimeResourceKeyOrderAbi xl6IrCgenAvlKeyOrderAbi(
    const XL6_IrType* type)
{
    if ( type == NULL || type->typeValueId == 0 )
        return XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID;
    switch ( type->cAbi ) {
    case XL6_IR_C_ABI_BOOL:
        return XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_BOOL;
    case XL6_IR_C_ABI_I64:
    case XL6_IR_C_ABI_TIME_I64:
        return XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_SIGNED_I64;
    case XL6_IR_C_ABI_U64:
        return XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_UNSIGNED_U64;
    case XL6_IR_C_ABI_F32:
    case XL6_IR_C_ABI_F64:
        return XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_FLOAT_TOTAL;
    case XL6_IR_C_ABI_MANAGED_STRING:
        return type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED
            ? XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_STRING
            : XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID;
    case XL6_IR_C_ABI_TYPE_ID:
        return XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_TYPE_ID;
    default:
        return XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID;
    }
}

/* xrtTypedTree owns values through immutable xrttype descriptors.  This map
 * is physical ABI only; no source spelling or AST method name reaches Cgen. */
static const char* xl6IrCgenNativeValueConverterExpr(const XL6_IrModule* module, XL6_IrTypeId typeId)
{
    const XL6_IrType* type = xl6IrTypeGet(&module->types,typeId);
    return type != NULL && type->kind == XL6_IR_TYPE_NULLABLE &&
        type->cAbi == XL6_IR_C_ABI_MANAGED_STRING
        ? "xl6NullableStringConverterV1()" : "NULL";
}
static const char* xl6IrCgenNativeValueTypeExpr(const XL6_IrModule* module, XL6_IrTypeId typeId)
{
    const XL6_IrType* type = module != NULL ? xl6IrTypeGet(&module->types, typeId) : NULL;
    if ( type == NULL || type->typeValueId == 0 ) return NULL;
    switch ( type->cAbi ) {
    case XL6_IR_C_ABI_TAGGED_NULLABLE:
        if ( type->kind != XL6_IR_TYPE_NULLABLE || type->paramCount != 1 || type->params == NULL ) return NULL;
        switch ( xl6IrNullableScalarElementAbi(module, typeId) ) {
        case XL6_IR_C_ABI_BOOL: return "xl6NullableBoolTypeV1()";
        case XL6_IR_C_ABI_I64: return "xl6NullableI64TypeV1()";
        case XL6_IR_C_ABI_U64: return "xl6NullableU64TypeV1()";
        case XL6_IR_C_ABI_F32: return "xl6NullableF32TypeV1()";
        case XL6_IR_C_ABI_F64: return "xl6NullableF64TypeV1()";
        case XL6_IR_C_ABI_TIME_I64: return "xl6NullableTimeTypeV1()";
        case XL6_IR_C_ABI_CHAR_U32: return "xl6NullableCharTypeV1()";
        default: return NULL;
        }
    case XL6_IR_C_ABI_BOOL: return "xrtTypeBool()";
    case XL6_IR_C_ABI_I64: return "xrtTypeInt64()";
    case XL6_IR_C_ABI_U64: return "xrtTypeUInt64()";
    case XL6_IR_C_ABI_F64: return "xrtTypeFloat64()";
    case XL6_IR_C_ABI_TIME_I64: return "xrtTypeTime()";
    case XL6_IR_C_ABI_TYPE_ID: return "xrtTypeType()";
    case XL6_IR_C_ABI_CHAR_U32: return "xrtTypeUInt32()";
    case XL6_IR_C_ABI_MANAGED_STRING:
        return type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED
            ? (type->kind == XL6_IR_TYPE_NULLABLE
                ? "xl6NullableStringTypeV1()" : "xrtTypeStringView()") : NULL;
    case XL6_IR_C_ABI_RUNTIME_XVALUE_PTR:
    case XL6_IR_C_ABI_MANAGED_BYTES:
        return type->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED
            ? "xrtTypeValue()" : NULL;
    case XL6_IR_C_ABI_F32: return "xrtTypeFloat32()";
    case XL6_IR_C_ABI_C_CHAR:
        return "(CHAR_MIN < 0 ? xrtTypeInt8() : xrtTypeUInt8())";
    case XL6_IR_C_ABI_C_SCHAR: return "xrtTypeInt8()";
    case XL6_IR_C_ABI_C_UCHAR: return "xrtTypeUInt8()";
    case XL6_IR_C_ABI_C_SHORT: return "xrtTypeInt16()";
    case XL6_IR_C_ABI_C_USHORT: return "xrtTypeUInt16()";
    case XL6_IR_C_ABI_C_INT: return "xrtTypeInt32()";
    case XL6_IR_C_ABI_C_UINT: return "xrtTypeUInt32()";
    case XL6_IR_C_ABI_C_LONG:
        return "(sizeof(long) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_C_ABI_C_ULONG:
        return "(sizeof(unsigned long) == 4 ? xrtTypeUInt32() : xrtTypeUInt64())";
    case XL6_IR_C_ABI_C_LONGLONG: return "xrtTypeInt64()";
    case XL6_IR_C_ABI_C_ULONGLONG: return "xrtTypeUInt64()";
    case XL6_IR_C_ABI_C_SIZE:
        return "(sizeof(size_t) == 4 ? xrtTypeUInt32() : xrtTypeUInt64())";
    case XL6_IR_C_ABI_C_SSIZE:
        return "(sizeof(xl6_ssize_t) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_C_ABI_C_PTRDIFF:
        return "(sizeof(ptrdiff_t) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_C_ABI_C_INTPTR:
        return "(sizeof(intptr_t) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_C_ABI_C_UINTPTR:
        return "(sizeof(uintptr_t) == 4 ? xrtTypeUInt32() : xrtTypeUInt64())";
    case XL6_IR_C_ABI_C_BOOL: return "xrtTypeBool()";
    case XL6_IR_C_ABI_C_FLOAT: return "xrtTypeFloat32()";
    case XL6_IR_C_ABI_C_DOUBLE: return "xrtTypeFloat64()";
    case XL6_IR_C_ABI_C_WCHAR:
        return "(sizeof(wchar_t) == 2 ? (WCHAR_MIN < 0 ? xrtTypeInt16() : xrtTypeUInt16()) : (WCHAR_MIN < 0 ? xrtTypeInt32() : xrtTypeUInt32()))";
    default:
        return NULL;
    }
}
static int xl6IrCgenAvlTypeAbi(
    const XL6_IrModule* module, XL6_IrTypeId typeId,
    const XL6_IrType** treeOut, const XL6_IrType** keyOut,
    const XL6_IrType** valueOut)
{
    const XL6_IrType* tree = module != NULL
        ? xl6IrTypeGet(&module->types, typeId) : NULL;
    const XL6_IrType* key = tree != NULL && tree->paramCount == 2 &&
            tree->params != NULL
        ? xl6IrTypeGet(&module->types, tree->params[0]) : NULL;
    const XL6_IrType* value = tree != NULL && tree->paramCount == 2 &&
            tree->params != NULL
        ? xl6IrTypeGet(&module->types, tree->params[1]) : NULL;
    if ( treeOut != NULL ) *treeOut = tree;
    if ( keyOut != NULL ) *keyOut = key;
    if ( valueOut != NULL ) *valueOut = value;
    return tree != NULL &&
           tree->kind == XL6_IR_TYPE_GENERIC_INSTANCE &&
           tree->runtimeResourceAbi == XL6_IR_RUNTIME_RESOURCE_AVL_TREE &&
           tree->cAbi == XL6_IR_C_ABI_RUNTIME_XVALUE_PTR &&
           tree->ownershipAbi == XL6_IR_OWNERSHIP_ABI_MANAGED &&
           tree->nullAbi == XL6_IR_NULL_ABI_XVALUE_SENTINEL &&
           tree->typeValueId != 0 && tree->paramCount == 2 &&
           tree->params != NULL && key != NULL && value != NULL &&
           xl6IrCgenAvlKeyOrderAbi(key) !=
               XL6_IR_RUNTIME_RESOURCE_KEY_ORDER_INVALID &&
           xl6IrCgenChannelPayloadAbi(key) !=
               XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID &&
           xl6IrCgenChannelPayloadAbi(value) !=
               XL6_IR_RUNTIME_RESOURCE_PAYLOAD_INVALID &&
           xl6IrCgenNativeValueTypeExpr(module, tree->params[0]) != NULL &&
           xl6IrCgenNativeValueTypeExpr(module, tree->params[1]) != NULL &&
           xl6IrCgenType(module, tree->params[0]) != NULL &&
           xl6IrCgenType(module, tree->params[1]) != NULL;
}

static int xl6IrCgenRuntimeChannelAbi(
    const XL6_IrModule* module, const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction* receiver = NULL;
    const XL6_IrInstruction* payloadValue = NULL;
    const XL6_IrInstruction* timeoutValue = NULL;
    const XL6_IrType* channel = NULL;
    const XL6_IrType* payload = NULL;
    const XL6_IrType* resultType;
    XL6_IrTypeId channelTypeId;
    int creates;
    int sends;
    int receives;
    int takesTimeout;
    int closes;
    if ( module == NULL || instruction == NULL ||
         instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
         !xl6IrCgenChannelRuntimeOpIs(instruction->runtimeOp) ||
         instruction->immediateText != NULL ||
         instruction->targetSymbol != 0 ||
         instruction->targetValue != XL6_IR_VALUE_INVALID ||
         instruction->calleeType != XL6_IR_TYPE_INVALID ||
         instruction->runtimeIdentityType != XL6_IR_TYPE_INVALID ||
         instruction->opcode != XL6_IR_OPCODE_INVALID ||
         instruction->firstOperand > module->operandCount ||
         instruction->operandCount >
             module->operandCount - instruction->firstOperand ) return 0;
    creates = instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_CREATE;
    sends = instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_SEND ||
            instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_TRY_SEND;
    receives = instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_RECV_INTO ||
               instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_TRY_RECV_INTO ||
               instruction->runtimeOp ==
                   XL6_IR_RUNTIME_CHANNEL_RECV_TIMEOUT_INTO;
    takesTimeout = instruction->runtimeOp ==
        XL6_IR_RUNTIME_CHANNEL_RECV_TIMEOUT_INTO;
    closes = instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_CLOSE;
    if ( !creates && instruction->operandCount != 0 )
        receiver = xl6IrCgenValueDefinition(
            module, module->operands[instruction->firstOperand], NULL);
    channelTypeId = creates ? instruction->type
                            : receiver != NULL ? receiver->type
                                               : XL6_IR_TYPE_INVALID;
    if ( !xl6IrCgenChannelTypeAbi(
             module, channelTypeId, &channel, &payload) ||
         instruction->payloadType != channel->params[0] ||
         instruction->runtimeResourcePayloadAbi !=
             xl6IrCgenChannelPayloadAbi(payload) ) return 0;
    resultType = xl6IrTypeGet(&module->types, instruction->type);
    if ( creates ) {
        const XL6_IrInstruction* capacity = instruction->operandCount == 1
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand], NULL)
            : NULL;
        const XL6_IrType* capacityType = capacity != NULL
            ? xl6IrTypeGet(&module->types, capacity->type) : NULL;
        return instruction->result != XL6_IR_VALUE_INVALID &&
               instruction->operandCount <= 1 &&
               (instruction->operandCount == 0 ||
                (capacityType != NULL &&
                 capacityType->cAbi == XL6_IR_C_ABI_I64 &&
                 capacityType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
                 capacityType->integerTarget ==
                     XL6_IR_INTEGER_TARGET_SIGNED_64));
    }
    if ( receiver == NULL || receiver->type != channelTypeId ) return 0;
    if ( closes )
        return instruction->operandCount == 1 &&
               instruction->result == XL6_IR_VALUE_INVALID &&
               instruction->type == XL6_IR_TYPE_INVALID;
    if ( instruction->result == XL6_IR_VALUE_INVALID || resultType == NULL )
        return 0;
    if ( sends ) {
        payloadValue = instruction->operandCount == 2
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand + 1], NULL)
            : NULL;
        return instruction->operandCount == 2 &&
               payloadValue != NULL &&
               payloadValue->type == instruction->payloadType &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    }
    if ( receives ) {
        const XL6_IrType* referenceType;
        payloadValue = instruction->operandCount >= 2
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand + 1], NULL)
            : NULL;
        timeoutValue = instruction->operandCount == 3
            ? xl6IrCgenValueDefinition(
                module, module->operands[instruction->firstOperand + 2], NULL)
            : NULL;
        referenceType = payloadValue != NULL
            ? xl6IrTypeGet(&module->types, payloadValue->type) : NULL;
        return instruction->operandCount == (takesTimeout ? 3u : 2u) &&
               payloadValue != NULL &&
               payloadValue->kind == XL6_IR_INSTR_STORAGE_ADDRESS &&
               referenceType != NULL &&
               referenceType->kind == XL6_IR_TYPE_STORAGE_REF &&
               referenceType->cAbi == XL6_IR_C_ABI_STORAGE_REF_PTR &&
               referenceType->paramCount == 1 &&
               referenceType->params != NULL &&
               referenceType->params[0] == instruction->payloadType &&
               (!takesTimeout ||
                (timeoutValue != NULL &&
                 xl6IrTypeGet(&module->types, timeoutValue->type) != NULL &&
                 xl6IrTypeGet(&module->types, timeoutValue->type)->cAbi ==
                     XL6_IR_C_ABI_I64 &&
                 xl6IrTypeGet(&module->types, timeoutValue->type)->ownershipAbi ==
                     XL6_IR_OWNERSHIP_ABI_TRIVIAL)) &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    }
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_IS_CLOSED )
        return instruction->operandCount == 1 &&
               resultType->cAbi == XL6_IR_C_ABI_BOOL &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL;
    if ( instruction->runtimeOp == XL6_IR_RUNTIME_CHANNEL_COUNT )
        return instruction->operandCount == 1 &&
               resultType->cAbi == XL6_IR_C_ABI_I64 &&
               resultType->ownershipAbi == XL6_IR_OWNERSHIP_ABI_TRIVIAL &&
               resultType->integerTarget == XL6_IR_INTEGER_TARGET_SIGNED_64;
    return 0;
}

static const char* xl6IrCgenChannelPayloadAbiToken(
    XL6_IrRuntimeResourcePayloadAbi abi)
{
    switch ( abi ) {
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY:
        return "XLIR_CHANNEL_PAYLOAD_TRIVIAL_COPY";
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY:
        return "XLIR_CHANNEL_PAYLOAD_STRING_COPY";
    case XL6_IR_RUNTIME_RESOURCE_PAYLOAD_XVALUE_RETAIN:
        return "XLIR_CHANNEL_PAYLOAD_XVALUE_RETAIN";
    default:
        return NULL;
    }
}
