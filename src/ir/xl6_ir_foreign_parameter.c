/* Sparse foreign parameter contracts. Declaration parsing ends before this
 * file; both lowering and Cgen consume these normalized rows. */
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
int xl6IrForeignOutputValueMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target)
{
    const XL6_IrNode* node=target ? xl6IrNodeGet(module,target->declaration) : NULL;
    if(!node || !node->foreignCall.valueOutput)return 0;
    size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || node->sourceKind==XL6_AST_GET || node->sourceKind==XL6_AST_LET) &&
        !(node->flags&XL6_AST_FLAG_STATIC) ? 1u : 0u;
    const XL6_IrType* result=xl6IrTypeGet(&module->types,target->returnType);
    return node->foreignCall.valueOutput==1 && node->foreignCall.outputIndexPlusOne &&
        !node->foreignCall.rollbackSymbol && !node->foreignCall.borrowedViewResult &&
        xl6ForeignOutputLogical(&node->foreignCall,target->parameterCount,receiver)!=SIZE_MAX &&
        (node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) && !(node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) &&
        target->kind==XL6_IR_CALL_TARGET_EXTERN_C && !target->runtimeBridge &&
        !target->nativeServicesRequired && !target->nativeContextAbi && !target->nativeResultAbi && !target->nativeArrayResultAbi &&
        target->variadicAbi==XL6_IR_CALL_VARIADIC_NONE && xl6IrRawCCallbackTrivialValueType(result) &&
        target->resultCAbi==XL6_IR_CALL_RESULT_C_ABI_STORAGE &&
        target->resultPhysicalAbi==XL6_IR_CALL_RESULT_PHYSICAL_DIRECT &&
        target->resultConversionAbi==XL6_IR_CALL_RESULT_CONVERSION_STATUS_OUTPUT_VALUE;
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
        f->spanParameters || f->retainedCallbacks || f->controlViews || f->outputIndexPlusOne ||
        f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_OWNED_NATIVE || f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_BOOL_STATUS || f->borrowedViewResult==XL6_FOREIGN_VIEW_RESULT_INT_STATUS)return 0;
    size_t bytes=strlen(node->text);
    if(bytes<2 || node->text[0]!='"' || node->text[bytes-1]!='"' || strlen(target->cName)!=bytes-2 ||
        memcmp(node->text+1,target->cName,bytes-2))return 0;
    int read=op==XL6_IR_FOREIGN_OPERATION_FIELD_READ || op==XL6_IR_FOREIGN_OPERATION_DEREF_READ;
    const XL6_IrType* value=read ? xl6IrTypeGet(&module->types,target->returnType) :
        target->parameterCount==2 ? xl6IrTypeGet(&module->types,target->parameterTypes[1]) : NULL;
    return value && (f->borrowedViewResult || xl6IrRawCCallbackTrivialValueType(value)) &&
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
const XL6_IrType* xl6IrForeignRetainedSignature(const XL6_IrModule* module,
    const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* contract=xl6IrForeignParameterGet(module,target,parameter);
    if(!contract || contract->kind!=XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK)return NULL;
    const XL6_IrType* type=xl6IrTypeGet(&module->types,target->parameterTypes[parameter]);
    if(type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params)
        type=xl6IrTypeGet(&module->types,type->params[0]);
    return type && type->kind==XL6_IR_TYPE_FUNCTION ? type : NULL;
}
size_t xl6IrForeignParameterWidth(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* contract=xl6IrForeignParameterGet(module,target,parameter);
    return !contract ? 1 : contract->kind==XL6_FOREIGN_PARAMETER_SPAN ? 2 : 3;
}
static int xl6IrForeignParameterRowValid(const XL6_ForeignParameterContract* row)
{
    if(!row || row->parameter>=64 || !memchr(row->header,0,sizeof(row->header)) ||
        (row->header[0] && (row->kind!=XL6_FOREIGN_PARAMETER_SPAN || !xl6ForeignHeaderName(row->header))))return 0;
    if(row->kind==XL6_FOREIGN_PARAMETER_SPAN) {
        if(row->callbackOwnerCName[0] || row->callbackOwnerIndexPlusOne || row->callbackFailureCode || row->callbackUserFirst ||
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
    return row->kind==XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK && !row->pointerCName[0] && !row->elementCName[0] &&
        !row->countAbi && !row->readOnly && row->callbackOwnerIndexPlusOne && row->callbackOwnerIndexPlusOne<=64 &&
        memchr(row->callbackOwnerCName,0,sizeof(row->callbackOwnerCName)) && xl6ForeignIdentifier(row->callbackOwnerCName);
}
static int xl6IrVerifyForeignParameters(const XL6_IrModule* module,XL6_DiagList* diags)
{
    if(module->foreignParameterCount>module->foreignParameterCapacity || (module->foreignParameterCount && !module->foreignParameters))goto invalid;
    for(size_t i=0;i<module->foreignParameterCount;++i) {
        const XL6_ForeignParameterContract* row=&module->foreignParameters[i];
        char expected[80]={0};
        if(row->kind==XL6_FOREIGN_PARAMETER_RETAINED_CALLBACK)
            snprintf(expected,sizeof(expected),"xlir_retained_d%u_p%u",(unsigned)row->declaration,(unsigned)row->parameter);
        if(!memchr(row->callbackTypeCName,0,sizeof(row->callbackTypeCName)) || strcmp(expected,row->callbackTypeCName))goto invalid;
        const XL6_IrNode* declaration=xl6IrNodeGet(module,row->declaration);
        if(!xl6IrForeignParameterRowValid(row) || !declaration || declaration->kind!=XL6_IR_NODE_FUNC_DECL || declaration->childCount<2)goto invalid;
        uint64 bit=UINT64_C(1)<<row->parameter;
        if(!(bit&(row->kind==XL6_FOREIGN_PARAMETER_SPAN ? declaration->foreignCall.spanParameters : declaration->foreignCall.retainedCallbacks)))goto invalid;
        const XL6_IrNode* params=xl6IrNodeGet(module,module->children[declaration->firstChild+1]);
        if(!params || params->kind!=XL6_IR_NODE_LIST || row->parameter>=params->childCount)goto invalid;
        for(size_t j=0;j<i;++j)
            if(module->foreignParameters[j].declaration==row->declaration && module->foreignParameters[j].parameter==row->parameter)goto invalid;
    }
    for(size_t n=0;n<module->nodeCount;++n) {
        const XL6_IrNode* declaration=&module->nodes[n];
        uint64 mask=declaration->foreignCall.spanParameters|declaration->foreignCall.retainedCallbacks;
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
int xl6IrForeignRetainedParameterMatches(const XL6_IrModule* module,const XL6_IrCallTarget* target,size_t parameter)
{
    const XL6_ForeignParameterContract* row=xl6IrForeignParameterGet(module,target,parameter);
    const XL6_IrType* signature=xl6IrForeignRetainedSignature(module,target,parameter);
    if(!row || !signature || !signature->typeValueId || signature->returnCount>1 ||
        signature->callableErasureAbi!=XL6_IR_CALLABLE_ERASURE_FIXED ||
        target->parameterAbis[parameter]!=XL6_IR_PARAMETER_ABI_BORROW || target->parameterPhysicalFieldDeclarations[parameter] ||
        target->parameterConstraintAbis[parameter] || !row->callbackOwnerIndexPlusOne || row->callbackOwnerIndexPlusOne>target->parameterCount)return 0;
    size_t source=row->callbackOwnerIndexPlusOne-1;
    const XL6_IrType* owner=xl6IrTypeGet(&module->types,target->parameterTypes[source]);
    const XL6_IrType* root=xl6IrForeignRootType(module,target->parameterTypes[source]);
    if(!owner || !owner->foreignPolicy || owner->kind==XL6_IR_TYPE_NULLABLE || !root ||
        target->parameterPhysicalAbis[source]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW)return 0;
    for(size_t p=0;p<signature->paramCount;++p) {
        const XL6_IrType* item=xl6IrTypeGet(&module->types,signature->params[p]);
        if(signature->parameterModes[p]!=XL6_IR_PARAMETER_MODE_DEFAULT || !item)return 0;
        if(item->foreignPolicy || item->foreignLifetime==XL6_FOREIGN_LIFETIME_CALLBACK) {
            if(signature->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_BORROW || xl6IrForeignRootType(module,signature->params[p])!=root)return 0;
        } else if(!xl6IrRawCCallbackTrivialValueType(item) || item->cAbi==XL6_IR_C_ABI_TYPE_ID ||
            signature->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_VALUE)return 0;
    }
    if(signature->returnCount) {
        const XL6_IrType* result=xl6IrTypeGet(&module->types,signature->callableResultType);
        if(!xl6IrRawCCallbackTrivialValueType(result))return 0;
        if(result->cAbi!=XL6_IR_C_ABI_I64 && result->cAbi!=XL6_IR_C_ABI_U64 &&
            !(result->cAbi>=XL6_IR_C_ABI_C_CHAR && result->cAbi<=XL6_IR_C_ABI_C_WCHAR) && row->callbackFailureCode)return 0;
    } else if(row->callbackFailureCode)return 0;
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
    case XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_VIEW_BORROW:
        return xl6IrForeignManagedViewParameter(module,target,p);
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
    case XL6_IR_CALL_PARAMETER_PHYSICAL_RETAINED_C_CALLBACK:
        return xl6IrForeignRetainedParameterMatches(module,target,p);
    default:return 0;
    }
}
