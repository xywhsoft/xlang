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

static const char* xl6IrCgenThreadHelper(XL6_IrRuntimeOp operation)
{
    switch (operation) {
    case XL6_IR_RUNTIME_THREAD_CREATE: return "xl6ThreadCreateV1";
    case XL6_IR_RUNTIME_THREAD_WAIT: return "xl6ThreadWaitV1";
    case XL6_IR_RUNTIME_THREAD_WAIT_TIMEOUT: return "xl6ThreadWaitTimeoutV1";
    case XL6_IR_RUNTIME_THREAD_EXIT_CODE: return "xl6ThreadExitCodeV1";
    case XL6_IR_RUNTIME_THREAD_STATE: return "xl6ThreadStateV1";
    case XL6_IR_RUNTIME_THREAD_STOP: return "xl6ThreadStopV1";
    default: return NULL;
    }
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
            "    v%u = xl6ThreadCreateV1(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_WAIT:
        xl6OutputPrintf(out,
            "    v%u = xl6ThreadWaitV1(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_WAIT_TIMEOUT:
        xl6OutputPrintf(out,
            "    v%u = xl6ThreadWaitTimeoutV1(v%u, 0x%016llXULL, v%u);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId,
            (unsigned int)module->operands[instruction->firstOperand + 1]);
        break;
    case XL6_IR_RUNTIME_THREAD_EXIT_CODE:
        xl6OutputPrintf(out,
            "    v%u = xl6ThreadExitCodeV1(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_STATE:
        xl6OutputPrintf(out,
            "    v%u = xl6ThreadStateV1(v%u, 0x%016llXULL);\n",
            (unsigned int)instruction->result, (unsigned int)first,
            (unsigned long long)threadType->typeValueId);
        break;
    case XL6_IR_RUNTIME_THREAD_STOP:
        xl6OutputPrintf(out,
            "    v%u = xl6ThreadStopV1(v%u, 0x%016llXULL);\n",
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
    return xl6IrChannelRuntimeOpIs(operation);
}

static int xl6IrCgenSpecialQueueRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return (operation >= XL6_IR_RUNTIME_SPECIAL_QUEUE_CREATE &&
            operation <= XL6_IR_RUNTIME_SPECIAL_QUEUE_IS_DRAINED) ||
           operation == XL6_IR_RUNTIME_SPECIAL_QUEUE_PUSH_BATCH ||
           operation == XL6_IR_RUNTIME_SPECIAL_QUEUE_POP_BATCH_INTO ||
           operation == XL6_IR_RUNTIME_CONCURRENT_QUEUE_TRY_POP ||
           operation == XL6_IR_RUNTIME_CONCURRENT_QUEUE_POP_BATCH ||
           operation == XL6_IR_RUNTIME_WAIT_QUEUE_POP ||
           operation == XL6_IR_RUNTIME_WAIT_QUEUE_POP_TIMEOUT;
}

static int xl6IrCgenStackRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return operation >= XL6_IR_RUNTIME_STACK_CREATE &&
           operation <= XL6_IR_RUNTIME_STACK_CAPACITY;
}

static int xl6IrCgenAvlRuntimeOpIs(XL6_IrRuntimeOp operation)
{
    return xl6IrTreeQuerySpecRuntime(operation) ||
           (operation >= XL6_IR_RUNTIME_AVL_TREE_CREATE &&
            operation <= XL6_IR_RUNTIME_AVL_TREE_CLONE);
}

static int xl6IrCgenAvlRuntimeOpMayRaise(XL6_IrRuntimeOp operation)
{
    return xl6IrTreeQuerySpecRuntime(operation) ||
        (operation>=XL6_IR_RUNTIME_AVL_TREE_CREATE && operation<=XL6_IR_RUNTIME_AVL_TREE_CLONE);
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
           xl6IrStackConstructionContract(module,typeId,NULL) &&
           xl6IrCgenType(module, stack->params[0]) != NULL;
}

/* xrtTypedTree owns values through immutable xrttype descriptors.  This map
 * is physical ABI only; no source spelling or AST method name reaches Cgen. */
static const char* xl6IrCgenNativeValueConverterExpr(const XL6_IrModule* module, XL6_IrTypeId typeId)
{
    const XL6_IrType* type = module != NULL ? xl6IrTypeGet(&module->types,typeId) : NULL;
    if (type == NULL) return "NULL";
    switch (type->nativeSlotAbi) {
#define XL6_NATIVE_VALUE_PROFILE(Tag,Name,Abi,CName,Header,Result,Copy,Drop,Eq,Box,Project,Release,Type,Converter,Sequence,Profile,ValueId,...) \
    case XL6_IR_NATIVE_SLOT_##Tag: return #Converter "()";
#include "src/type/xl6_native_value_profiles.inc"
#undef XL6_NATIVE_VALUE_PROFILE
#define XL6_IR_CGEN_SLOT_PROVIDER(Slot,Type,Converter) case XL6_IR_NATIVE_SLOT_##Slot: return #Converter "()";
#include "src/cgen/xl6_ir_cgen_slot_providers.inc"
#undef XL6_IR_CGEN_SLOT_PROVIDER
    default: return "NULL";
    }
}
static const char* xl6IrCgenNativeSlotTypeExpr(XL6_IrNativeSlotAbi slot)
{
    switch (slot) {
#define XL6_NATIVE_VALUE_PROFILE(Tag,Name,Abi,CName,Header,Result,Copy,Drop,Eq,Box,Project,Release,Type,Converter,Sequence,Profile,ValueId,...) \
    case XL6_IR_NATIVE_SLOT_##Tag: return #Type "()";
#include "src/type/xl6_native_value_profiles.inc"
#undef XL6_NATIVE_VALUE_PROFILE
#define XL6_IR_CGEN_SLOT_PROVIDER(Slot,Type,Converter) case XL6_IR_NATIVE_SLOT_##Slot: return #Type "()";
#include "src/cgen/xl6_ir_cgen_slot_providers.inc"
#undef XL6_IR_CGEN_SLOT_PROVIDER
    case XL6_IR_NATIVE_SLOT_BOOL: return "xrtTypeBool()";
    case XL6_IR_NATIVE_SLOT_I64: return "xrtTypeInt64()";
    case XL6_IR_NATIVE_SLOT_U64: return "xrtTypeUInt64()";
    case XL6_IR_NATIVE_SLOT_F64: return "xrtTypeFloat64()";
    case XL6_IR_NATIVE_SLOT_TIME: return "xrtTypeTime()";
    case XL6_IR_NATIVE_SLOT_TYPE_ID: return "xrtTypeType()";
    case XL6_IR_NATIVE_SLOT_XVALUE_CARRIER: return "xrtTypeValue()";
    case XL6_IR_NATIVE_SLOT_FUTURE_PTR: return "xrtTypeFuture()";
    case XL6_IR_NATIVE_SLOT_F32: return "xrtTypeFloat32()";
    case XL6_IR_NATIVE_SLOT_C_CHAR:
        return "(CHAR_MIN < 0 ? xrtTypeInt8() : xrtTypeUInt8())";
    case XL6_IR_NATIVE_SLOT_C_SCHAR: return "xrtTypeInt8()";
    case XL6_IR_NATIVE_SLOT_C_UCHAR: return "xrtTypeUInt8()";
    case XL6_IR_NATIVE_SLOT_C_SHORT: return "xrtTypeInt16()";
    case XL6_IR_NATIVE_SLOT_C_USHORT: return "xrtTypeUInt16()";
    case XL6_IR_NATIVE_SLOT_C_INT: return "xrtTypeInt32()";
    case XL6_IR_NATIVE_SLOT_C_UINT: return "xrtTypeUInt32()";
    case XL6_IR_NATIVE_SLOT_C_LONG:
        return "(sizeof(long) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_NATIVE_SLOT_C_ULONG:
        return "(sizeof(unsigned long) == 4 ? xrtTypeUInt32() : xrtTypeUInt64())";
    case XL6_IR_NATIVE_SLOT_C_LONGLONG: return "xrtTypeInt64()";
    case XL6_IR_NATIVE_SLOT_C_ULONGLONG: return "xrtTypeUInt64()";
    case XL6_IR_NATIVE_SLOT_C_SIZE:
        return "(sizeof(size_t) == 4 ? xrtTypeUInt32() : xrtTypeUInt64())";
    case XL6_IR_NATIVE_SLOT_C_SSIZE:
        return "(sizeof(intptr_t) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_NATIVE_SLOT_C_PTRDIFF:
        return "(sizeof(ptrdiff_t) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_NATIVE_SLOT_C_INTPTR:
        return "(sizeof(intptr_t) == 4 ? xrtTypeInt32() : xrtTypeInt64())";
    case XL6_IR_NATIVE_SLOT_C_UINTPTR:
        return "(sizeof(uintptr_t) == 4 ? xrtTypeUInt32() : xrtTypeUInt64())";
    case XL6_IR_NATIVE_SLOT_C_BOOL: return "xrtTypeBool()";
    case XL6_IR_NATIVE_SLOT_C_FLOAT: return "xrtTypeFloat32()";
    case XL6_IR_NATIVE_SLOT_C_DOUBLE: return "xrtTypeFloat64()";
    case XL6_IR_NATIVE_SLOT_C_WCHAR:
        return "(sizeof(wchar_t) == 2 ? (WCHAR_MIN < 0 ? xrtTypeInt16() : xrtTypeUInt16()) : (WCHAR_MIN < 0 ? xrtTypeInt32() : xrtTypeUInt32()))";
    default:
        return NULL;
    }
}
static const char* xl6IrCgenNativeValueTypeExpr(const XL6_IrModule* module, XL6_IrTypeId typeId)
{
    const XL6_IrType* type=module ? xl6IrTypeGet(&module->types,typeId) : NULL;
    return type && type->typeValueId ? xl6IrCgenNativeSlotTypeExpr(type->nativeSlotAbi) : NULL;
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
           xl6IrAvlConstructionRecipe(module,typeId,NULL,NULL,NULL,NULL) &&
           xl6IrAvlKeyOrderAbi(module,typeId) !=
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

static int xl6IrCgenRuntimeChannelAbi(const XL6_IrModule* module,const XL6_IrInstruction* instruction)
{ return xl6IrChannelInstructionCompatible(module,instruction); }

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
