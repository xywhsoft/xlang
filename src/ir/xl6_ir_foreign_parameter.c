/* Sparse foreign parameter contracts. Declaration parsing ends before this
 * file; both lowering and Cgen consume these normalized rows. */
uint32 xl6IrForeignFunctionHeaderIndex(const XL6_IrModule* module,XL6_IrNodeId declaration)
{
    if(!module || module->foreignFunctionHeaderCount>module->foreignFunctionHeaderCapacity ||
        module->foreignFunctionHeaderCount>UINT32_MAX || module->foreignFunctionHeaderCount>module->nodeCount ||
        (module->foreignFunctionHeaderCount && !module->foreignFunctionHeaders))return 0;
    for(size_t i=0;i<module->foreignFunctionHeaderCount;++i)
        if(module->foreignFunctionHeaders[i].declaration==declaration)return (uint32)i+1;
    return 0;
}
const XL6_ForeignFunctionHeaderContract* xl6IrForeignFunctionHeaderGet(
    const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(!target || !target->foreignHeaderIndexPlusOne || !module ||
        target->foreignHeaderIndexPlusOne>module->foreignFunctionHeaderCount ||
        module->foreignFunctionHeaderCount>module->foreignFunctionHeaderCapacity || !module->foreignFunctionHeaders)return NULL;
    const XL6_ForeignFunctionHeaderContract* row=&module->foreignFunctionHeaders[target->foreignHeaderIndexPlusOne-1];
    return row->declaration==target->declaration ? row : NULL;
}
static int xl6IrVerifyFunctionHeaders(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignFunctionHeaderCount>module->foreignFunctionHeaderCapacity ||
        module->foreignFunctionHeaderCount>module->nodeCount ||
        (module->foreignFunctionHeaderCount && !module->foreignFunctionHeaders))goto invalid;
    for(size_t i=0;i<module->foreignFunctionHeaderCount;++i){
        const XL6_ForeignFunctionHeaderContract* row=&module->foreignFunctionHeaders[i];
        const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
        if(!node || node->kind!=XL6_IR_NODE_FUNC_DECL ||
            (node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC &&
             node->sourceKind!=XL6_AST_GET && node->sourceKind!=XL6_AST_LET) ||
            (node->flags&XL6_AST_FLAG_FOREIGN_FIELD) || node->foreignCall.receiverDereference ||
            !memchr(row->header,0,sizeof(row->header)) || !xl6ForeignHeaderName(row->header))goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignFunctionHeaders[j].declaration==row->declaration)goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native function headers require unique authoritative declaration contracts");
}
const XL6_ForeignPointerQualifierContract* xl6IrForeignPointerQualifierGet(
    const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t p)
{
    if(!module || !target || p>=target->parameterCount ||
        module->foreignPointerQualifierCount>module->foreignPointerQualifierCapacity ||
        (module->foreignPointerQualifierCount && !module->foreignPointerQualifiers))return NULL;
    for(size_t i=0;i<module->foreignPointerQualifierCount;++i)
        if(module->foreignPointerQualifiers[i].declaration==target->declaration &&
            module->foreignPointerQualifiers[i].parameter==p)return &module->foreignPointerQualifiers[i];
    return NULL;
}
static int xl6IrVerifyPointerQualifiers(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignPointerQualifierCount>module->foreignPointerQualifierCapacity ||
        (module->foreignPointerQualifierCount && !module->foreignPointerQualifiers))goto invalid;
    for(size_t i=0;i<module->foreignPointerQualifierCount;++i){
        const XL6_ForeignPointerQualifierContract* row=&module->foreignPointerQualifiers[i];
        const XL6_IrType* type=xl6IrPointerQualifierSourceType(module,row);char expected[80];
        if(!type || !memchr(row->cName,0,sizeof(row->cName)) ||
            !xl6IrPointerQualifierName(type,expected,sizeof(expected)) || strcmp(row->cName,expected))goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignPointerQualifiers[j].declaration==row->declaration &&
            module->foreignPointerQualifiers[j].parameter==row->parameter)goto invalid;
        for(size_t j=0;j<module->foreignValueCount;++j)if(module->foreignValues[j].declaration==row->declaration &&
            module->foreignValues[j].parameter==row->parameter)goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native pointer qualifiers require unique exact const pointee contracts");
}
int xl6IrForeignPointerQualifierMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t p)
{
    const XL6_ForeignPointerQualifierContract* row=xl6IrForeignPointerQualifierGet(module,target,p);
    if(!row || !target->parameterTypes || !target->parameterPhysicalAbis || !target->parameterAbis ||
        !target->parameterConstraintAbis || !target->parameterPhysicalFieldDeclarations ||
        target->kind!=XL6_IR_CALL_TARGET_EXTERN_C || target->runtimeBridge ||
        target->foreignOperationAbi!=XL6_IR_FOREIGN_OPERATION_CALL || target->parameterConstraintAbis[p] ||
        target->parameterPhysicalFieldDeclarations[p] || xl6IrForeignParameterGet(module,target,p))return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[p]);
    if(type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1)type=xl6IrTypeGet(&module->types,type->params[0]);
    char expected[80];
    if(type && type->kind==XL6_IR_TYPE_BUILTIN && type->cAbi==XL6_IR_C_ABI_RUNTIME_XVALUE_PTR && type->name && !strcmp(type->name,"var"))
        return !strcmp(row->cName,"const xvalue*") && target->parameterPhysicalAbis[p]==XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_XVALUE_BORROW &&
            target->parameterAbis[p]==XL6_IR_PARAMETER_ABI_BORROW;
    if(!type || type->kind!=XL6_IR_TYPE_CLASS || !type->foreignHeader ||
        !xl6ForeignConstPointerName(type->cName,expected,sizeof(expected)) || strcmp(expected,row->cName))return 0;
    int managed=xl6IrForeignManagedViewParameter(module,target,p);
    return target->parameterPhysicalAbis[p]==(managed ? XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_FOREIGN_VIEW_BORROW :
        XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_NATIVE_POINTER_DIRECT) &&
        (managed ? target->parameterAbis[p]==XL6_IR_PARAMETER_ABI_BORROW : type->ownershipAbi==XL6_IR_OWNERSHIP_ABI_TRIVIAL);
}
const XL6_ForeignValueContract* xl6IrForeignValueGet(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t p)
{
    if(!module || !target || p>=target->parameterCount)return NULL;
    for(size_t i=0;i<module->foreignValueCount;++i)
        if(module->foreignValues[i].declaration==target->declaration && module->foreignValues[i].parameter==p)return &module->foreignValues[i];
    return NULL;
}
int xl6IrForeignValueMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t p)
{
    const XL6_ForeignValueContract* row=xl6IrForeignValueGet(module,target,p);
    if(!target || p>=target->parameterCount || !target->parameterTypes || !target->parameterAbis ||
        !target->parameterConstraintAbis || !target->parameterPhysicalFieldDeclarations)return 0;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[p]);
    return row && type && type->kind==XL6_IR_TYPE_CLASS && type->foreignHeader &&
        (type->foreignPolicy || type->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) &&
        memchr(type->cName,0,sizeof(type->cName)) && strchr(type->cName,'*') && xl6IrForeignManagedViewParameter(module,target,p) &&
        target->parameterAbis[p]==XL6_IR_PARAMETER_ABI_BORROW && !target->parameterConstraintAbis[p] &&
        !target->parameterPhysicalFieldDeclarations[p] && !xl6IrForeignParameterGet(module,target,p) &&
        target->foreignOperationAbi==XL6_IR_FOREIGN_OPERATION_CALL;
}
static int xl6IrVerifyForeignValues(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignValueCount>module->foreignValueCapacity || (module->foreignValueCount && !module->foreignValues))goto invalid;
    for(size_t i=0;i<module->foreignValueCount;++i){
        const XL6_ForeignValueContract* row=&module->foreignValues[i];
        const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
        if(!node || row->parameter>=64 || !memchr(row->cName,0,sizeof(row->cName)) ||
            !xl6ForeignIdentifier(!strncmp(row->cName,"struct ",7) ? row->cName+7:row->cName))goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignValues[j].declaration==row->declaration && module->foreignValues[j].parameter==row->parameter)goto invalid;
        if(node->sourceKind!=XL6_AST_EXTERN_FUNC && node->sourceKind!=XL6_AST_NATIVE_EXTERN_FUNC &&
            node->sourceKind!=XL6_AST_GET && node->sourceKind!=XL6_AST_LET)goto invalid;
        size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || node->sourceKind==XL6_AST_GET ||
            node->sourceKind==XL6_AST_LET) && !(node->flags&XL6_AST_FLAG_STATIC) ? 1u:0u;
        if(node->kind!=XL6_IR_NODE_FUNC_DECL || node->childCount<2 ||
            (size_t)node->firstChild+1>=module->childCount)goto invalid;
        const XL6_IrNode* parameters=xl6IrNodeGet(module,module->children[node->firstChild+1]);
        if(!parameters || parameters->kind!=XL6_IR_NODE_LIST || row->parameter>=parameters->childCount+receiver)goto invalid;
        /* Parameter arrays are authenticated later by the call-target pass.
         * Do not traverse them while validating pre-lowering declaration rows. */
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native pointee values require unique exact non-null preparation contracts");
}
XL6_IrForeignOperationAbi xl6IrForeignOperationForTarget(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration) : NULL;
    if(!node || (!(node->flags&XL6_AST_FLAG_FOREIGN_FIELD) && !node->foreignCall.receiverDereference))return XL6_IR_FOREIGN_OPERATION_CALL;
    if(node->foreignCall.receiverDereference){
        if(node->foreignCall.receiverDereference!=1 || (node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_STATIC)) ||
            target->kind!=XL6_IR_CALL_TARGET_EXTERN_C)return XL6_IR_FOREIGN_OPERATION_INVALID;
        return node->sourceKind==XL6_AST_GET ? XL6_IR_FOREIGN_OPERATION_DEREF_READ :
            node->sourceKind==XL6_AST_LET ? XL6_IR_FOREIGN_OPERATION_DEREF_WRITE : XL6_IR_FOREIGN_OPERATION_INVALID;
    }
    if(target->kind!=XL6_IR_CALL_TARGET_EXTERN_C || (node->flags&XL6_AST_FLAG_STATIC))return XL6_IR_FOREIGN_OPERATION_INVALID;
    return node->sourceKind==XL6_AST_GET ? XL6_IR_FOREIGN_OPERATION_FIELD_READ :
        node->sourceKind==XL6_AST_LET ? XL6_IR_FOREIGN_OPERATION_FIELD_WRITE : XL6_IR_FOREIGN_OPERATION_INVALID;
}
const XL6_ForeignFieldLengthContract* xl6IrForeignFieldLengthGet(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(!module || !target)return NULL;
    for(size_t i=0;i<module->foreignFieldLengthCount;++i)
        if(module->foreignFieldLengths[i].declaration==target->declaration)return &module->foreignFieldLengths[i];
    return NULL;
}
const XL6_ForeignStorageContract* xl6IrForeignStorageGet(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(!module || !target)return NULL;
    for(size_t i=0;i<module->foreignStorageCount;++i)
        if(module->foreignStorage[i].declaration==target->declaration)return &module->foreignStorage[i];
    return NULL;
}
const XL6_ForeignRetainContract* xl6IrForeignRetainGet(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(!module || !target)return NULL;
    for(size_t i=0;i<module->foreignRetainCount;++i)
        if(module->foreignRetains[i].declaration==target->declaration)return &module->foreignRetains[i];
    return NULL;
}
const XL6_ForeignCallbackSpanContract* xl6IrForeignCallbackSpanGet(const XL6_IrModule* module,
    const XL6_ForeignParameterContract* callback,size_t argument)
{
    if(!module || !callback)return NULL;
    for(size_t i=0;i<module->foreignCallbackSpanCount;++i){
        const XL6_ForeignCallbackSpanContract* row=&module->foreignCallbackSpans[i];
        if(row->declaration==callback->declaration && row->parameter==callback->parameter && row->argument==argument)return row;
    }
    return NULL;
}
static int xl6IrVerifyForeignCallbackSpans(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignCallbackSpanCount>module->foreignCallbackSpanCapacity ||
        (module->foreignCallbackSpanCount && !module->foreignCallbackSpans))goto invalid;
    for(size_t i=0;i<module->foreignCallbackSpanCount;++i){
        const XL6_ForeignCallbackSpanContract* row=&module->foreignCallbackSpans[i];
        const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
        if(!node || row->parameter>=64 || row->argument>=64 ||
            !((node->foreignCall.retainedCallbacks|node->foreignCall.physicalPairs)&(UINT64_C(1)<<row->parameter)) ||
            (row->countAbi!=XL6_FOREIGN_SPAN_COUNT_INT && row->countAbi!=XL6_FOREIGN_SPAN_COUNT_SIZE) ||
            !memchr(row->pointerCName,0,sizeof(row->pointerCName)) ||
            (strcmp(row->pointerCName,"const void*") && strcmp(row->pointerCName,"const uint8_t*") &&
             strcmp(row->pointerCName,"const unsigned char*") && strcmp(row->pointerCName,"const char*")))goto invalid;
        const XL6_ForeignParameterContract* callback=NULL;
        for(size_t p=0;p<module->foreignParameterCount;++p)
            if(module->foreignParameters[p].declaration==row->declaration && module->foreignParameters[p].parameter==row->parameter)
                callback=&module->foreignParameters[p];
        if(!callback || callback->kind==XL6_FOREIGN_PARAMETER_SPAN)goto invalid;
        for(size_t j=0;j<i;++j){
            const XL6_ForeignCallbackSpanContract* previous=&module->foreignCallbackSpans[j];
            if(previous->declaration==row->declaration && previous->parameter==row->parameter && previous->argument==row->argument)goto invalid;
        }
    }
    for(size_t n=0;n<module->nodeCount;++n){
        size_t count=0;
        for(size_t i=0;i<module->foreignCallbackSpanCount;++i)count+=module->foreignCallbackSpans[i].declaration==n+1;
        if(module->nodes[n].foreignCall.callbackSpanMappings!=count)goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"callback byte spans require unique normalized physical pointer/count mappings");
}
static int xl6IrForeignOwnershipHelperMatches(const XL6_IrModule* module,
    XL6_SymbolId symbol,XL6_IrTypeId input,int status)
{
    const XL6_IrCallTarget* helper=xl6IrCallTargetGet(module,symbol);
    const XL6_IrNode* node=helper ? xl6IrNodeGet(module,helper->declaration):NULL;
    const XL6_IrType* result=helper ? xl6IrTypeGet(&module->types,helper->returnType):NULL;
    return helper && node && node->sourceKind==XL6_AST_EXTERN_FUNC &&
        (node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) &&
        !(node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) &&
        xl6ForeignOwnershipHelperFacts(&node->foreignCall,status) &&
        helper->kind==XL6_IR_CALL_TARGET_EXTERN_C && helper->foreignOperationAbi==XL6_IR_FOREIGN_OPERATION_CALL &&
        !helper->runtimeBridge && !helper->variadicAbi && !helper->nativeServicesRequired &&
        !helper->nativeContextAbi && !helper->nativeResultAbi && !helper->nativeArrayResultAbi &&
        helper->parameterCount==1 && helper->parameterTypes && helper->parameterTypes[0]==input &&
        helper->parameterAbis && helper->parameterAbis[0]==XL6_IR_PARAMETER_ABI_BORROW &&
        helper->parameterPhysicalAbis && helper->parameterPhysicalAbis[0]==XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW &&
        (status ? result && result->cAbi==XL6_IR_C_ABI_C_INT : !helper->returnType) &&
        helper->resultPhysicalAbi==XL6_IR_CALL_RESULT_PHYSICAL_DIRECT &&
        (node->foreignCall.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_INT_STATUS ?
            !status && helper->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_INT_STATUS &&
            helper->nativeSuccessCode==node->foreignCall.successCode &&
            helper->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_STATUS_CHECK :
            helper->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_STORAGE && !helper->nativeSuccessCode &&
            helper->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_DIRECT);
}
int xl6IrForeignRetainResultMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration):NULL;
    const XL6_ForeignRetainContract* row=xl6IrForeignRetainGet(module,target);
    if(!node)return 0;
    if(node->foreignCall.borrowedViewResult!=XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE)return !row;
    XL6_IrTypeId input=target->returnType;
    const XL6_IrType* result=xl6IrTypeGet(&module->types,input);
    if(result && result->kind==XL6_IR_TYPE_NULLABLE && result->paramCount==1 && result->params){
        input=result->params[0];result=xl6IrTypeGet(&module->types,input);
    }
    return row && result && result->kind==XL6_IR_TYPE_CLASS && result->foreignHeader &&
        result->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED && !result->foreignScope && !result->foreignWatch &&
        (row->success==XL6_FOREIGN_RETAIN_ZERO || row->success==XL6_FOREIGN_RETAIN_POSITIVE) &&
        !node->foreignCall.outputIndexPlusOne && !node->foreignCall.inplaceOutput &&
        (node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) &&
        !(node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) &&
        target->kind==XL6_IR_CALL_TARGET_EXTERN_C && !target->runtimeBridge && !target->variadicAbi &&
        !target->nativeServicesRequired && !target->nativeContextAbi && !target->nativeResultAbi && !target->nativeArrayResultAbi &&
        target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_NATIVE_VIEW_ADMIT &&
        xl6IrForeignOwnershipHelperMatches(module,row->retainSymbol,input,1) &&
        xl6IrForeignOwnershipHelperMatches(module,row->rollbackSymbol,input,0);
}
static int xl6IrVerifyForeignRetains(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignRetainCount>module->foreignRetainCapacity ||
        (module->foreignRetainCount && !module->foreignRetains))goto invalid;
    for(size_t i=0;i<module->foreignRetainCount;++i){
        const XL6_ForeignRetainContract* row=&module->foreignRetains[i];
        const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
        if(!node || node->foreignCall.borrowedViewResult!=XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE ||
            !row->retainSymbol || !row->rollbackSymbol ||
            (row->success!=XL6_FOREIGN_RETAIN_ZERO && row->success!=XL6_FOREIGN_RETAIN_POSITIVE))goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignRetains[j].declaration==row->declaration)goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n){
        size_t found=0;
        for(size_t i=0;i<module->foreignRetainCount;++i)found+=module->foreignRetains[i].declaration==n+1;
        if(found!=(module->nodes[n].foreignCall.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_RETAINED_NATIVE))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"retained native results require one normalized retain/release contract per declaration");
}
static int xl6IrVerifyForeignStorage(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignStorageCount>module->foreignStorageCapacity ||
        (module->foreignStorageCount && !module->foreignStorage))goto invalid;
    for(size_t i=0;i<module->foreignStorageCount;++i){
        const XL6_ForeignStorageContract* row=&module->foreignStorage[i];
        const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
        const XL6_IrNode* adopt=xl6IrSymbolNodeGet(module,row->adoptSymbol);
        if(!node || (node->foreignCall.inplaceOutput!=XL6_FOREIGN_INPLACE_STATUS && node->foreignCall.inplaceOutput!=XL6_FOREIGN_INPLACE_VOID) ||
            (row->kind!=XL6_FOREIGN_STORAGE_POLICY && row->kind!=XL6_FOREIGN_STORAGE_NATIVE_ADOPTION) ||
            (row->kind==XL6_FOREIGN_STORAGE_POLICY && (row->adoptSymbol || row->successCode)) ||
            (row->kind==XL6_FOREIGN_STORAGE_NATIVE_ADOPTION && (!adopt ||
                adopt->sourceKind!=XL6_AST_EXTERN_FUNC || !(adopt->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
                (adopt->flags&XL6_AST_FLAG_FOREIGN_PURE))) || !memchr(row->recordCName,0,sizeof(row->recordCName)) ||
            !xl6ForeignIdentifier(!strncmp(row->recordCName,"struct ",7) ? row->recordCName+7 : row->recordCName))goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignStorage[j].declaration==row->declaration)goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n){
        size_t found=0;
        for(size_t i=0;i<module->foreignStorageCount;++i)found+=module->foreignStorage[i].declaration==n+1;
        if(found!=(module->nodes[n].foreignCall.inplaceOutput ? 1u:0u))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"in-place native storage requires one normalized adoption contract per factory");
}
int xl6IrForeignStorageFactoryMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration):NULL;
    const XL6_ForeignStorageContract* row=xl6IrForeignStorageGet(module,target);
    if(!node)return 0;
    if(!node->foreignCall.inplaceOutput)return !row;
    const XL6_IrType* result=xl6IrTypeGet(&module->types,target->returnType);
    if(!((node->foreignCall.inplaceOutput==XL6_FOREIGN_INPLACE_STATUS || node->foreignCall.inplaceOutput==XL6_FOREIGN_INPLACE_VOID) && row && result && result->kind==XL6_IR_TYPE_CLASS &&
        result->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNED && result->foreignHeader &&
        !result->foreignWatch && !result->foreignScope &&
        node->foreignCall.outputIndexPlusOne && !node->foreignCall.valueOutput &&
        node->foreignCall.borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE &&
        target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_NATIVE_VIEW_ADMIT &&
        target->outputResultAbi==xl6IrCallOutputResultAbiForTarget(module,target)))return 0;
    if(row->kind==XL6_FOREIGN_STORAGE_POLICY)return !row->adoptSymbol && !row->successCode;
    return row->kind==XL6_FOREIGN_STORAGE_NATIVE_ADOPTION &&
        xl6IrForeignOwnershipHelperMatches(module,row->adoptSymbol,target->returnType,1);
}
static int xl6IrVerifyForeignFieldLengths(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignFieldLengthCount>module->foreignFieldLengthCapacity ||
        (module->foreignFieldLengthCount && !module->foreignFieldLengths))goto invalid;
    for(size_t i=0;i<module->foreignFieldLengthCount;++i){
        const XL6_ForeignFieldLengthContract* row=&module->foreignFieldLengths[i];
        const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
        if(!node || !memchr(row->field,0,sizeof(row->field)) || !xl6ForeignIdentifier(row->field) ||
            (row->countAbi!=XL6_FOREIGN_SPAN_COUNT_INT && row->countAbi!=XL6_FOREIGN_SPAN_COUNT_SIZE) ||
            !xl6ForeignResultCountedField(node->foreignCall.borrowedViewResult) || node->sourceKind!=XL6_AST_GET ||
            !(node->flags&XL6_AST_FLAG_FOREIGN_FIELD) || (node->flags&XL6_AST_FLAG_STATIC))goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignFieldLengths[j].declaration==row->declaration)goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n){
        const XL6_IrNode* node=&module->nodes[n];
        if(!xl6ForeignResultCountedField(node->foreignCall.borrowedViewResult))continue;
        size_t found=0;
        for(size_t i=0;i<module->foreignFieldLengthCount;++i)found+=module->foreignFieldLengths[i].declaration==n+1;
        if(found!=1)goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"counted native fields require unique normalized length contracts");
}
static int xl6IrForeignMultiOutput(const XL6_IrNode* node)
{return node && (node->foreignCall.valueOutput==XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS || node->foreignCall.valueOutput==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID);}
static const XL6_ForeignOutputContract* xl6IrForeignOutputGet(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t ordinal)
{
    if(!module || !target || module->foreignOutputCount>module->foreignOutputCapacity || (module->foreignOutputCount && !module->foreignOutputs))return NULL;
    for(size_t i=0;i<module->foreignOutputCount;++i)
        if(module->foreignOutputs[i].declaration==target->declaration && module->foreignOutputs[i].ordinal==ordinal)return &module->foreignOutputs[i];
    return NULL;
}
size_t xl6IrForeignOutputCount(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration):NULL;
    if(!node || !node->foreignCall.valueOutput)return 0;
    if(!xl6IrForeignMultiOutput(node))return 1;
    if(module->foreignOutputCount>module->foreignOutputCapacity || (module->foreignOutputCount && !module->foreignOutputs))return 0;
    size_t count=0;
    for(size_t i=0;i<module->foreignOutputCount;++i)count+=module->foreignOutputs[i].declaration==target->declaration;
    return count;
}
XL6_IrTypeId xl6IrForeignOutputType(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t ordinal)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration):NULL;
    if(!node || !node->foreignCall.valueOutput)return 0;
    if(!xl6IrForeignMultiOutput(node))return ordinal ? 0 : target->returnType;
    const XL6_IrType* result=xl6IrTypeGet(&module->types,target->returnType);
    return result && result->kind==XL6_IR_TYPE_TUPLE && result->params && ordinal<result->paramCount ? result->params[ordinal] : 0;
}
int xl6IrForeignOutputStep(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t step,size_t* input,size_t* output)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration):NULL;
    if(!node || !input || !output)return 0;
    size_t count=xl6IrForeignOutputCount(module,target);
    size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || node->sourceKind==XL6_AST_GET || node->sourceKind==XL6_AST_LET) && !(node->flags&XL6_AST_FLAG_STATIC) ? 1u:0u;
    size_t physical=0,p=0,o=0;
    if(step>=target->parameterCount+count)return 0;
    for(size_t event=0;event<=step;++event){
        const XL6_ForeignOutputContract* row=xl6IrForeignMultiOutput(node) ? xl6IrForeignOutputGet(module,target,o):NULL;
        size_t slot=row ? row->physicalIndex : node->foreignCall.outputIndexPlusOne-1;
        if(o<count && slot==physical){
            *input=SIZE_MAX;*output=o++;++physical;
        }else{
            if(p>=target->parameterCount || (o<count && slot<physical))return 0;
            *input=p;*output=SIZE_MAX;
            size_t explicitParameter=p>=receiver ? p-receiver:SIZE_MAX;
            physical+=explicitParameter<64 && (node->foreignCall.physicalPairs&(UINT64_C(1)<<explicitParameter)) ? 2 :
                explicitParameter<64 && (node->foreignCall.retainedCallbacks&(UINT64_C(1)<<explicitParameter)) ? 3 : 1;
            ++p;
        }
    }
    return 1;
}
static int xl6IrVerifyForeignOutputs(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignOutputCount>module->foreignOutputCapacity || (module->foreignOutputCount && !module->foreignOutputs))goto invalid;
    for(size_t i=0;i<module->foreignOutputCount;++i){
        const XL6_ForeignOutputContract* row=&module->foreignOutputs[i];
        const XL6_IrNode* node=xl6IrNodeGet(module,row->declaration);
        if(!xl6IrForeignMultiOutput(node) || node->kind!=XL6_IR_NODE_FUNC_DECL || row->ordinal>=64 || row->physicalIndex>=64)goto invalid;
        for(size_t j=0;j<i;++j)if(module->foreignOutputs[j].declaration==row->declaration &&
            (module->foreignOutputs[j].ordinal==row->ordinal || module->foreignOutputs[j].physicalIndex==row->physicalIndex))goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n){
        const XL6_IrNode* node=&module->nodes[n];if(!xl6IrForeignMultiOutput(node))continue;
        XL6_IrCallTarget target={0};target.declaration=(XL6_IrNodeId)(n+1);
        size_t count=xl6IrForeignOutputCount(module,&target);uint32 slots[64];
        const XL6_IrNode* params=node->childCount>1 ? xl6IrNodeGet(module,module->children[node->firstChild+1]):NULL;
        const XL6_IrNode* returns=node->childCount>2 ? xl6IrNodeGet(module,module->children[node->firstChild+2]):NULL;
        size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || node->sourceKind==XL6_AST_GET || node->sourceKind==XL6_AST_LET) && !(node->flags&XL6_AST_FLAG_STATIC) ? 1u:0u;
        if(count<2 || count>64 || !params || !returns || returns->childCount!=count)goto invalid;
        for(size_t o=0;o<count;++o){
            const XL6_ForeignOutputContract* row=xl6IrForeignOutputGet(module,&target,o);
            if(!row || (o && row->physicalIndex<=slots[o-1]))goto invalid;
            slots[o]=row->physicalIndex;
        }
        if(node->foreignCall.outputIndexPlusOne!=slots[0]+1 ||
            !xl6ForeignMultiOutputSlotsValid(&node->foreignCall,params->childCount+receiver,receiver,slots,count))goto invalid;
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native multi-output rows must exactly cover increasing complete C slots and language results");
}
int xl6IrForeignOutputValueMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration) : NULL;
    if(!node || !node->foreignCall.valueOutput)return 0;
    size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || node->sourceKind==XL6_AST_GET || node->sourceKind==XL6_AST_LET) && !(node->flags&XL6_AST_FLAG_STATIC) ? 1u:0u;
    size_t count=xl6IrForeignOutputCount(module,target);
    if(!count || count>64)return 0;
    for(size_t o=0;o<count;++o)if(!xl6IrRawCCallbackTrivialValueType(xl6IrTypeGet(&module->types,xl6IrForeignOutputType(module,target,o))))return 0;
    if(xl6IrForeignMultiOutput(node)){
        const XL6_IrType* result=xl6IrTypeGet(&module->types,target->returnType);
        if(count<2 || !result || result->kind!=XL6_IR_TYPE_TUPLE || result->paramCount!=count)return 0;
    }else if(xl6ForeignOutputLogical(&node->foreignCall,target->parameterCount,receiver)==SIZE_MAX)return 0;
    for(size_t step=0;step<target->parameterCount+count;++step){size_t in,out;if(!xl6IrForeignOutputStep(module,target,step,&in,&out))return 0;}
    return node->foreignCall.valueOutput<=XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID &&
        ((node->foreignCall.valueOutput!=XL6_FOREIGN_VALUE_OUTPUT_VOID && node->foreignCall.valueOutput!=XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID) || !node->foreignCall.successCode) &&
        target->outputResultAbi==xl6IrCallOutputResultAbiForTarget(module,target) && node->foreignCall.outputIndexPlusOne &&
        !node->foreignCall.rollbackSymbol && !node->foreignCall.borrowedViewResult &&
        (node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) && !(node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) &&
        target->kind==XL6_IR_CALL_TARGET_EXTERN_C && !target->runtimeBridge &&
        !target->nativeServicesRequired && !target->nativeContextAbi && !target->nativeResultAbi && !target->nativeArrayResultAbi &&
        target->variadicAbi==XL6_IR_CALL_VARIADIC_NONE &&
        target->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_STORAGE && target->resultPhysicalAbi==XL6_IR_CALL_RESULT_PHYSICAL_DIRECT &&
        target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_NATIVE_OUTPUT_VALUE;
}
int xl6IrForeignManagedPointeeWrite(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    if(!module || !target || target->foreignOperationAbi!=XL6_IR_FOREIGN_OPERATION_DEREF_WRITE ||
       target->parameterCount!=2 || !target->parameterTypes || target->returnType)return 0;
    const XL6_IrType* receiver=xl6IrTypeGet(&module->types,target->parameterTypes[0]);
    const XL6_IrType* value=xl6IrTypeGet(&module->types,target->parameterTypes[1]);
    const XL6_IrType* root=xl6IrForeignRootType(module,target->parameterTypes[0]);
    return receiver && value && root && receiver->kind==XL6_IR_TYPE_CLASS &&
        receiver->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK &&
        value->foreignLifetime==XL6_FOREIGN_LIFETIME_OWNER &&
        xl6IrNativeValuePlanForType(receiver) && xl6IrNativeValuePlanForType(value) &&
        xl6IrForeignRootType(module,target->parameterTypes[1])==root;
}
int xl6IrForeignProjectionMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    XL6_IrForeignOperationAbi op=xl6IrForeignOperationForTarget(module,target);
    if(!target || target->foreignOperationAbi!=op || op==XL6_IR_FOREIGN_OPERATION_INVALID)return 0;
    if(op==XL6_IR_FOREIGN_OPERATION_CALL)return 1;
    const XL6_IrNode* node=xl6IrNodeGet(module,target->declaration);
    const XL6_IrType* owner=target->parameterTypes && target->parameterCount ? xl6IrTypeGet(&module->types,target->parameterTypes[0]) : NULL;
    const XL6_ForeignCallFacts* f=&node->foreignCall;
    if(!owner || owner->kind!=XL6_IR_TYPE_CLASS || !owner->foreignHeader || !node->declarationOwnerType ||
        target->runtimeBridge || target->implementationAbi || target->nativeServicesRequired || target->nativeContextAbi ||
        target->nativeResultAbi || target->nativeArrayResultAbi || target->variadicAbi ||
        !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (node->flags&XL6_AST_FLAG_FOREIGN_OWNED_STRING) ||
        !xl6ForeignIdentifier(target->cName) || !node->text ||
        f->constRecordAddresses || f->staticCallbackAddresses || f->textViews || f->bytesViews || f->terminatedStrings ||
        f->physicalPairs || f->retainedCallbacks || f->controlViews || f->outputIndexPlusOne ||
        f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE || f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_BOOL_STATUS || f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_INT_STATUS)return 0;
    size_t bytes=strlen(node->text);
    if(bytes<2 || node->text[0]!='"' || node->text[bytes-1]!='"' || strlen(target->cName)!=bytes-2 ||
        memcmp(node->text+1,target->cName,bytes-2))return 0;
    int read=op==XL6_IR_FOREIGN_OPERATION_FIELD_READ || op==XL6_IR_FOREIGN_OPERATION_DEREF_READ;
    const XL6_IrType* value=read ? xl6IrTypeGet(&module->types,target->returnType) :
        target->parameterCount==2 ? xl6IrTypeGet(&module->types,target->parameterTypes[1]) : NULL;
    return value && (f->borrowedViewResult || xl6IrRawCCallbackTrivialValueType(value) || xl6IrForeignManagedPointeeWrite(module,target)) &&
        (read ? target->parameterCount==1 :
            !target->returnType && target->parameterCount==2 && !f->borrowedViewResult && !(node->flags&XL6_AST_FLAG_FOREIGN_PURE));
}

const XL6_IrClassField* xl6IrForeignSpanBufferField(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* contract=xl6IrForeignParameterGet(module,target,parameter);
    if(!contract || contract->kind!=XL6_FOREIGN_PARAMETER_SPAN)return NULL;
    const XL6_IrClassField* field=xl6IrManagedNativeConstructorField(module,target->parameterTypes[parameter]);
    return field && field->nativeHandlePolicyCName &&
        !strcmp(field->nativeHandlePolicyCName,"xl6BufferHandlePolicyV1Get") ? field : NULL;
}
const XL6_IrType* xl6IrForeignCallbackSignature(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* contract=xl6IrForeignParameterGet(module,target,parameter);
    if(!contract || contract->kind==XL6_FOREIGN_PARAMETER_SPAN)return NULL;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[parameter]);
    if(type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params)
        type=xl6IrTypeGet(&module->types,type->params[0]);
    return type && type->kind==XL6_IR_TYPE_FUNCTION ? type : NULL;
}
size_t xl6IrForeignParameterWidth(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* contract=xl6IrForeignParameterGet(module,target,parameter);
    return !contract ? 1 : contract->kind==XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK ? 3 : 2;
}
static int xl6IrForeignParameterRowValid(const XL6_ForeignParameterContract* row)
{
    if(!row || row->parameter>=64 || !memchr(row->header,0,sizeof(row->header)) ||
        (row->header[0] && ((row->kind!=XL6_FOREIGN_PARAMETER_SPAN && row->kind!=XL6_FOREIGN_PARAMETER_SYNC_CALLBACK) || !xl6ForeignHeaderName(row->header))))return 0;
    if(row->kind==XL6_FOREIGN_PARAMETER_SPAN) {
        if(row->callbackOwnerCName[0] || row->callbackOwnerIndexPlusOne || row->callbackFailureCode || row->callbackUserFirst || row->callbackResultKind ||
            (row->countAbi!=XL6_FOREIGN_SPAN_COUNT_SIZE && row->countAbi!=XL6_FOREIGN_SPAN_COUNT_INT) ||
            !memchr(row->pointerCName,0,sizeof(row->pointerCName)) || !memchr(row->elementCName,0,sizeof(row->elementCName)) ||
            !xl6ForeignPointerSpelling(row->pointerCName))return 0;
        const char* element=row->pointerCName;
        bool readOnly=!strncmp(element,"const ",6);
        if(readOnly)element+=6;
        const char* end=strchr(element,'*');
        if(!end || end[1])return 0;
        size_t bytes=(size_t)(end-element);
        while(bytes && element[bytes-1]==' ')--bytes;
        return bytes && readOnly==row->readOnly && strlen(row->elementCName)==bytes && !memcmp(element,row->elementCName,bytes);
    }
    return (row->kind==XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK || row->kind==XL6_FOREIGN_PARAMETER_SYNC_CALLBACK) && !row->pointerCName[0] && !row->elementCName[0] &&
        (row->callbackResultKind==XL6_FOREIGN_CALLBACK_RESULT_TRIVIAL || row->callbackResultKind==XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT) &&
        !row->countAbi && !row->readOnly && row->callbackOwnerIndexPlusOne<=64 &&
        (row->kind==XL6_FOREIGN_PARAMETER_SYNC_CALLBACK ? !row->callbackOwnerCName[0] && !row->callbackResultKind :
         memchr(row->callbackOwnerCName,0,sizeof(row->callbackOwnerCName)) && xl6ForeignIdentifier(row->callbackOwnerCName));
}
static int xl6IrVerifyForeignParameters(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignParameterCount>module->foreignParameterCapacity || (module->foreignParameterCount && !module->foreignParameters))goto invalid;
    for(size_t i=0;i<module->foreignParameterCount;++i) {
        const XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        char expected[80]={0};
        if(row->kind!=XL6_FOREIGN_PARAMETER_SPAN)
            snprintf(expected,sizeof(expected),row->kind==XL6_FOREIGN_PARAMETER_SYNC_CALLBACK ? "xlir_sync_d%u_p%u" : "xlir_retained_d%u_p%u",(unsigned)row->declaration,(unsigned)row->parameter);
        if(!memchr(row->callbackTypeCName,0,sizeof(row->callbackTypeCName)) || strcmp(expected,row->callbackTypeCName))goto invalid;
        const XL6_IrNode* declaration=xl6IrNodeGet(module,row->declaration);
        if(!xl6IrForeignParameterRowValid(row) || !declaration || declaration->kind!=XL6_IR_NODE_FUNC_DECL || declaration->childCount<2)goto invalid;
        uint64 bit=UINT64_C(1)<<row->parameter;
        if(!(bit&(row->kind!=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK ? declaration->foreignCall.physicalPairs : declaration->foreignCall.retainedCallbacks)))goto invalid;
        const XL6_IrNode* params=xl6IrNodeGet(module,module->children[declaration->firstChild+1]);
        if(!params || params->kind!=XL6_IR_NODE_LIST || row->parameter>=params->childCount)goto invalid;
        for(size_t j=0;j<i;++j)
            if(module->foreignParameters[j].declaration==row->declaration && module->foreignParameters[j].parameter==row->parameter)goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n) {
        const XL6_IrNode* declaration=&module->nodes[n];
        uint64 mask=declaration->foreignCall.physicalPairs|declaration->foreignCall.retainedCallbacks;
        for(size_t p=0;p<64;++p)if(mask&(UINT64_C(1)<<p)) {
            int found=0;
            for(size_t i=0;i<module->foreignParameterCount;++i)
                found+=module->foreignParameters[i].declaration==n+1 && module->foreignParameters[i].parameter==p;
            if(found!=1)goto invalid;
        }
    }
    return 1;
invalid:
    return xl6IrDiag(diags,NULL,"foreign parameter expansions require unique normalized declaration rows");
}
int xl6IrForeignSpanParameterMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,parameter);
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[parameter]);
    if(!row || row->kind!=XL6_FOREIGN_PARAMETER_SPAN || !type ||
        target->parameterAbis[parameter]!=((type->cAbi==XL6_IR_C_ABI_C_ARRAY || type->cAbi==XL6_IR_C_ABI_C_RECORD) ? XL6_IR_PARAMETER_ABI_VALUE : XL6_IR_PARAMETER_ABI_BORROW) ||
        target->parameterConstraintAbis[parameter])return 0;
    const XL6_IrClassField* buffer=xl6IrForeignSpanBufferField(module,target,parameter);
    if(buffer)return type->kind==XL6_IR_TYPE_CLASS && target->parameterPhysicalFieldDeclarations[parameter]==buffer->declaration;
    if(target->parameterPhysicalFieldDeclarations[parameter])return 0;
    return row->readOnly && ((type->kind==XL6_IR_TYPE_BUILTIN && (type->cAbi==XL6_IR_C_ABI_MANAGED_STRING ||
        type->cAbi==XL6_IR_C_ABI_MANAGED_BYTES)) || type->cAbi==XL6_IR_C_ABI_C_ARRAY || type->cAbi==XL6_IR_C_ABI_C_RECORD);
}
int xl6IrForeignCallbackParameterMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,parameter);
    const XL6_IrType* signature=xl6IrForeignCallbackSignature(module,target,parameter);
    const XL6_IrNode* declaration=xl6IrNodeGet(module,target->declaration);
    if(!declaration || (declaration->flags&XL6_AST_FLAG_FOREIGN_PURE) ||
        (row && row->kind==XL6_FOREIGN_PARAMETER_SYNC_CALLBACK && (declaration->flags&XL6_AST_FLAG_FOREIGN_NONBLOCKING)))return 0;
    if(!row || !signature || !signature->typeValueId || signature->returnCount>1 ||
        signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED ||
        target->parameterAbis[parameter]!=XL6_IR_PARAMETER_ABI_BORROW || target->parameterPhysicalFieldDeclarations[parameter] ||
        target->parameterConstraintAbis[parameter] ||
        row->callbackOwnerIndexPlusOne>target->parameterCount)return 0;
    size_t source=row->callbackOwnerIndexPlusOne-1;
    const XL6_IrType* owner=source<target->parameterCount ? xl6IrTypeGet(&module->types,target->parameterTypes[source]) : NULL;
    const XL6_IrType* root=source<target->parameterCount ? xl6IrForeignRootType(module,target->parameterTypes[source]) : NULL;
    if(row->callbackOwnerIndexPlusOne && (!owner || !owner->foreignPolicy || owner->kind==XL6_IR_TYPE_NULLABLE || !root ||
        target->parameterPhysicalAbis[source]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW))return 0;
    for(size_t p=0;p<signature->paramCount;++p) {
        const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
        if(signature->parameterModes[p]!=XL6_IR_PARAMETER_MODE_DEFAULT || !item)return 0;
        const XL6_ForeignCallbackSpanContract* span=xl6IrForeignCallbackSpanGet(module,row,p);
        if(span && (item->kind!=XL6_IR_TYPE_CLASS || item->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLBACK ||
            !item->foreignHeader || !item->cName[0]))return 0;
        if(item->foreignPolicy || item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) {
            if(signature->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_BORROW ||
                ((item->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLBACK || item->foreignOwner) &&
                 xl6IrForeignRootType(module,signature->params[p])!=root))return 0;
        } else if(!xl6IrRawCCallbackTrivialValueType(item) || item->cAbi==XL6_IR_C_ABI_TYPE_ID ||
            signature->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_VALUE)return 0;
    }
    for(size_t i=0;i<module->foreignCallbackSpanCount;++i){
        const XL6_ForeignCallbackSpanContract* span=&module->foreignCallbackSpans[i];
        if(span->declaration==row->declaration && span->parameter==row->parameter && span->argument>=signature->paramCount)return 0;
    }
    if(signature->returnCount) {
        const XL6_IrType* result=xl6IrTypeGet(&module->types,signature->callableResultType);
        if(row->callbackResultKind==XL6_FOREIGN_CALLBACK_RESULT_OWNED_TEXT){
            if(!result || result->cAbi!=XL6_IR_C_ABI_MANAGED_STRING ||
                (result->kind!=XL6_IR_TYPE_BUILTIN && result->kind!=XL6_IR_TYPE_NULLABLE) || row->callbackFailureCode)return 0;
        }else if(!xl6IrRawCCallbackTrivialValueType(result))return 0;
        if(result->cAbi!=XL6_IR_C_ABI_I64 && result->cAbi!=XL6_IR_C_ABI_U64 &&
            !(result->cAbi>=XL6_IR_C_ABI_C_CHAR && result->cAbi<=XL6_IR_C_ABI_C_WCHAR) && row->callbackFailureCode)return 0;
    } else if(row->callbackFailureCode || row->callbackResultKind)return 0;
    return 1;
}

/* One closed witness capability. Source/ABI authentication stays in XLIR;
 * every consumer renders the same fully verified preparation transaction. */
int xl6IrForeignPreparedParameterMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t p)
{
    if(!target || p>=target->parameterCount)return 0;
    switch(target->parameterPhysicalAbis[p]) {
    case XL6_IR_CALL_PARAMETER_PHYSICAL_DIRECT:
        return !target->parameterPhysicalFieldDeclarations[p];
    case XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VALUE_BORROW:
        return xl6IrForeignValueMatches(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW:
        return xl6IrForeignManagedViewParameter(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_FOREIGN_VIEW_BORROW:
    case XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_NATIVE_POINTER_DIRECT:
    case XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_XVALUE_BORROW:
        return xl6IrForeignPointerQualifierMatches(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_TEXT_VIEW_BORROW:
        return xl6IrForeignTextViewParameter(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_BYTES_VIEW_BORROW:
        return xl6IrForeignBytesViewParameter(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_TERMINATED_STRING_BORROW:
        return xl6IrForeignTerminatedStringParameter(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_CONST_C_RECORD_ADDRESS:
        return xl6IrForeignConstRecordParameter(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_CLASS_HANDLE_BORROW:
        return xl6IrCallTargetNativeClassPhysicalField(module,target,p)!=NULL;
    case XL6_IR_CALL_PARAMETER_PHYSICAL_NATIVE_HANDLE_ACCESS_BORROW:
        return xl6IrCallTargetNativeAccessField(module,target,p)!=NULL;
    case XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_SPAN_BORROW:
        return xl6IrForeignSpanParameterMatches(module,target,p);
    case XL6_IR_CALL_PARAMETER_PHYSICAL_SYNC_C_CALLBACK:
    case XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK:
        return xl6IrForeignCallbackParameterMatches(module,target,p);
    default:return 0;
    }
}
