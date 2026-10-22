/* Included by xl6_ir.c. C owners never enter xvalue or native-value profiles. */
int xl6IrTypeIsCOwned(const XL6_IrType* type)
{
    return type && type->kind==XL6_IR_TYPE_BUILTIN && !type->paramCount &&
        type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_MANAGED && type->nullAbi==XL6_IR_NULL_ABI_NONE &&
        ((type->cAbi==XL6_IR_C_ABI_C_STRING && type->abiId==xl6TypeAbiIdText("xlang.cstring.v1")) ||
         (type->cAbi==XL6_IR_C_ABI_C_BYTES && type->abiId==xl6TypeAbiIdText("xlang.cbytes.v1")));
}
int xl6IrCOwnedRuntimeOpIs(XL6_IrRuntimeOp op)
{
    return op>=XL6_IR_RUNTIME_CSTRING_TAKE && op<=XL6_IR_RUNTIME_CBYTES_SIZE_TAKE;
}
XL6_IrCallErrorPhysicalAbi xl6IrCOwnedErrorPhysicalAbi(XL6_IrRuntimeOp op)
{
    if(op==XL6_IR_RUNTIME_CBYTES_SIZE_TAKE) return XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT;
    if(op==XL6_IR_RUNTIME_CSTRING_BOX || op==XL6_IR_RUNTIME_CBYTES_COPY)
        return XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT;
    return XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
}
static int xl6IrCOwnedReleaser(const XL6_IrModule* module,XL6_SymbolId symbol)
{
    const XL6_IrCallTarget* target=xl6IrCallTargetGet(module,symbol);
    const XL6_IrType* param=target && target->parameterCount==1 ?
        xl6IrTypeGet(&module->types,target->parameterTypes[0]) : NULL;
    return target && target->kind==XL6_IR_CALL_TARGET_EXTERN_C && target->cName && *target->cName &&
        target->errorAbi==XL6_IR_ERROR_ABI_NO_RAISE && target->returnType==XL6_IR_TYPE_INVALID &&
        target->parameterCount==1 && target->parameterAbis &&
        target->parameterAbis[0]==XL6_IR_PARAMETER_ABI_VALUE && param &&
        param->cAbi==XL6_IR_C_ABI_RAW_POINTER && !param->paramCount;
}
int xl6IrCOwnedTakeNodeCompatible(const XL6_IrModule* module,const XL6_IrNode* node)
{
    const XL6_IrType* result=node ? xl6IrTypeGet(&module->types,node->storageType) : NULL;
    const XL6_IrNode* args=node && node->childCount==2 ?
        xl6IrNodeGet(module,module->children[node->firstChild+1]) : NULL;
    if(!node || node->callKind!=XL6_SEM_CALL_C_OWNED_TAKE || !xl6IrTypeIsCOwned(result) ||
        !args || args->childCount!=(result->cAbi==XL6_IR_C_ABI_C_STRING ? 2u : 3u) ||
        !xl6IrCOwnedReleaser(module,node->targetSymbol)) return 0;
    return 1;
}
int xl6IrCOwnedInstructionCompatible(const XL6_IrModule* module,const XL6_IrInstruction* ins)
{
    if(!module || !ins || ins->kind!=XL6_IR_INSTR_RUNTIME_CALL || !xl6IrCOwnedRuntimeOpIs(ins->runtimeOp) ||
        !ins->result || ins->firstOperand>module->operandCount ||
        ins->operandCount>module->operandCount-ins->firstOperand || ins->immediateText ||
        ins->immediateData || ins->immediateSize || ins->targetValue || ins->calleeType ||
        ins->runtimeIdentityType || ins->payloadType || ins->opcode || ins->integerTarget ||
        ins->integerSourceAbi || ins->castAbi) return 0;
    const XL6_IrInstruction* input=ins->operandCount ?
        xl6IrValueDefinition(module,module->operands[ins->firstOperand],NULL) : NULL;
    const XL6_IrType* source=input ? xl6IrTypeGet(&module->types,input->type) : NULL;
    const XL6_IrType* result=xl6IrTypeGet(&module->types,ins->type);
    if(!source || !result) return 0;
    if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SIZE_TAKE) {
        const XL6_IrInstruction* length=ins->operandCount==2 ?
            xl6IrValueDefinition(module,module->operands[ins->firstOperand+1],NULL) : NULL;
        return !ins->targetSymbol && xl6IrTypeIsCOwned(source) &&
            source->cAbi==XL6_IR_C_ABI_C_BYTES && ins->type==input->type &&
            length && xl6IrTypeGet(&module->types,length->type)->cAbi==XL6_IR_C_ABI_I64;
    }
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_TAKE || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_TAKE) {
        int binary=ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_TAKE;
        const XL6_IrNode* origin=xl6IrNodeGet(module,ins->origin);
        if(!xl6IrCOwnedTakeNodeCompatible(module,origin) || ins->targetSymbol!=origin->targetSymbol ||
            ins->type!=origin->storageType || !xl6IrTypeIsCOwned(result) ||
            result->cAbi!=(binary ? XL6_IR_C_ABI_C_BYTES : XL6_IR_C_ABI_C_STRING) ||
            ins->operandCount!=1 || source->cAbi!=XL6_IR_C_ABI_RAW_POINTER) return 0;
        return 1;
    }
    if(ins->targetSymbol || ins->operandCount!=1 || !xl6IrTypeIsCOwned(source))return 0;
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY)
        return source->cAbi==XL6_IR_C_ABI_C_STRING && result->cAbi==XL6_IR_C_ABI_MANAGED_STRING;
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_BOX)
        return source->cAbi==XL6_IR_C_ABI_C_STRING && xl6IrTypeIsDynamicXValueBuiltin(module,ins->type);
    return source->cAbi==XL6_IR_C_ABI_C_BYTES &&
        (result->cAbi==XL6_IR_C_ABI_MANAGED_BYTES || xl6IrTypeIsDynamicXValueBuiltin(module,ins->type));
}
