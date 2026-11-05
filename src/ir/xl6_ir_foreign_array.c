/* Closed native Array columns: no AST or backend element-type guessing. */
static int xl6IrArrayRowsValid(const XL6_IrModule* m)
{
    return m && m->foreignArrayGroupCount<=m->foreignArrayGroupCapacity && (!m->foreignArrayGroupCount || m->foreignArrayGroups) &&
        m->foreignArrayInputCount<=m->foreignArrayInputCapacity && (!m->foreignArrayInputCount || m->foreignArrayInputs) &&
        m->foreignArrayFieldCount<=m->foreignArrayFieldCapacity && (!m->foreignArrayFieldCount || m->foreignArrayFields) &&
        (!m->foreignArrayGroupCount || (m->foreignArrayGroupCount-1)/64<m->nodeCount) &&
        (!m->foreignArrayInputCount || (m->foreignArrayInputCount-1)/64<m->nodeCount) &&
        (!m->foreignArrayFieldCount || (m->foreignArrayFieldCount-1)/64<m->nodeCount);
}
const XL6_ForeignArrayGroupContract* xl6IrForeignArrayGroupGet(const XL6_IrModule* m,const XL6_IrCallTarget* t,size_t physical)
{
    if(!t || !xl6IrArrayRowsValid(m))return NULL;
    const XL6_ForeignArrayGroupContract* found=NULL;
    for(size_t i=0;i<m->foreignArrayGroupCount;++i){const XL6_ForeignArrayGroupContract* row=&m->foreignArrayGroups[i];
        if(row->declaration==t->declaration && row->physicalIndex==physical){if(found)return NULL;found=row;}}
    return found;
}
const XL6_ForeignArrayGroupContract* xl6IrForeignArrayParameter(const XL6_IrModule* m,const XL6_IrCallTarget* t,size_t p)
{
    if(!t || !xl6IrArrayRowsValid(m))return NULL;
    const XL6_ForeignArrayGroupContract* found=NULL;
    for(size_t i=0;i<m->foreignArrayGroupCount;++i){const XL6_ForeignArrayGroupContract* row=&m->foreignArrayGroups[i];
        if(row->declaration==t->declaration && p>=row->firstParameter && p<=row->sizeParameter){if(found)return NULL;found=row;}}
    return found;
}
static int xl6IrArrayGroupExpected(const XL6_IrModule* m,const XL6_ForeignArrayGroupContract* row,XL6_ForeignArrayGroupContract* expected)
{
    const XL6_IrNode* params;size_t receiver,outputs;uint32 slots[64]={0};
    const XL6_IrNode* node=xl6IrRegionSignature(m,row->declaration,&params,&receiver,slots,&outputs);
    if(!node || !xl6ForeignInputHasArrayGroups(node->foreignCall.inputMappingFacts) ||
        xl6ForeignInputHasPacks(node->foreignCall.inputMappingFacts) || xl6ForeignInputHasRegions(node->foreignCall.inputMappingFacts) ||
        !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
        row->physicalIndex>=64 || row->firstParameter<receiver || row->firstParameter>=64 || !row->arrayCount ||
        row->arrayCount>63 || row->sizeParameter!=row->firstParameter+row->arrayCount || row->sizeParameter>=64 || row->sizeParameter>=params->childCount+receiver ||
        (row->countAbi!=XL6_FOREIGN_SPAN_COUNT_INT && row->countAbi!=XL6_FOREIGN_SPAN_COUNT_SIZE) ||
        !memchr(row->recordCName,0,sizeof(row->recordCName)) || (row->recordCName[0] && !xl6ForeignIdentifier(row->recordCName)))return 0;
    const XL6_IrType* size=xl6IrTypeGet(&m->types,xl6IrRegionInputType(m,node,params,receiver,row->sizeParameter));
    if(!size || size->kind!=XL6_IR_TYPE_BUILTIN || size->cAbi!=XL6_IR_C_ABI_I64 || size->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL)return 0;
    uint64 mask=row->arrayCount==63 ? UINT64_MAX:((UINT64_C(1)<<(row->arrayCount+1))-1)<<(row->firstParameter-receiver);
    if(mask&(node->foreignCall.constAggregateAddresses|node->foreignCall.staticCallbackAddresses|node->foreignCall.textViews|
        node->foreignCall.bytesViews|node->foreignCall.terminatedStrings|node->foreignCall.physicalPairs|node->foreignCall.retainedCallbacks))return 0;
    for(size_t q=0;q<m->foreignPointerQualifierCount;++q)if(m->foreignPointerQualifiers[q].declaration==row->declaration &&
        m->foreignPointerQualifiers[q].parameter>=row->firstParameter && m->foreignPointerQualifiers[q].parameter<=row->sizeParameter)return 0;
    for(size_t q=0;q<m->foreignValueCount;++q)if(m->foreignValues[q].declaration==row->declaration &&
        m->foreignValues[q].parameter>=row->firstParameter && m->foreignValues[q].parameter<=row->sizeParameter)return 0;
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,node->declarationOwnerType);const char* header=owner ? owner->foreignHeader:NULL;
    for(size_t h=0;h<m->foreignFunctionHeaderCount;++h)if(m->foreignFunctionHeaders[h].declaration==row->declaration)header=m->foreignFunctionHeaders[h].header;
    if(!header || !xl6ForeignHeaderName(header) || strlen(header)>=sizeof(row->header))return 0;
    *expected=*row;strcpy(expected->header,header);return 1;
}
static int xl6IrArrayColumnTypes(const XL6_IrModule* m,const XL6_ForeignArrayGroupContract* group,size_t column,
    XL6_ForeignArrayElementKind kind,uint32* arrayType,uint32* elementType,const char** physical)
{
    const XL6_IrNode* params;size_t receiver,outputs;uint32 slots[64]={0};
    const XL6_IrNode* node=xl6IrRegionSignature(m,group->declaration,&params,&receiver,slots,&outputs);
    if(!node || column>=group->arrayCount)return 0;
    size_t p=group->firstParameter+column;
    XL6_IrTypeId id=xl6IrRegionInputType(m,node,params,receiver,p);const XL6_IrType* array=xl6IrTypeGet(&m->types,id);
    if(!array || array->kind!=XL6_IR_TYPE_GENERIC_INSTANCE || array->collectionAbi!=XL6_IR_COLLECTION_ARRAY ||
        array->paramCount!=1 || !array->params || !xl6IrSequenceLayoutComplete(array->sequenceLayout.abi))return 0;
    const XL6_IrType* element=xl6IrTypeGet(&m->types,array->params[0]);
    const XL6_IrType* base=element && element->kind==XL6_IR_TYPE_NULLABLE && element->paramCount==1 && element->params ? xl6IrTypeGet(&m->types,element->params[0]):element;
    const char* name=NULL;
    if(kind==XL6_FOREIGN_ARRAY_ELEMENT_VALUE){
        if(!xl6IrForeignTrivialCValue(m,array->params[0],0))return 0;
        name=element->kind==XL6_IR_TYPE_C_RECORD ? element->cName:xl6IrPackScalarCName(element);
    }else if(kind==XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW){
        if(!element || !base || base->kind!=XL6_IR_TYPE_CLASS || !base->foreignPolicy || !base->foreignHeader ||
            (base->foreignLifetime!=XL6_FOREIGN_LIFETIME_OWNED && base->foreignLifetime!=XL6_FOREIGN_LIFETIME_OWNER) ||
            !xl6IrForeignRootType(m,array->params[0]) || array->sequenceLayout.slotAbi!=XL6_IR_NATIVE_SLOT_FOREIGN_VIEW_V1 ||
            array->sequenceLayout.transferAbi!=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_NATIVE_VALUE_COPY)return 0;
        name=base->cName;
    }else if(kind==XL6_FOREIGN_ARRAY_ELEMENT_C_STRING){
        if(!element || !base || base->kind!=XL6_IR_TYPE_BUILTIN || base->cAbi!=XL6_IR_C_ABI_MANAGED_STRING ||
            element->cAbi!=base->cAbi || element->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED ||
            array->sequenceLayout.transferAbi!=XL6_IR_RUNTIME_RESOURCE_PAYLOAD_STRING_COPY)return 0;
        name="const char*";
    }
    if(!name || !name[0])return 0;
    for(size_t q=0;q<m->foreignPointerQualifierCount;++q)if(m->foreignPointerQualifiers[q].declaration==group->declaration && m->foreignPointerQualifiers[q].parameter==p)return 0;
    for(size_t q=0;q<m->foreignValueCount;++q)if(m->foreignValues[q].declaration==group->declaration && m->foreignValues[q].parameter==p)return 0;
    *arrayType=id;*elementType=array->params[0];if(physical)*physical=name;return 1;
}
static XL6_ForeignArrayProfile xl6IrArrayProfile(const XL6_IrModule* m,const XL6_ForeignArrayGroupContract* group,
    const XL6_ForeignArrayInputContract* row)
{
    if(row->kind==XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW)return XL6_FOREIGN_ARRAY_PROFILE_VIEW_CALL_PIN;
    if(row->kind==XL6_FOREIGN_ARRAY_ELEMENT_C_STRING)return XL6_FOREIGN_ARRAY_PROFILE_STRING_READ_PIN;
    const XL6_IrType* element=xl6IrTypeGet(&m->types,row->elementType);
    const XL6_IrType* array=xl6IrTypeGet(&m->types,row->arrayType);
    if(!element || !array || row->kind!=XL6_FOREIGN_ARRAY_ELEMENT_VALUE)return XL6_FOREIGN_ARRAY_PROFILE_INVALID;
    if(!group->recordCName[0] && element->kind==XL6_IR_TYPE_BUILTIN &&
        array->sequenceLayout.transferAbi==XL6_IR_RUNTIME_RESOURCE_PAYLOAD_TRIVIAL_COPY && !strncmp(row->pointerCName,"const ",6))
        return XL6_FOREIGN_ARRAY_PROFILE_SCALAR_READ_PIN;
    return XL6_FOREIGN_ARRAY_PROFILE_VALUE_SNAPSHOT;
}
int xl6IrForeignArrayColumn(const XL6_IrModule* m,const XL6_ForeignArrayGroupContract* group,size_t column,
    XL6_ForeignArrayInputContract* output,const XL6_ForeignArrayFieldContract** recordField)
{
    if(!group || !xl6IrArrayRowsValid(m) || column>=group->arrayCount)return 0;
    XL6_ForeignArrayInputContract result={0};size_t found=0;const XL6_ForeignArrayFieldContract* field=NULL;
    if(group->recordCName[0]){
        for(size_t i=0;i<m->foreignArrayFieldCount;++i){const XL6_ForeignArrayFieldContract* row=&m->foreignArrayFields[i];
            if(row->declaration==group->declaration && row->groupPhysicalIndex==group->physicalIndex && row->column==column){
                result=(XL6_ForeignArrayInputContract){.declaration=row->declaration,.groupPhysicalIndex=row->groupPhysicalIndex,.column=row->column,
                    .arrayType=row->arrayType,.elementType=row->elementType,.kind=row->kind,.profile=row->profile};
                strcpy(result.elementCName,row->physicalCName);field=row;++found;}}
    }else for(size_t i=0;i<m->foreignArrayInputCount;++i){const XL6_ForeignArrayInputContract* row=&m->foreignArrayInputs[i];
        if(row->declaration==group->declaration && row->groupPhysicalIndex==group->physicalIndex && row->column==column){result=*row;++found;}}
    if(found!=1)return 0;
    if(output)*output=result;
    if(recordField)*recordField=field;
    return 1;
}
static int xl6IrCloseArrayGroups(XL6_IrModule* m)
{
    if(!xl6IrArrayRowsValid(m))return 0;
    for(size_t i=0;i<m->foreignArrayGroupCount;++i){XL6_ForeignArrayGroupContract expected;
        if(!xl6IrArrayGroupExpected(m,&m->foreignArrayGroups[i],&expected))return 0;
        m->foreignArrayGroups[i]=expected;}
    for(size_t i=0;i<m->foreignArrayInputCount;++i){XL6_ForeignArrayInputContract* row=&m->foreignArrayInputs[i];XL6_IrCallTarget t={.declaration=row->declaration};
        const XL6_ForeignArrayGroupContract* group=xl6IrForeignArrayGroupGet(m,&t,row->groupPhysicalIndex);const char* name=NULL;
        if(!group || group->recordCName[0] || !memchr(row->pointerCName,0,sizeof(row->pointerCName)) || !xl6ForeignArrayPointerSpelling(row->pointerCName) ||
            !xl6IrArrayColumnTypes(m,group,row->column,row->kind,&row->arrayType,&row->elementType,&name) || strlen(name)>=sizeof(row->elementCName))return 0;
        strcpy(row->elementCName,name);row->profile=xl6IrArrayProfile(m,group,row);if(!row->profile)return 0;}
    for(size_t i=0;i<m->foreignArrayFieldCount;++i){XL6_ForeignArrayFieldContract* row=&m->foreignArrayFields[i];XL6_IrCallTarget t={.declaration=row->declaration};const char* name=NULL;
        const XL6_ForeignArrayGroupContract* group=xl6IrForeignArrayGroupGet(m,&t,row->groupPhysicalIndex);
        if(!group || !group->recordCName[0] || !memchr(row->field,0,sizeof(row->field)) || !xl6ForeignIdentifier(row->field) ||
            !xl6IrArrayColumnTypes(m,group,row->column,row->kind,&row->arrayType,&row->elementType,&name) || strlen(name)>=sizeof(row->physicalCName))return 0;
        strcpy(row->physicalCName,name);
        XL6_ForeignArrayInputContract column={.arrayType=row->arrayType,.elementType=row->elementType,.kind=row->kind};
        row->profile=xl6IrArrayProfile(m,group,&column);if(!row->profile)return 0;}
    return 1;
}
static int xl6IrVerifyArrayGroups(const XL6_IrModule* m,XL6_DiagList* diags)
{
    if(!xl6IrArrayRowsValid(m))goto invalid;
    for(size_t i=0;i<m->foreignArrayGroupCount;++i){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[i];XL6_ForeignArrayGroupContract expected;
        if(!xl6IrArrayGroupExpected(m,group,&expected) || !memchr(group->header,0,sizeof(group->header)) || strcmp(group->header,expected.header))goto invalid;
        for(size_t j=0;j<i;++j){const XL6_ForeignArrayGroupContract* old=&m->foreignArrayGroups[j];
            if(old->declaration==group->declaration && (old->physicalIndex==group->physicalIndex ||
                (old->firstParameter<=group->sizeParameter && group->firstParameter<=old->sizeParameter)))goto invalid;}
        for(size_t col=0;col<group->arrayCount;++col){XL6_ForeignArrayInputContract input;const XL6_ForeignArrayFieldContract* field;uint32 arrayType,elementType;const char* name;
            if(!xl6IrForeignArrayColumn(m,group,col,&input,&field) || !xl6IrArrayColumnTypes(m,group,col,input.kind,&arrayType,&elementType,&name) ||
                input.arrayType!=arrayType || input.elementType!=elementType || input.profile!=xl6IrArrayProfile(m,group,&input) ||
                !memchr(input.elementCName,0,sizeof(input.elementCName)) || strcmp(input.elementCName,name) ||
                (field && (!memchr(field->physicalCName,0,sizeof(field->physicalCName)) || strcmp(field->physicalCName,name))))goto invalid;}
    }
    for(size_t i=0;i<m->foreignArrayInputCount+m->foreignArrayFieldCount;++i){
        int field=i>=m->foreignArrayInputCount;size_t at=field ? i-m->foreignArrayInputCount:i;
        uint32 decl=field ? m->foreignArrayFields[at].declaration:m->foreignArrayInputs[at].declaration;
        uint32 slot=field ? m->foreignArrayFields[at].groupPhysicalIndex:m->foreignArrayInputs[at].groupPhysicalIndex;
        uint32 col=field ? m->foreignArrayFields[at].column:m->foreignArrayInputs[at].column;XL6_IrCallTarget t={.declaration=decl};
        const XL6_ForeignArrayGroupContract* group=xl6IrForeignArrayGroupGet(m,&t,slot);
        if(!group || field!=(group->recordCName[0]!=0) || col>=group->arrayCount)goto invalid;
        if(!field && (!memchr(m->foreignArrayInputs[at].pointerCName,0,sizeof(m->foreignArrayInputs[at].pointerCName)) || !xl6ForeignArrayPointerSpelling(m->foreignArrayInputs[at].pointerCName)))goto invalid;
        if(field){const XL6_ForeignArrayFieldContract* row=&m->foreignArrayFields[at];
            if(!memchr(row->field,0,sizeof(row->field)) || !xl6ForeignIdentifier(row->field))goto invalid;
            for(size_t j=0;j<at;++j){const XL6_ForeignArrayFieldContract* old=&m->foreignArrayFields[j];
                if(old->declaration==decl && old->groupPhysicalIndex==slot && !strcmp(old->field,row->field))goto invalid;}}
    }
    for(size_t n=0;n<m->nodeCount;++n){size_t groups=0;
        for(size_t g=0;g<m->foreignArrayGroupCount;++g)groups+=m->foreignArrayGroups[g].declaration==n+1;
        if((groups!=0)!=xl6ForeignInputHasArrayGroups(m->nodes[n].foreignCall.inputMappingFacts))goto invalid;
        if(groups){const XL6_IrNode* params;size_t receiver,outputs;uint32 slots[64];
            if(!xl6IrRegionSignature(m,(uint32)n+1,&params,&receiver,slots,&outputs))goto invalid;
            XL6_IrCallTarget t={.declaration=(uint32)n+1,.parameterCount=params->childCount+receiver};size_t count=xl6IrForeignPhysicalStepCount(m,&t);
            if(count>t.parameterCount+outputs)goto invalid;
            for(size_t event=0;event<count;++event)if(!xl6IrForeignPhysicalStepGet(m,&t,event,NULL))goto invalid;}
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native Array projections require closed typed columns, exact count/slots and scoped element storage");
}
int xl6IrForeignArrayGroupsMatch(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(!t || !xl6IrArrayRowsValid(m))return 0;
    const XL6_IrNode* node=xl6IrNodeGet(m,t->declaration);
    size_t found=0;
    for(size_t i=0;i<m->foreignArrayGroupCount;++i){const XL6_ForeignArrayGroupContract* group=&m->foreignArrayGroups[i];if(group->declaration!=t->declaration)continue;++found;
        if(t->kind!=XL6_IR_CALL_TARGET_EXTERN_C || t->runtimeBridge || t->variadicAbi || t->nativeContextAbi || t->nativeServicesRequired ||
            t->nativeResultAbi || t->nativeArrayResultAbi || group->sizeParameter>=t->parameterCount)return 0;
        for(size_t col=0;col<group->arrayCount;++col){XL6_ForeignArrayInputContract input;size_t p=group->firstParameter+col;
            if(!xl6IrForeignArrayColumn(m,group,col,&input,NULL) || input.arrayType!=t->parameterTypes[p] ||
                t->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_BORROW || t->parameterPhysicalAbis[p]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_ARRAY_COLUMN ||
                t->parameterPhysicalFieldDeclarations[p] || t->parameterConstraintAbis[p])return 0;}
        if(t->parameterPhysicalAbis[group->sizeParameter]!=XL6_IR_CALL_PARAMETER_PHYSICAL_FOREIGN_ARRAY_SIZE ||
            t->parameterAbis[group->sizeParameter]!=XL6_IR_PARAMETER_ABI_VALUE || t->parameterPhysicalFieldDeclarations[group->sizeParameter] || t->parameterConstraintAbis[group->sizeParameter])return 0;
    }
    return (found!=0)==(node && xl6ForeignInputHasArrayGroups(node->foreignCall.inputMappingFacts));
}
