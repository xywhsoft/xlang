/* Sparse foreign parameter contracts. Declaration parsing ends before this
 * file; both lowering and Cgen consume these normalized rows. */
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
