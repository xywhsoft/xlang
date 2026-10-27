/* Included by xl6_ir.c. C owners never enter xvalue or native-value profiles. */
static XL6_IrRuntimeOp xl6IrCOwnedCompareOp(const char* text)
{
    if(!text)return XL6_IR_RUNTIME_NONE;
    if(!strcmp(text,"=="))return XL6_IR_RUNTIME_C_COMPARE_EQ;
    if(!strcmp(text,"!="))return XL6_IR_RUNTIME_C_COMPARE_NE;
    if(!strcmp(text,"<"))return XL6_IR_RUNTIME_C_COMPARE_LT;
    if(!strcmp(text,"<="))return XL6_IR_RUNTIME_C_COMPARE_LE;
    if(!strcmp(text,">"))return XL6_IR_RUNTIME_C_COMPARE_GT;
    if(!strcmp(text,">="))return XL6_IR_RUNTIME_C_COMPARE_GE;
    return XL6_IR_RUNTIME_NONE;
}
int xl6IrTypeIsCOwned(const XL6_IrType* type)
{
    if(!type || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED)return 0;
    int optional=type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params &&
        type->nullAbi==XL6_IR_NULL_ABI_TAGGED_VALUE;
    int plain=type->kind==XL6_IR_TYPE_BUILTIN && !type->paramCount && type->nullAbi==XL6_IR_NULL_ABI_NONE;
    return (plain || optional) &&
        ((type->cAbi==XL6_IR_C_ABI_C_STRING && type->abiId==xl6TypeAbiIdText(optional ? "xlang.cstring.optional.v1" : "xlang.cstring.v1")) ||
         (type->cAbi==XL6_IR_C_ABI_C_BYTES && type->abiId==xl6TypeAbiIdText(optional ? "xlang.cbytes.optional.v1" : "xlang.cbytes.v1")));
}
/* Reuse the existing native-leaf capture machinery without admitting C
 * carriers to native boxing, containers or the XRT language type registry. */
const XL6_IrNativeValuePlan* xl6IrOwnedTransferPlanForType(const XL6_IrType* type)
{
    static const XL6_IrNativeValuePlan plan={
        .abiName="xlang.compiler.c-owner.v1",.cName="XL6_COwnedV1",
        .header="__xl_runtime/xl6_c_owned.h",.copySymbol="xl6COwnedCopyCapturedV1",
        .dropSymbol="xl6COwnedDropCapturedV1",.releaseEffect=XL6_IR_RELEASE_EFFECT_NO_RAISE,
        .captureHeader="__xl_runtime/xl6_c_owned_capture.h",.captureProfileSymbol="xl6COwnedCaptureProfileV1Get"};
    return xl6IrTypeIsCOwned(type) ? &plan : xl6IrNativeValuePlanForType(type);
}
int xl6IrCOwnedRuntimeOpIs(XL6_IrRuntimeOp op)
{
    return op>=XL6_IR_RUNTIME_CSTRING_TAKE && op<=XL6_IR_RUNTIME_C_BORROW_VIEW;
}
XL6_IrCallErrorPhysicalAbi xl6IrCOwnedErrorPhysicalAbi(XL6_IrRuntimeOp op)
{
    if(op==XL6_IR_RUNTIME_C_PRINT || op==XL6_IR_RUNTIME_C_PRINTLN)return XL6_IR_CALL_ERROR_PHYSICAL_FALSE_RESULT;
    if(op==XL6_IR_RUNTIME_CBYTES_CONCAT)return XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT;
    if(op==XL6_IR_RUNTIME_CSTRING_COPY || op==XL6_IR_RUNTIME_CSTRING_SET_COPY || op==XL6_IR_RUNTIME_CBYTES_SET_COPY ||
        op==XL6_IR_RUNTIME_C_OWNED_COPY || op==XL6_IR_RUNTIME_CSTRING_TAKE ||
        op==XL6_IR_RUNTIME_CBYTES_TAKE || op==XL6_IR_RUNTIME_CBYTES_FROM_BYTES ||
        op==XL6_IR_RUNTIME_CBYTES_SIZE_TAKE || op==XL6_IR_RUNTIME_CSTRING_SIZE_TAKE ||
        op==XL6_IR_RUNTIME_CSTRING_AT || op==XL6_IR_RUNTIME_CBYTES_AT)
        return XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT;
    if(op==XL6_IR_RUNTIME_CBYTES_FORMAT || op==XL6_IR_RUNTIME_CSTRING_BOX || op==XL6_IR_RUNTIME_CBYTES_COPY)
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
    if(!node || node->callKind!=XL6_SEM_CALL_C_OWNED_TAKE || (!xl6IrTypeIsCOwned(result) || result->kind!=XL6_IR_TYPE_BUILTIN) ||
        !args || (args->childCount!=3 && !(result->cAbi==XL6_IR_C_ABI_C_STRING && args->childCount==2)) ||
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
    if(!result) return 0;
    if(ins->runtimeOp==XL6_IR_RUNTIME_C_PRINT || ins->runtimeOp==XL6_IR_RUNTIME_C_PRINTLN) {
        const XL6_IrNode* origin=xl6IrNodeGet(module,ins->origin);
        if(ins->targetSymbol || result->cAbi!=XL6_IR_C_ABI_BOOL || !origin || origin->callKind!=XL6_SEM_CALL_C_OWNED_PRINT)return 0;
        for(size_t i=0;i<ins->operandCount;++i) {
            const XL6_IrInstruction* def=xl6IrValueDefinition(module,module->operands[ins->firstOperand+i],NULL);
            const XL6_IrType* type=def ? xl6IrTypeGet(&module->types,def->type) : NULL;
            if(!type || (!xl6IrTypeIsCOwned(type) && type->cAbi!=XL6_IR_C_ABI_MANAGED_STRING &&
                type->cAbi!=XL6_IR_C_ABI_MANAGED_BYTES && !xl6IrTypeIsDynamicXValueBuiltin(module,def->type)))return 0;
        }
        return 1;
    }
    if(!source) return 0;
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_SET_COPY || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SET_COPY) {
        const XL6_IrInstruction* key=ins->operandCount==3 ? xl6IrValueDefinition(module,module->operands[ins->firstOperand+1],NULL) : NULL;
        const XL6_IrInstruction* item=ins->operandCount==3 ? xl6IrValueDefinition(module,module->operands[ins->firstOperand+2],NULL) : NULL;
        const XL6_IrType* keyType=key ? xl6IrTypeGet(&module->types,key->type) : NULL;
        const XL6_IrType* itemType=item ? xl6IrTypeGet(&module->types,item->type) : NULL;
        int text=ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_SET_COPY;
        return !ins->targetSymbol && keyType && itemType && ins->type==input->type && xl6IrTypeIsCOwned(source) &&
            source->cAbi==(text ? XL6_IR_C_ABI_C_STRING : XL6_IR_C_ABI_C_BYTES) &&
            keyType->integerTarget==XL6_IR_INTEGER_TARGET_SIGNED_64 &&
            (text ? itemType->cAbi==XL6_IR_C_ABI_CHAR_U32 : itemType->integerTarget==XL6_IR_INTEGER_TARGET_UNSIGNED_8);
    }
    if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_FORMAT)
        return ins->operandCount==1 && !ins->targetSymbol && source->kind==XL6_IR_TYPE_BUILTIN && source->cAbi==XL6_IR_C_ABI_C_BYTES && result->cAbi==XL6_IR_C_ABI_MANAGED_STRING;
    if(ins->runtimeOp==XL6_IR_RUNTIME_C_OWNED_COPY)
        return ins->operandCount==1 && !ins->targetSymbol && xl6IrTypeIsCOwned(source) && ins->type==input->type;
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_FROM_STRING || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_FROM_BYTES)
        return ins->operandCount==1 && !ins->targetSymbol && xl6IrTypeIsCOwned(result) &&
            ((source->kind==XL6_IR_TYPE_NULLABLE)==(result->kind==XL6_IR_TYPE_NULLABLE)) &&
            ((source->cAbi==XL6_IR_C_ABI_MANAGED_STRING && result->cAbi==XL6_IR_C_ABI_C_STRING && ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_FROM_STRING) ||
             (source->cAbi==XL6_IR_C_ABI_MANAGED_BYTES && result->cAbi==XL6_IR_C_ABI_C_BYTES && ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_FROM_BYTES));
    if(ins->runtimeOp==XL6_IR_RUNTIME_C_BORROW_VIEW)
        return ins->operandCount==1 && !ins->targetSymbol && xl6IrTypeIsCOwned(result) && source->kind==XL6_IR_TYPE_BUILTIN && result->kind==XL6_IR_TYPE_BUILTIN &&
            ((source->cAbi==XL6_IR_C_ABI_MANAGED_STRING && result->cAbi==XL6_IR_C_ABI_C_STRING) ||
             (source->cAbi==XL6_IR_C_ABI_MANAGED_BYTES && result->cAbi==XL6_IR_C_ABI_C_BYTES));
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_VIEW)
        return ins->operandCount==1 && !ins->targetSymbol && xl6IrTypeIsCOwned(source) &&
            source->cAbi==XL6_IR_C_ABI_C_STRING && result->cAbi==XL6_IR_C_ABI_MANAGED_STRING &&
            (source->kind!=XL6_IR_TYPE_NULLABLE || result->kind==XL6_IR_TYPE_NULLABLE);
    if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_CONCAT) {
        const XL6_IrInstruction* right=ins->operandCount==2 ? xl6IrValueDefinition(module,module->operands[ins->firstOperand+1],NULL) : NULL;
        const XL6_IrType* other=right ? xl6IrTypeGet(&module->types,right->type) : NULL;
        return !ins->targetSymbol && other && source->kind==XL6_IR_TYPE_BUILTIN && other->kind==XL6_IR_TYPE_BUILTIN &&
            result->kind==XL6_IR_TYPE_BUILTIN && result->cAbi==XL6_IR_C_ABI_C_BYTES &&
            (source->cAbi==XL6_IR_C_ABI_C_BYTES || source->cAbi==XL6_IR_C_ABI_MANAGED_BYTES) &&
            (other->cAbi==XL6_IR_C_ABI_C_BYTES || other->cAbi==XL6_IR_C_ABI_MANAGED_BYTES);
    }
    if(ins->runtimeOp>=XL6_IR_RUNTIME_C_COMPARE_EQ) {
        const XL6_IrInstruction* right=ins->operandCount==2 ?
            xl6IrValueDefinition(module,module->operands[ins->firstOperand+1],NULL) : NULL;
        const XL6_IrType* other=right ? xl6IrTypeGet(&module->types,right->type) : NULL;
        int text=source->cAbi==XL6_IR_C_ABI_C_STRING || source->cAbi==XL6_IR_C_ABI_MANAGED_STRING;
        int binary=source->cAbi==XL6_IR_C_ABI_C_BYTES || source->cAbi==XL6_IR_C_ABI_MANAGED_BYTES;
        return !ins->targetSymbol && other && result->cAbi==XL6_IR_C_ABI_BOOL &&
            ((source->kind==XL6_IR_TYPE_NULLABLE)==(other->kind==XL6_IR_TYPE_NULLABLE)) &&
            (source->kind!=XL6_IR_TYPE_NULLABLE || ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_EQ || ins->runtimeOp==XL6_IR_RUNTIME_C_COMPARE_NE) &&
            (xl6IrTypeIsCOwned(source) || xl6IrTypeIsCOwned(other)) &&
            ((text && (other->cAbi==XL6_IR_C_ABI_C_STRING || other->cAbi==XL6_IR_C_ABI_MANAGED_STRING)) ||
             (binary && (other->cAbi==XL6_IR_C_ABI_C_BYTES || other->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)));
    }
    if(ins->runtimeOp>=XL6_IR_RUNTIME_CSTRING_LENGTH) {
        int binary=ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_LENGTH || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_AT;
        int index=ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_AT || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_AT;
        const XL6_IrInstruction* key=index && ins->operandCount==2 ?
            xl6IrValueDefinition(module,module->operands[ins->firstOperand+1],NULL) : NULL;
        return !ins->targetSymbol && xl6IrTypeIsCOwned(source) && source->kind==XL6_IR_TYPE_BUILTIN &&
            source->cAbi==(binary ? XL6_IR_C_ABI_C_BYTES : XL6_IR_C_ABI_C_STRING) &&
            ins->operandCount==(index ? 2u : 1u) &&
            (!index || (key && xl6IrTypeGet(&module->types,key->type)->cAbi==XL6_IR_C_ABI_I64)) &&
            result->cAbi==(index && !binary ? XL6_IR_C_ABI_CHAR_U32 : XL6_IR_C_ABI_I64);
    }
    if(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SIZE_TAKE || ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_SIZE_TAKE) {
        const XL6_IrInstruction* length=ins->operandCount==2 ?
            xl6IrValueDefinition(module,module->operands[ins->firstOperand+1],NULL) : NULL;
        return !ins->targetSymbol && xl6IrTypeIsCOwned(source) && source->kind==XL6_IR_TYPE_BUILTIN &&
            source->cAbi==(ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_SIZE_TAKE ? XL6_IR_C_ABI_C_BYTES : XL6_IR_C_ABI_C_STRING) && ins->type==input->type &&
            length && xl6IrTypeGet(&module->types,length->type)->cAbi==XL6_IR_C_ABI_I64;
    }
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_TAKE || ins->runtimeOp==XL6_IR_RUNTIME_CBYTES_TAKE) {
        const XL6_IrNode* origin=xl6IrNodeGet(module,ins->origin);
        const XL6_IrNode* args=origin && origin->childCount==2 ?
            xl6IrNodeGet(module,module->children[origin->firstChild+1]) : NULL;
        if(!xl6IrCOwnedTakeNodeCompatible(module,origin) || ins->targetSymbol!=origin->targetSymbol ||
            ins->type!=origin->storageType || !xl6IrTypeIsCOwned(result) ||
            !args || (ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_TAKE ?
                result->cAbi!=XL6_IR_C_ABI_C_STRING || args->childCount!=2 : args->childCount!=3) ||
            ins->operandCount!=1 || source->cAbi!=XL6_IR_C_ABI_RAW_POINTER) return 0;
        return 1;
    }
    if(ins->targetSymbol || ins->operandCount!=1 || !xl6IrTypeIsCOwned(source))return 0;
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_COPY)
        return source->cAbi==XL6_IR_C_ABI_C_STRING && result->cAbi==XL6_IR_C_ABI_MANAGED_STRING &&
            (source->kind!=XL6_IR_TYPE_NULLABLE || result->kind==XL6_IR_TYPE_NULLABLE);
    if(ins->runtimeOp==XL6_IR_RUNTIME_CSTRING_BOX)
        return source->cAbi==XL6_IR_C_ABI_C_STRING && xl6IrTypeIsDynamicXValueBuiltin(module,ins->type);
    return source->cAbi==XL6_IR_C_ABI_C_BYTES &&
        ((result->cAbi==XL6_IR_C_ABI_MANAGED_BYTES &&
          (source->kind!=XL6_IR_TYPE_NULLABLE || result->kind==XL6_IR_TYPE_NULLABLE)) || xl6IrTypeIsDynamicXValueBuiltin(module,ins->type));
}
