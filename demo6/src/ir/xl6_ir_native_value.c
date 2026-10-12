/* Included by xl6_ir.c. Native owning records have one closed representation
 * contract. Source spelling, foreign structs and a matching sizeof cannot
 * confer this capability. Capture is independently published as an immutable
 * native leaf capability. Nullable/cells need separate owning plans. */
static const XL6_IrNativeValuePlan g_xl6IrNativeValues[] = {
#define XL6_NATIVE_VALUE_PROFILE(Tag,Name,Abi,CName,Header,Result,Copy,Drop,Eq,Box,Project,Release,Type,Converter,Sequence,Profile,ValueId,CaptureHeader,CaptureProfile) \
    {XL6_IR_NATIVE_VALUE_##Tag,Abi,CName,Header,Result,Copy,Drop,Eq,Box,Project,XL6_IR_RELEASE_EFFECT_##Release, \
        XL6_IR_NATIVE_SLOT_##Tag,#Type "()",#Converter "()",Sequence,Profile,ValueId,CaptureHeader,CaptureProfile},
#include "src/type/xl6_native_value_profiles.inc"
#undef XL6_NATIVE_VALUE_PROFILE
};
static XL6_IrNativeValueAbi xl6IrNativeValueAbiFromFrontend(XL6_TypeAbiId abi)
{
    size_t i;
    for (i=0;i<XL6_ARRAY_COUNT(g_xl6IrNativeValues);++i)
        if (abi == xl6TypeAbiIdText(g_xl6IrNativeValues[i].abiName)) return g_xl6IrNativeValues[i].abi;
    return XL6_IR_NATIVE_VALUE_NONE;
}
static XL6_IrNativeValueAbi xl6IrNativeValueAbiFromType(const XL6_IrType* type)
{
    if (!type || type->kind != XL6_IR_TYPE_BUILTIN || type->paramCount || type->returnCount)
        return XL6_IR_NATIVE_VALUE_NONE;
    return xl6IrNativeValueAbiFromFrontend(type->abiId);
}
const XL6_IrNativeValuePlan* xl6IrNativeValuePlanForType(const XL6_IrType* type)
{
    size_t i;
    if (!type || type->cAbi != XL6_IR_C_ABI_NATIVE_VALUE ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED || type->nullAbi != XL6_IR_NULL_ABI_NONE ||
        type->nativeValueAbi == XL6_IR_NATIVE_VALUE_NONE ||
        type->nativeValueAbi != xl6IrNativeValueAbiFromType(type)) return NULL;
    for (i=0;i<XL6_ARRAY_COUNT(g_xl6IrNativeValues);++i)
        if (g_xl6IrNativeValues[i].abi == type->nativeValueAbi) return &g_xl6IrNativeValues[i];
    return NULL;
}
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeNativeValueErrorPhysicalAbi(XL6_IrRuntimeOp operation)
{
    if (operation == XL6_IR_RUNTIME_NATIVE_VALUE_COPY ||
        operation == XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED)
        return XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT;
    return operation == XL6_IR_RUNTIME_NATIVE_VALUE_BOX
        ? XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
}
const XL6_IrNativeValuePlan* xl6IrNativeValueInstructionPlan(
    const XL6_IrModule* module,const XL6_IrInstruction* instruction)
{
    const XL6_IrInstruction *left,*right;
    const XL6_IrType *type,*result;
    if (!module || !instruction || instruction->kind != XL6_IR_INSTR_RUNTIME_CALL ||
        instruction->result == XL6_IR_VALUE_INVALID || instruction->immediateText ||
        instruction->immediateData || instruction->immediateSize || instruction->targetSymbol ||
        instruction->targetValue || instruction->calleeType || instruction->runtimeIdentityType ||
        instruction->opcode != XL6_IR_OPCODE_INVALID ||
        instruction->integerTarget != XL6_IR_INTEGER_TARGET_INVALID ||
        instruction->integerSourceAbi != XL6_IR_INTEGER_SOURCE_INVALID ||
        instruction->firstOperand > module->operandCount ||
        instruction->operandCount > module->operandCount-instruction->firstOperand) return NULL;
    if (instruction->runtimeOp != XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED &&
        instruction->castAbi != XL6_IR_CAST_INVALID) return NULL;
    if (instruction->runtimeOp == XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED) {
        if (instruction->operandCount != 1 || instruction->payloadType != instruction->type ||
            instruction->castAbi != XL6_IR_CAST_XVALUE_TO_NATIVE_VALUE_CHECKED) return NULL;
        left=xl6IrValueDefinition(module,module->operands[instruction->firstOperand],NULL);
        /* Physical checked projection is also used by exact typed collection
         * reads. Source `as` authorization lives in Typed IR; the lowered
         * operation's type, cast family and checked outcome are its contract,
         * not the spelling/kind of its diagnostic origin. */
        if (!left || !xl6IrTypeIsDynamicXValueBuiltin(module,left->type)) return NULL;
        return xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,instruction->type));
    }
    if (instruction->runtimeOp == XL6_IR_RUNTIME_NATIVE_VALUE_BOX) {
        if (instruction->operandCount != 1 || !xl6IrTypeIsDynamicXValueBuiltin(module,instruction->type)) return NULL;
        left=xl6IrValueDefinition(module,module->operands[instruction->firstOperand],NULL);
        if (!left || left->type != instruction->payloadType) return NULL;
        return xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,instruction->payloadType));
    }
    if (instruction->runtimeOp == XL6_IR_RUNTIME_NATIVE_VALUE_COPY) {
        if (instruction->operandCount != 1 || instruction->payloadType != XL6_IR_TYPE_INVALID) return NULL;
        left=xl6IrValueDefinition(module,module->operands[instruction->firstOperand],NULL);
        if (!left || left->type != instruction->type) return NULL;
        return xl6IrNativeValuePlanForType(xl6IrTypeGet(&module->types,instruction->type));
    }
    if (instruction->runtimeOp != XL6_IR_RUNTIME_NATIVE_VALUE_EQUAL || instruction->operandCount != 2)
        return NULL;
    left=xl6IrValueDefinition(module,module->operands[instruction->firstOperand],NULL);
    right=xl6IrValueDefinition(module,module->operands[instruction->firstOperand+1],NULL);
    type=xl6IrTypeGet(&module->types,instruction->payloadType);
    result=xl6IrTypeGet(&module->types,instruction->type);
    if (!left || !right || left->type != instruction->payloadType || right->type != left->type ||
        !result || result->cAbi != XL6_IR_C_ABI_BOOL ||
        result->ownershipAbi != XL6_IR_OWNERSHIP_ABI_TRIVIAL || result->kind != XL6_IR_TYPE_BUILTIN)
        return NULL;
    return xl6IrNativeValuePlanForType(type);
}
