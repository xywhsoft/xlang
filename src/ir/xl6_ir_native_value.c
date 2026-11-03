/* Included by xl6_ir.c. Native owning records have one closed representation
 * contract. Source spelling, foreign structs and a matching sizeof cannot
 * confer this capability. Capture is independently published as an immutable
 * native leaf capability. Owning nullable records have independent plans. */
static const XL6_IrNativeValuePlan g_xl6IrNativeValues[] = {
#define XL6_NATIVE_VALUE_PROFILE(Tag,Name,Abi,CName,Header,Result,Copy,Drop,Eq,Box,Project,Release,Type,Converter,Sequence,Profile,ValueId,CaptureHeader,CaptureProfile) \
    {XL6_IR_NATIVE_VALUE_##Tag,Abi,CName,Header,Result,Copy,Drop,Eq,Box,Project,XL6_IR_RELEASE_EFFECT_##Release, \
        XL6_IR_NATIVE_SLOT_##Tag,#Type "()",#Converter "()",Sequence,Profile,ValueId,CaptureHeader,CaptureProfile,XL6_IR_NATIVE_VALUE_NONE,NULL,false,false,NULL},
#include "src/type/xl6_native_value_profiles.inc"
#undef XL6_NATIVE_VALUE_PROFILE
    {XL6_IR_NATIVE_VALUE_NULLABLE_SLOT_MAP_KEY_V1,"xlang.runtime.nullable-slot-map-key.v1",
        "XL6_NullableSlotMapKeyV1","__xl_runtime/xl6_nullable_slot_map_key_abi.h","xlir_result_nullable_slot_map_key_v1",
        "xl6NullableSlotMapKeyCopyValueV1","xl6NullableSlotMapKeyDropValueV1","xl6NullableSlotMapKeyEqualV1",
        "xl6NullableSlotMapKeyBoxV1","xl6NullableSlotMapKeyUnboxV1",XL6_IR_RELEASE_EFFECT_NO_RAISE,
        XL6_IR_NATIVE_SLOT_NULLABLE_SLOT_MAP_KEY_V1,"xl6NullableSlotMapKeyTypeV1()","xl6NullableSlotMapKeyConverterV1()",
        "xl6SequenceNullableSlotMapKeyCreateV1","xl6NullableSlotMapKeyCollectionProfileV1Get",
        XL6_NULLABLE_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,"__xl_runtime/xl6_nullable_slot_map_key_capture.h",
        "xl6NullableSlotMapKeyCaptureProfileV1Get",XL6_IR_NATIVE_VALUE_SLOT_MAP_KEY_V1,"xl6NullableSlotMapKeyTryUnboxV1",false,false,NULL},
};
static const XL6_IrNativeValuePlan g_xl6IrForeignViewValue={
    .abi=XL6_IR_NATIVE_VALUE_FOREIGN_VIEW_V1,.abiName="xlang.foreign.view.v1",
    .cName="XL6_ForeignView*",.header="__xl_runtime/xl6_foreign_view_abi.h",
    .errorResultCName="xlir_result_foreign_view_v1",.copySymbol="xl6ForeignViewCopyValueV1",
    .dropSymbol="xl6ForeignViewDropValueV1",.equalSymbol="xl6ForeignViewEqualValueV1",
    .boxSymbol="xl6ForeignViewBoxV1",.projectSymbol="xl6ForeignViewUnboxV1",
    .releaseEffect=XL6_IR_RELEASE_EFFECT_MAY_RAISE,.slotAbi=XL6_IR_NATIVE_SLOT_FOREIGN_VIEW_V1,
    .typeExpr="xl6ForeignViewStorageTypeV1()",.converterExpr="xl6ForeignViewSequenceConverterV1()",
    .sequenceSymbol="xl6ForeignViewSequenceCreateV1",.captureHeader="__xl_runtime/xl6_foreign_view_capture.h",
    .captureProfileSymbol="xl6ForeignViewCaptureProfileV1Get",.graphCapture=true,.nominalDomainArguments=true,.nominalExtractSymbol="xl6ArrayExtractForeignViewV1"
};
static const XL6_IrNativeValuePlan g_xl6IrNullableForeignViewValue={
    .abi=XL6_IR_NATIVE_VALUE_NULLABLE_FOREIGN_VIEW_V1,.abiName="xlang.foreign.nullable-view.v1",
    .cName="XL6_ForeignView*",.header="__xl_runtime/xl6_foreign_view_abi.h",
    .errorResultCName="xlir_result_foreign_view_v1",.copySymbol="xl6ForeignViewCopyValueV1",
    .dropSymbol="xl6ForeignViewDropValueV1",.equalSymbol="xl6ForeignViewEqualValueV1",
    .boxSymbol="xl6ForeignViewBoxV1",.projectSymbol="xl6ForeignViewUnboxV1",
    .tryProjectSymbol="xl6ForeignViewTryUnboxV1",.nullableElementAbi=XL6_IR_NATIVE_VALUE_FOREIGN_VIEW_V1,
    .releaseEffect=XL6_IR_RELEASE_EFFECT_MAY_RAISE,.slotAbi=XL6_IR_NATIVE_SLOT_FOREIGN_VIEW_V1,
    .typeExpr="xl6ForeignViewStorageTypeV1()",.converterExpr="xl6ForeignViewSequenceConverterV1()",
    .sequenceSymbol="xl6ForeignViewSequenceCreateV1",.captureHeader="__xl_runtime/xl6_foreign_view_capture.h",
    .captureProfileSymbol="xl6ForeignViewCaptureProfileV1Get",.graphCapture=true,.nominalDomainArguments=true,.nominalExtractSymbol="xl6ArrayExtractForeignViewV1"
};
static const XL6_IrNativeValuePlan g_xl6IrScopeViewValue={
    .abi=XL6_IR_NATIVE_VALUE_FOREIGN_SCOPE_VIEW_V1,.abiName="xlang.foreign.scope-view.v1",
    .cName="XL6_ForeignScopeViewV1",.header="__xl_runtime/xl6_foreign_scope_value.h",
    .errorResultCName="xlir_result_foreign_scope_view_v1",
    .copySymbol="xl6ScopeViewCopyV1",
    .dropSymbol="xl6ScopeViewDropV1",
    .equalSymbol="xl6ScopeViewEqualV1",
    .boxSymbol="xl6ScopeViewBoxV1",
    .projectSymbol="xl6ScopeViewUnboxV1",
    .tryProjectSymbol="xl6ScopeViewTryUnboxV1",
    .releaseEffect=XL6_IR_RELEASE_EFFECT_NO_RAISE,.slotAbi=XL6_IR_NATIVE_SLOT_FOREIGN_SCOPE_VIEW_V1,
    .typeExpr="xl6ScopeViewTypeV1()",.converterExpr="xl6ScopeViewConverterV1()",
    .sequenceSymbol="xl6ScopeViewSequenceCreateV1",.captureHeader="__xl_runtime/xl6_foreign_scope_value.h",
    .captureProfileSymbol="xl6ScopeViewCaptureV1",.nominalDomainArguments=true,.nominalExtractSymbol="xl6ArrayExtractScopeViewV1"
};
static const XL6_IrNativeValuePlan g_xl6IrNullableScopeViewValue={
    .abi=XL6_IR_NATIVE_VALUE_NULLABLE_FOREIGN_SCOPE_VIEW_V1,.abiName="xlang.foreign.nullable-scope-view.v1",
    .cName="XL6_NullableForeignScopeViewV1",.header="__xl_runtime/xl6_foreign_scope_value.h",
    .errorResultCName="xlir_result_nullable_foreign_scope_view_v1",
    .copySymbol="xl6NullableScopeViewCopyV1",
    .dropSymbol="xl6NullableScopeViewDropV1",
    .equalSymbol="xl6NullableScopeViewEqualV1",
    .boxSymbol="xl6NullableScopeViewBoxV1",
    .projectSymbol="xl6NullableScopeViewUnboxV1",
    .tryProjectSymbol="xl6NullableScopeViewTryUnboxV1",
    .releaseEffect=XL6_IR_RELEASE_EFFECT_NO_RAISE,.slotAbi=XL6_IR_NATIVE_SLOT_NULLABLE_FOREIGN_SCOPE_VIEW_V1,
    .typeExpr="xl6NullableScopeViewTypeV1()",.converterExpr="xl6NullableScopeViewConverterV1()",
    .sequenceSymbol="xl6ScopeViewSequenceCreateV1",.captureHeader="__xl_runtime/xl6_foreign_scope_value.h",
    .captureProfileSymbol="xl6NullableScopeViewCaptureV1",.nominalDomainArguments=true,.nominalExtractSymbol="xl6ArrayExtractScopeViewV1",
    .nullableElementAbi=XL6_IR_NATIVE_VALUE_FOREIGN_SCOPE_VIEW_V1
};
static XL6_IrNativeValueAbi xl6IrNativeValueAbiFromFrontend(XL6_TypeAbiId abi)
{
    size_t i;
    for (i=0;i<XL6_ARRAY_COUNT(g_xl6IrNativeValues);++i)
        if (!g_xl6IrNativeValues[i].nullableElementAbi &&
            abi == xl6TypeAbiIdText(g_xl6IrNativeValues[i].abiName)) return g_xl6IrNativeValues[i].abi;
    return XL6_IR_NATIVE_VALUE_NONE;
}
XL6_IrClosureStorageAbi xl6IrClosureEnvironmentAbi(const XL6_IrModule* module,const XL6_IrClosureTarget* target)
{
    if(!module || !target || target->firstCapture>module->closureCaptureCount ||
        target->captureCount>module->closureCaptureCount-target->firstCapture)return XL6_IR_CLOSURE_STORAGE_NONE;
    XL6_IrClosureStorageAbi result=XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2;
    for(size_t i=0;i<target->captureCount;++i){
        const XL6_IrClosureCapture* capture=&module->closureCaptures[target->firstCapture+i];
        if(capture->abi!=XL6_IR_CLOSURE_CAPTURE_COPY_NATIVE_VALUE)continue;
        const XL6_IrNativeValuePlan* native=xl6IrOwnedTransferPlanForType(xl6IrTypeGet(&module->types,capture->type));
        if(!native || !native->captureHeader || !native->captureProfileSymbol)return XL6_IR_CLOSURE_STORAGE_NONE;
        XL6_IrClosureStorageAbi required=native->graphCapture ? XL6_IR_CLOSURE_STORAGE_RESIDENT_GRAPH_LAYOUT_V4 :
            XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V3;
        if(required>result)result=required;
    }
    return result;
}
XL6_IrClosureStorageAbi xl6IrModuleClosureStorageRequirement(const XL6_IrModule* module)
{
    if(!module)return XL6_IR_CLOSURE_STORAGE_NONE;
    XL6_IrClosureStorageAbi result=XL6_IR_CLOSURE_STORAGE_RESIDENT_LAYOUT_V2;
    for(size_t i=0;i<module->closureTargetCount;++i){
        if(module->closureTargets[i].reachability==XL6_IR_REACHABILITY_UNREACHABLE)continue;
        XL6_IrClosureStorageAbi required=xl6IrClosureEnvironmentAbi(module,&module->closureTargets[i]);
        if(required==XL6_IR_CLOSURE_STORAGE_NONE)return required;
        if(required>result)result=required;
    }
    return result;
}
XL6_IrCellStorageAbi xl6IrModuleCellStorageRequirement(const XL6_IrModule* module)
{
    if(!module)return XL6_IR_CELL_STORAGE_NONE;
    XL6_IrCellStorageAbi result=XL6_IR_CELL_STORAGE_RESIDENT_TRANSFER_V1;
    for(size_t i=0;i<module->storageCount;++i){
        if(module->storages[i].cellType==XL6_IR_TYPE_INVALID)continue;
        const XL6_IrNativeValuePlan* native=xl6IrOwnedTransferPlanForType(xl6IrTypeGet(&module->types,module->storages[i].type));
        if(!native)continue;
        XL6_IrCellStorageAbi required=native->graphCapture ? XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_GRAPH_V3 :
            XL6_IR_CELL_STORAGE_RESIDENT_NATIVE_LEAF_V2;
        if(required>result)result=required;
    }
    return result;
}
static XL6_IrNativeValueAbi xl6IrNativeValueAbiFromType(const XL6_IrType* type)
{
    if (!type || type->returnCount)
        return XL6_IR_NATIVE_VALUE_NONE;
    if(type->foreignHeader && type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK)
        return type->kind==XL6_IR_TYPE_CLASS ? XL6_IR_NATIVE_VALUE_FOREIGN_SCOPE_VIEW_V1 :
            type->kind==XL6_IR_TYPE_NULLABLE ? XL6_IR_NATIVE_VALUE_NULLABLE_FOREIGN_SCOPE_VIEW_V1 : XL6_IR_NATIVE_VALUE_NONE;
    if(type->foreignHeader && type->foreignPolicy &&
        (type->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED || type->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNER))
        return type->kind==XL6_IR_TYPE_CLASS ? XL6_IR_NATIVE_VALUE_FOREIGN_VIEW_V1 :
            type->kind==XL6_IR_TYPE_NULLABLE ? XL6_IR_NATIVE_VALUE_NULLABLE_FOREIGN_VIEW_V1 : XL6_IR_NATIVE_VALUE_NONE;
    if(type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params){
        for(size_t i=0;i<XL6_ARRAY_COUNT(g_xl6IrNativeValues);++i)
            if(g_xl6IrNativeValues[i].nullableElementAbi &&
                type->abiId==xl6TypeAbiIdText(g_xl6IrNativeValues[i].abiName))return g_xl6IrNativeValues[i].abi;
        return XL6_IR_NATIVE_VALUE_NONE;
    }
    if(type->kind!=XL6_IR_TYPE_BUILTIN || type->paramCount)return XL6_IR_NATIVE_VALUE_NONE;
    return xl6IrNativeValueAbiFromFrontend(type->abiId);
}
const XL6_IrNativeValuePlan* xl6IrNativeValuePlanForType(const XL6_IrType* type)
{
    size_t i;
    if (!type || type->cAbi != XL6_IR_C_ABI_NATIVE_VALUE ||
        type->ownershipAbi != XL6_IR_OWNERSHIP_ABI_MANAGED ||
        type->nativeValueAbi == XL6_IR_NATIVE_VALUE_NONE ||
        type->nativeValueAbi != xl6IrNativeValueAbiFromType(type)) return NULL;
    if(type->nativeValueAbi==XL6_IR_NATIVE_VALUE_FOREIGN_SCOPE_VIEW_V1 ||
        type->nativeValueAbi==XL6_IR_NATIVE_VALUE_NULLABLE_FOREIGN_SCOPE_VIEW_V1)
        return type->nullAbi==(type->kind==XL6_IR_TYPE_NULLABLE ? XL6_IR_NULL_ABI_TAGGED_VALUE : XL6_IR_NULL_ABI_NONE) ?
            (type->kind==XL6_IR_TYPE_NULLABLE ? &g_xl6IrNullableScopeViewValue : &g_xl6IrScopeViewValue) : NULL;
    if(type->nativeValueAbi==XL6_IR_NATIVE_VALUE_FOREIGN_VIEW_V1 ||
        type->nativeValueAbi==XL6_IR_NATIVE_VALUE_NULLABLE_FOREIGN_VIEW_V1)
        return type->nullAbi==XL6_IR_NULL_ABI_POINTER_SENTINEL ?
            (type->kind==XL6_IR_TYPE_NULLABLE ? &g_xl6IrNullableForeignViewValue : &g_xl6IrForeignViewValue) : NULL;
    for (i=0;i<XL6_ARRAY_COUNT(g_xl6IrNativeValues);++i)
        if (g_xl6IrNativeValues[i].abi == type->nativeValueAbi &&
            type->nullAbi==(g_xl6IrNativeValues[i].nullableElementAbi ? XL6_IR_NULL_ABI_TAGGED_VALUE : XL6_IR_NULL_ABI_NONE))
            return &g_xl6IrNativeValues[i];
    return NULL;
}
static const XL6_IrNativeValuePlan* xl6IrNullableNativeValuePlanForElement(const XL6_IrType* element)
{
    const XL6_IrNativeValuePlan* base=xl6IrNativeValuePlanForType(element);
    if(!base || base->nullableElementAbi)return NULL;
    if(base==&g_xl6IrForeignViewValue)return &g_xl6IrNullableForeignViewValue;
    if(base==&g_xl6IrScopeViewValue)return &g_xl6IrNullableScopeViewValue;
    for(size_t i=0;i<XL6_ARRAY_COUNT(g_xl6IrNativeValues);++i)
        if(g_xl6IrNativeValues[i].nullableElementAbi==base->abi)return &g_xl6IrNativeValues[i];
    return NULL;
}
const XL6_IrNativeValuePlan* xl6IrNullableNativeValuePlan(const XL6_IrTypePool* pool,const XL6_IrType* type)
{
    const XL6_IrNativeValuePlan* plan=xl6IrNativeValuePlanForType(type);
    const XL6_IrType* element=pool && type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params
        ? xl6IrTypeGet(pool,type->params[0]) : NULL;
    return plan && plan->nullableElementAbi && plan==xl6IrNullableNativeValuePlanForElement(element) ? plan : NULL;
}
XL6_IrCallErrorPhysicalAbi xl6IrRuntimeNativeValueErrorPhysicalAbi(XL6_IrRuntimeOp operation)
{
    if (operation == XL6_IR_RUNTIME_NATIVE_VALUE_COPY ||
        operation == XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED || operation == XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY)
        return XL6_IR_CALL_ERROR_PHYSICAL_STATUS_OUTPUT;
    return operation == XL6_IR_RUNTIME_NATIVE_VALUE_BOX
        ? XL6_IR_CALL_ERROR_PHYSICAL_NULL_RESULT : XL6_IR_CALL_ERROR_PHYSICAL_INVALID;
}
int xl6IrRuntimeNativeValueOpIs(XL6_IrRuntimeOp op)
{
    return (op>=XL6_IR_RUNTIME_NATIVE_VALUE_COPY && op<=XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED) ||
        op==XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY;
}
const char* xl6IrRuntimeNativeValueSymbol(const XL6_IrNativeValuePlan* plan,XL6_IrRuntimeOp op)
{
    if(!plan)return NULL;
    switch(op){
    case XL6_IR_RUNTIME_NATIVE_VALUE_COPY:return plan->copySymbol;
    case XL6_IR_RUNTIME_NATIVE_VALUE_EQUAL:return plan->equalSymbol;
    case XL6_IR_RUNTIME_NATIVE_VALUE_BOX:return plan->boxSymbol;
    case XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED:return plan->projectSymbol;
    case XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY:return plan->tryProjectSymbol;
    default:return NULL;
    }
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
        instruction->runtimeOp != XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY &&
        instruction->castAbi != XL6_IR_CAST_INVALID) return NULL;
    if (instruction->runtimeOp == XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_CHECKED ||
        instruction->runtimeOp == XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY) {
        int isTry=instruction->runtimeOp==XL6_IR_RUNTIME_NATIVE_VALUE_PROJECT_TRY;
        if (instruction->operandCount != 1 || instruction->payloadType != instruction->type ||
            instruction->castAbi != (isTry ? XL6_IR_CAST_XVALUE_TO_NULLABLE_NATIVE_VALUE_TRY :
                XL6_IR_CAST_XVALUE_TO_NATIVE_VALUE_CHECKED)) return NULL;
        left=xl6IrValueDefinition(module,module->operands[instruction->firstOperand],NULL);
        /* Physical checked projection is also used by exact typed collection
         * reads. Source `as` authorization lives in Typed IR; the lowered
         * operation's type, cast family and checked outcome are its contract,
         * not the spelling/kind of its diagnostic origin. */
        if (!left || !xl6IrTypeIsDynamicXValueBuiltin(module,left->type)) return NULL;
        type=xl6IrTypeGet(&module->types,instruction->type);
        return isTry ? xl6IrNullableNativeValuePlan(&module->types,type) : xl6IrNativeValuePlanForType(type);
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
