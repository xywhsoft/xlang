/* Physical-only record schemas are closed independently of language records. */
static const XL6_ForeignOutputContract* xl6IrForeignOutputGet(const XL6_IrModule*,const XL6_IrCallTarget*,size_t);
static const char* xl6IrPackScalarCName(const XL6_IrType* type)
{
    if(!type || type->kind!=XL6_IR_TYPE_BUILTIN || type->paramCount || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_TRIVIAL)return NULL;
    switch(type->cAbi){
    case XL6_IR_C_ABI_BOOL:case XL6_IR_C_ABI_C_BOOL:return "bool";
    case XL6_IR_C_ABI_I64:return "int64_t";
    case XL6_IR_C_ABI_U64:return "uint64_t";
    case XL6_IR_C_ABI_C_CHAR:return "char";
    case XL6_IR_C_ABI_C_SCHAR:return "signed char";
    case XL6_IR_C_ABI_C_UCHAR:return "unsigned char";
    case XL6_IR_C_ABI_C_SHORT:return "short";
    case XL6_IR_C_ABI_C_USHORT:return "unsigned short";
    case XL6_IR_C_ABI_C_INT:return "int";
    case XL6_IR_C_ABI_C_UINT:return "unsigned int";
    case XL6_IR_C_ABI_C_LONG:return "long";
    case XL6_IR_C_ABI_C_ULONG:return "unsigned long";
    case XL6_IR_C_ABI_C_LONGLONG:return "long long";
    case XL6_IR_C_ABI_C_ULONGLONG:return "unsigned long long";
    case XL6_IR_C_ABI_C_SIZE:return "size_t";
    case XL6_IR_C_ABI_C_PTRDIFF:return "ptrdiff_t";
    case XL6_IR_C_ABI_C_INTPTR:return "intptr_t";
    case XL6_IR_C_ABI_C_UINTPTR:return "uintptr_t";
    case XL6_IR_C_ABI_F32:case XL6_IR_C_ABI_C_FLOAT:return "float";
    case XL6_IR_C_ABI_F64:case XL6_IR_C_ABI_C_DOUBLE:return "double";
    default:return NULL;
    }
}
static int xl6IrPackArraysValid(const XL6_IrModule* m)
{
    return m && m->foreignPackCount<=m->foreignPackCapacity && (!m->foreignPackCount || m->foreignPacks) &&
        m->foreignPackFieldCount<=m->foreignPackFieldCapacity && (!m->foreignPackFieldCount || m->foreignPackFields) &&
        m->foreignFixedInputCount<=m->foreignFixedInputCapacity && (!m->foreignFixedInputCount || m->foreignFixedInputs) &&
        (!m->foreignFixedInputCount || (m->foreignFixedInputCount-1)/64<m->nodeCount) &&
        (!m->foreignPackCount || (m->foreignPackCount-1)/64<m->nodeCount) &&
        (!m->foreignPackFieldCount || (m->foreignPackFieldCount-1)/256<m->nodeCount);
}
const XL6_ForeignFixedInputContract* xl6IrForeignFixedInputGet(const XL6_IrModule* m,const XL6_IrCallTarget* t,size_t physical)
{
    if(!t || !xl6IrPackArraysValid(m))return NULL;
    const XL6_ForeignFixedInputContract* found=NULL;
    for(size_t i=0;i<m->foreignFixedInputCount;++i)if(m->foreignFixedInputs[i].declaration==t->declaration && m->foreignFixedInputs[i].physicalIndex==physical){
        if(found)return NULL;
        found=&m->foreignFixedInputs[i];
    }
    return found;
}
const XL6_ForeignPackContract* xl6IrForeignPackGet(const XL6_IrModule* m,const XL6_IrCallTarget* t,size_t physical)
{
    if(!t || !xl6IrPackArraysValid(m))return NULL;
    const XL6_ForeignPackContract* found=NULL;
    for(size_t i=0;i<m->foreignPackCount;++i)if(m->foreignPacks[i].declaration==t->declaration && m->foreignPacks[i].physicalIndex==physical){
        if(found)return NULL;
        found=&m->foreignPacks[i];
    }
    return found;
}
const XL6_ForeignPackContract* xl6IrForeignPackParameter(const XL6_IrModule* m,const XL6_IrCallTarget* t,size_t p)
{
    if(!t || !xl6IrPackArraysValid(m))return NULL;
    const XL6_ForeignPackContract* found=NULL;
    for(size_t i=0;i<m->foreignPackCount;++i){const XL6_ForeignPackContract* row=&m->foreignPacks[i];
        if(row->declaration!=t->declaration || p<row->firstParameter || p-row->firstParameter>=row->parameterCount)continue;
        if(found)return NULL;
        found=row;
    }
    return found;
}
size_t xl6IrForeignPhysicalStepCount(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(!t || !xl6IrPackArraysValid(m))return SIZE_MAX;
    const XL6_IrNode* node=xl6IrNodeGet(m,t->declaration);if(!node)return SIZE_MAX;
    size_t outputs=node->foreignCall.valueOutput ? xl6IrForeignPhysicalOutputCount(m,t):(node->foreignCall.outputIndexPlusOne ? 1:0);
    size_t count=t->parameterCount+outputs;
    for(size_t i=0;i<m->foreignFixedInputCount;++i)count+=m->foreignFixedInputs[i].declaration==t->declaration;
    for(size_t i=0;i<m->foreignPackCount;++i){const XL6_ForeignPackContract* row=&m->foreignPacks[i];
        if(row->declaration!=t->declaration)continue;
        if(!row->parameterCount || row->parameterCount-1>count)return SIZE_MAX;
        count-=row->parameterCount-1;
    }
    if(m->foreignArrayGroupCount>m->foreignArrayGroupCapacity || (m->foreignArrayGroupCount && !m->foreignArrayGroups))return SIZE_MAX;
    for(size_t i=0;i<m->foreignArrayGroupCount;++i){const XL6_ForeignArrayGroupContract* row=&m->foreignArrayGroups[i];
        if(row->declaration!=t->declaration)continue;
        if(!row->arrayCount || row->arrayCount>count)return SIZE_MAX;
        count-=row->arrayCount;
    }
    return count;
}
int xl6IrForeignPhysicalStepGet(const XL6_IrModule* m,const XL6_IrCallTarget* t,size_t wanted,XL6_ForeignPhysicalStep* output)
{
    const XL6_IrNode* node=t ? xl6IrNodeGet(m,t->declaration):NULL;
    if(!node || wanted>=xl6IrForeignPhysicalStepCount(m,t))return 0;
    size_t outputs=node->foreignCall.valueOutput ? xl6IrForeignPhysicalOutputCount(m,t):(node->foreignCall.outputIndexPlusOne ? 1:0);
    size_t receiver=(node->sourceKind==XL6_AST_NATIVE_EXTERN_FUNC || node->sourceKind==XL6_AST_GET || node->sourceKind==XL6_AST_LET) &&
        !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    size_t physical=0,p=0,o=0;
    for(size_t event=0;event<=wanted;++event){
        const XL6_ForeignOutputContract* outRow=node->foreignCall.valueOutput ? xl6IrForeignOutputGet(m,t,o):NULL;
        const XL6_ForeignNativeSequenceContract* sequence=xl6IrForeignNativeSequenceGet(m,t);
        size_t slot=sequence ? (o==0 ? sequence->dataIndex:sequence->countIndex):outRow ? outRow->physicalIndex:node->foreignCall.outputIndexPlusOne-1;
        XL6_ForeignPhysicalStep step={0};step.physicalIndex=(uint32)physical;
        if(o<outputs && slot==physical){
            step.kind=XL6_FOREIGN_PHYSICAL_STEP_OUTPUT;step.ordinal=(uint32)o;step.parameter=UINT32_MAX;++o;++physical;
        }else if(xl6IrForeignFixedInputGet(m,t,physical)){
            step.kind=XL6_FOREIGN_PHYSICAL_STEP_FIXED;step.parameter=UINT32_MAX;++physical;
        }else{
            if(p>=t->parameterCount || (o<outputs && slot<physical))return 0;
            const XL6_ForeignPackContract* pack=xl6IrForeignPackParameter(m,t,p);step.parameter=(uint32)p;
            const XL6_ForeignArrayGroupContract* arrays=xl6IrForeignArrayParameter(m,t,p);
            if(arrays){
                size_t width=arrays->recordCName[0] ? 2:arrays->arrayCount+1;
                if(p!=arrays->firstParameter || arrays->physicalIndex!=physical || arrays->sizeParameter>=t->parameterCount ||
                    (o<outputs && slot>physical && slot<physical+width))return 0;
                step.kind=XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP;p+=arrays->arrayCount+1;physical+=width;
            }else if(pack){
                if(p!=pack->firstParameter || pack->physicalIndex!=physical || pack->parameterCount>t->parameterCount-p)return 0;
                step.kind=XL6_FOREIGN_PHYSICAL_STEP_PACK;p+=pack->parameterCount;++physical;
            }else{
                step.kind=XL6_FOREIGN_PHYSICAL_STEP_INPUT;size_t explicitParameter=p>=receiver ? p-receiver:SIZE_MAX;
                physical+=explicitParameter<64 && (node->foreignCall.physicalPairs&(UINT64_C(1)<<explicitParameter)) ? 2:
                    explicitParameter<64 && (node->foreignCall.retainedCallbacks&(UINT64_C(1)<<explicitParameter)) ? 3:1;
                ++p;
            }
        }
        if(event==wanted){if(output)*output=step;return 1;}
    }
    return 0;
}
size_t xl6IrForeignFirstOutputLogical(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    size_t count=xl6IrForeignPhysicalStepCount(m,t),before=0;
    if(count>t->parameterCount+64)return SIZE_MAX;
    for(size_t i=0;i<count;++i){XL6_ForeignPhysicalStep step;
        if(!xl6IrForeignPhysicalStepGet(m,t,i,&step))return SIZE_MAX;
        if(step.kind==XL6_FOREIGN_PHYSICAL_STEP_OUTPUT)return before;
        if(step.kind==XL6_FOREIGN_PHYSICAL_STEP_INPUT)++before;
        else if(step.kind==XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP){const XL6_ForeignArrayGroupContract* group=xl6IrForeignArrayGroupGet(m,t,step.physicalIndex);if(!group)return SIZE_MAX;before+=group->arrayCount+1;}
        else if(step.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){const XL6_ForeignPackContract* pack=xl6IrForeignPackGet(m,t,step.physicalIndex);if(!pack)return SIZE_MAX;before+=pack->parameterCount;}
    }
    return SIZE_MAX;
}
static int xl6IrPackExpected(const XL6_IrModule* m,const XL6_ForeignPackContract* row,XL6_ForeignPackContract* expected)
{
    const XL6_IrNode* params;size_t receiver,outputCount;uint32 outputs[64]={0};
    const XL6_IrNode* node=xl6IrRegionSignature(m,row->declaration,&params,&receiver,outputs,&outputCount);
    if(!node || !xl6ForeignInputHasPacks(node->foreignCall.inputMappingFacts) || xl6ForeignInputHasRegions(node->foreignCall.inputMappingFacts) ||
        !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
        row->firstParameter<receiver || row->firstParameter>params->childCount+receiver || !row->parameterCount ||
        row->firstParameter>=64 || row->parameterCount>64-row->firstParameter ||
        row->parameterCount>params->childCount+receiver-row->firstParameter || row->physicalIndex>=64 ||
        !memchr(row->recordCName,0,sizeof(row->recordCName)) || !xl6ForeignIdentifier(row->recordCName))return 0;
    const XL6_IrType* owner=xl6IrTypeGet(&m->types,node->declarationOwnerType);const char* header=owner ? owner->foreignHeader:NULL;
    for(size_t i=0;i<m->foreignFunctionHeaderCount;++i)if(m->foreignFunctionHeaders[i].declaration==row->declaration)header=m->foreignFunctionHeaders[i].header;
    if(!header || !xl6ForeignHeaderName(header) || strlen(header)>=sizeof(row->header))return 0;
    *expected=*row;strcpy(expected->header,header);return 1;
}
static const XL6_ForeignParameterContract* xl6IrPackSpan(const XL6_IrModule* m,uint32 declaration,size_t p,size_t receiver)
{
    if(p<receiver || m->foreignParameterCount>m->foreignParameterCapacity || (m->foreignParameterCount && !m->foreignParameters))return NULL;
    const XL6_ForeignParameterContract* found=NULL;
    for(size_t i=0;i<m->foreignParameterCount;++i){const XL6_ForeignParameterContract* row=&m->foreignParameters[i];
        if(row->declaration!=declaration || row->parameter!=p-receiver)continue;
        if(found)return NULL;
        found=row;
    }
    return found && found->kind==XL6_FOREIGN_PARAMETER_SPAN && found->readOnly ? found:NULL;
}
static int xl6IrPackFieldExpected(const XL6_IrModule* m,const XL6_ForeignPackFieldContract* row,XL6_ForeignPackFieldContract* expected)
{
    XL6_IrCallTarget target={0};target.declaration=row->declaration;
    const XL6_ForeignPackContract* pack=xl6IrForeignPackGet(m,&target,row->packPhysicalIndex);
    const XL6_IrNode* params;size_t receiver,outputs;uint32 slots[64]={0};
    const XL6_IrNode* node=xl6IrRegionSignature(m,row->declaration,&params,&receiver,slots,&outputs);
    if(!pack || !node || !memchr(row->field,0,sizeof(row->field)) || !xl6ForeignPackPath(row->field))return 0;
    *expected=*row;
    size_t parentLength=xl6ForeignPackParentLength(row->field),parents=0;uint32 parentIndex=0;
    for(size_t i=0;i<m->foreignPackFieldCount;++i){const XL6_ForeignPackFieldContract* parent=&m->foreignPackFields[i];
        if(parent->declaration==row->declaration && parent->packPhysicalIndex==row->packPhysicalIndex &&
            parent->kind==XL6_FOREIGN_PACK_FIELD_RECORD && memchr(parent->field,0,sizeof(parent->field)) &&
            strlen(parent->field)==parentLength && !strncmp(parent->field,row->field,parentLength)){
            ++parents;parentIndex=(uint32)i+1;
        }
    }
    if(parentLength ? parents!=1:parents!=0)return 0;
    expected->recordParentIndexPlusOne=parentIndex;
    if(row->kind==XL6_FOREIGN_PACK_FIELD_RECORD){
        return row->parameter==UINT32_MAX && !row->logicalType && !row->countAbi &&
            memchr(row->physicalCName,0,sizeof(row->physicalCName)) && xl6ForeignIdentifier(row->physicalCName);
    }
    if(row->kind==XL6_FOREIGN_PACK_FIELD_SIZEOF){
        if(row->parameter!=UINT32_MAX || row->logicalType || row->countAbi || !memchr(row->physicalCName,0,sizeof(row->physicalCName)) ||
            (strcmp(row->physicalCName,"uint8_t") && strcmp(row->physicalCName,"uint16_t") && strcmp(row->physicalCName,"uint32_t") &&
             strcmp(row->physicalCName,"uint64_t") && strcmp(row->physicalCName,"size_t")))return 0;
        return 1;
    }
    size_t p=row->parameter;if(p<pack->firstParameter || p-pack->firstParameter>=pack->parameterCount || p>=params->childCount+receiver)return 0;
    for(size_t i=0;i<m->foreignPointerQualifierCount;++i)
        if(m->foreignPointerQualifiers[i].declaration==row->declaration && m->foreignPointerQualifiers[i].parameter==p)return 0;
    for(size_t i=0;i<m->foreignValueCount;++i)
        if(m->foreignValues[i].declaration==row->declaration && m->foreignValues[i].parameter==p)return 0;
    XL6_IrTypeId typeId=xl6IrRegionInputType(m,node,params,receiver,p);const XL6_IrType* type=xl6IrTypeGet(&m->types,typeId);
    const XL6_IrType* inner=type && type->kind==XL6_IR_TYPE_NULLABLE && type->paramCount==1 && type->params ? xl6IrTypeGet(&m->types,type->params[0]):type;
    const char* cName=NULL;XL6_ForeignSpanCountAbi countAbi=0;
    if(row->kind==XL6_FOREIGN_PACK_FIELD_VALUE){
        if(!xl6IrForeignTrivialCValue(m,typeId,0))return 0;
        cName=type->kind==XL6_IR_TYPE_C_RECORD ? type->cName:xl6IrPackScalarCName(type);
    }
    else if(row->kind==XL6_FOREIGN_PACK_FIELD_VIEW_RAW){
        if(!type || !inner || inner->kind!=XL6_IR_TYPE_CLASS || !inner->foreignHeader ||
            (inner->foreignLifetime!=XL6_FOREIGN_LIFETIME_OWNED && inner->foreignLifetime!=XL6_FOREIGN_LIFETIME_OWNER && inner->foreignLifetime!=XL6_FOREIGN_LIFETIME_CALLBACK) ||
            type->foreignLifetime!=inner->foreignLifetime || type->cAbi!=inner->cAbi || (node->foreignCall.controlViews&(UINT64_C(1)<<p)))return 0;
        cName=inner->cName;
    }else if(row->kind==XL6_FOREIGN_PACK_FIELD_SPAN_DATA || row->kind==XL6_FOREIGN_PACK_FIELD_SPAN_COUNT){
        const XL6_ForeignParameterContract* span=xl6IrPackSpan(m,row->declaration,p,receiver);
        if(!span || !type || type->kind==XL6_IR_TYPE_NULLABLE)return 0;
        cName=row->kind==XL6_FOREIGN_PACK_FIELD_SPAN_DATA ? span->pointerCName:span->countAbi==XL6_FOREIGN_SPAN_COUNT_INT ? "int":"size_t";
        if(row->kind==XL6_FOREIGN_PACK_FIELD_SPAN_COUNT)countAbi=span->countAbi;
    }else{
        if(!type || !inner || inner->kind!=XL6_IR_TYPE_BUILTIN || inner->paramCount || inner->cAbi!=XL6_IR_C_ABI_MANAGED_STRING ||
            type->cAbi!=inner->cAbi || type->ownershipAbi!=XL6_IR_OWNERSHIP_ABI_MANAGED)return 0;
        if(row->kind==XL6_FOREIGN_PACK_FIELD_TEXT_SIZE){
            if(row->countAbi!=XL6_FOREIGN_SPAN_COUNT_INT && row->countAbi!=XL6_FOREIGN_SPAN_COUNT_SIZE)return 0;
            countAbi=row->countAbi;cName=countAbi==XL6_FOREIGN_SPAN_COUNT_INT ? "int":"size_t";
        }else if(row->kind==XL6_FOREIGN_PACK_FIELD_TEXT_DATA || row->kind==XL6_FOREIGN_PACK_FIELD_C_STRING)cName="const char*";
    }
    if(!cName || strlen(cName)>=sizeof(row->physicalCName))return 0;
    uint64 bit=UINT64_C(1)<<(p-receiver);
    if((row->kind==XL6_FOREIGN_PACK_FIELD_TEXT_DATA || row->kind==XL6_FOREIGN_PACK_FIELD_TEXT_SIZE) && !(node->foreignCall.textViews&bit))return 0;
    if(row->kind==XL6_FOREIGN_PACK_FIELD_C_STRING && !(node->foreignCall.terminatedStrings&bit))return 0;
    expected->logicalType=typeId;expected->countAbi=countAbi;strcpy(expected->physicalCName,cName);return 1;
}
static int xl6IrClosePacks(XL6_IrModule* m)
{
    for(size_t i=0;i<m->foreignPackCount;++i){XL6_ForeignPackContract expected;
        if(!xl6IrPackExpected(m,&m->foreignPacks[i],&expected))return 0;
        m->foreignPacks[i]=expected;
    }
    for(size_t i=0;i<m->foreignPackFieldCount;++i){XL6_ForeignPackFieldContract expected;
        if(!xl6IrPackFieldExpected(m,&m->foreignPackFields[i],&expected))return 0;
        m->foreignPackFields[i]=expected;
    }
    return 1;
}
static int xl6IrVerifyPacks(const XL6_IrModule* m,XL6_DiagList* diags)
{
    if(!xl6IrPackArraysValid(m))goto invalid;
    for(size_t i=0;i<m->foreignFixedInputCount;++i){const XL6_ForeignFixedInputContract* row=&m->foreignFixedInputs[i];
        const XL6_IrNode* params;size_t receiver,outputs;uint32 slots[64]={0};
        const XL6_IrNode* node=xl6IrRegionSignature(m,row->declaration,&params,&receiver,slots,&outputs);
        const XL6_IrType* owner=node ? xl6IrTypeGet(&m->types,node->declarationOwnerType):NULL;
        const char* header=owner ? owner->foreignHeader:NULL;
        for(size_t h=0;h<m->foreignFunctionHeaderCount;++h)if(m->foreignFunctionHeaders[h].declaration==row->declaration)header=m->foreignFunctionHeaders[h].header;
        if(!node || !header || !xl6ForeignHeaderName(header) || !xl6ForeignInputHasFixedInputs(node->foreignCall.inputMappingFacts) ||
            !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (node->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
            xl6ForeignInputHasRegions(node->foreignCall.inputMappingFacts) || !xl6ForeignFixedInputTypeValid(row))goto invalid;
        for(size_t old=0;old<i;++old)if(m->foreignFixedInputs[old].declaration==row->declaration && m->foreignFixedInputs[old].physicalIndex==row->physicalIndex)goto invalid;
    }
    for(size_t i=0;i<m->foreignPackCount;++i){const XL6_ForeignPackContract* pack=&m->foreignPacks[i];XL6_ForeignPackContract expected;
        if(!xl6IrPackExpected(m,pack,&expected) || !memchr(pack->header,0,sizeof(pack->header)) || strcmp(pack->header,expected.header))goto invalid;
        for(size_t j=0;j<i;++j){const XL6_ForeignPackContract* old=&m->foreignPacks[j];if(old->declaration!=pack->declaration)continue;
            if(old->physicalIndex==pack->physicalIndex || (old->firstParameter<pack->firstParameter+pack->parameterCount && pack->firstParameter<old->firstParameter+old->parameterCount))goto invalid;
        }
        uint64 used=0,textData=0,textSize=0,spanData=0,spanCount=0;size_t fields=0;
        for(size_t f=0;f<m->foreignPackFieldCount;++f){const XL6_ForeignPackFieldContract* field=&m->foreignPackFields[f];
            if(field->declaration!=pack->declaration || field->packPhysicalIndex!=pack->physicalIndex)continue;
            ++fields;
            if(field->kind==XL6_FOREIGN_PACK_FIELD_SIZEOF || field->kind==XL6_FOREIGN_PACK_FIELD_RECORD)continue;
            if(field->parameter>=64)goto invalid;
            uint64 bit=UINT64_C(1)<<field->parameter;used|=bit;
            uint64* paired=field->kind==XL6_FOREIGN_PACK_FIELD_TEXT_DATA ? &textData:field->kind==XL6_FOREIGN_PACK_FIELD_TEXT_SIZE ? &textSize:
                field->kind==XL6_FOREIGN_PACK_FIELD_SPAN_DATA ? &spanData:field->kind==XL6_FOREIGN_PACK_FIELD_SPAN_COUNT ? &spanCount:NULL;
            if(paired){if(*paired&bit)goto invalid;*paired|=bit;}
        }
        uint64 mask=pack->parameterCount==64 ? UINT64_MAX:((UINT64_C(1)<<pack->parameterCount)-1)<<pack->firstParameter;
        if(!fields || used!=mask || textData!=textSize || spanData!=spanCount)goto invalid;
    }
    for(size_t i=0;i<m->foreignPackFieldCount;++i){const XL6_ForeignPackFieldContract* row=&m->foreignPackFields[i];XL6_ForeignPackFieldContract expected;
        if(!xl6IrPackFieldExpected(m,row,&expected) || row->logicalType!=expected.logicalType || row->countAbi!=expected.countAbi ||
            row->recordParentIndexPlusOne!=expected.recordParentIndexPlusOne ||
            !memchr(row->physicalCName,0,sizeof(row->physicalCName)) || strcmp(row->physicalCName,expected.physicalCName))goto invalid;
        for(size_t j=0;j<i;++j){const XL6_ForeignPackFieldContract* old=&m->foreignPackFields[j];
            if(old->declaration==row->declaration && old->packPhysicalIndex==row->packPhysicalIndex && !strcmp(old->field,row->field))goto invalid;}
    }
    for(size_t n=0;n<m->nodeCount;++n){size_t packs=0,fields=0;for(size_t i=0;i<m->foreignPackCount;++i)packs+=m->foreignPacks[i].declaration==n+1;
        for(size_t i=0;i<m->foreignPackFieldCount;++i)fields+=m->foreignPackFields[i].declaration==n+1;
        if((packs!=0)!=xl6ForeignInputHasPacks(m->nodes[n].foreignCall.inputMappingFacts) || (packs!=0)!=(fields!=0))goto invalid;
        size_t nulls=0;for(size_t i=0;i<m->foreignFixedInputCount;++i)nulls+=m->foreignFixedInputs[i].declaration==n+1;
        if((nulls!=0)!=xl6ForeignInputHasFixedInputs(m->nodes[n].foreignCall.inputMappingFacts))goto invalid;
        if(!packs && !nulls)continue;
        const XL6_IrNode* params;size_t receiver,outputs;uint32 slots[64]={0};
        const XL6_IrNode* node=xl6IrRegionSignature(m,(uint32)n+1,&params,&receiver,slots,&outputs);if(!node)goto invalid;
        XL6_IrCallTarget target={0};target.declaration=(uint32)n+1;target.parameterCount=params->childCount+receiver;
        size_t count=xl6IrForeignPhysicalStepCount(m,&target);if(count>target.parameterCount+outputs+64)goto invalid;
        for(size_t i=0;i<count;++i)if(!xl6IrForeignPhysicalStepGet(m,&target,i,NULL))goto invalid;
        for(size_t i=0;i<m->foreignFixedInputCount;++i){const XL6_ForeignFixedInputContract* row=&m->foreignFixedInputs[i];if(row->declaration!=n+1)continue;
            if(row->physicalIndex>=64 || (receiver && !row->physicalIndex) || !xl6ForeignFixedInputTypeValid(row))goto invalid;
            size_t found=0;for(size_t s=0;s<count;++s){XL6_ForeignPhysicalStep step;
                if(!xl6IrForeignPhysicalStepGet(m,&target,s,&step))goto invalid;
                found+=step.kind==XL6_FOREIGN_PHYSICAL_STEP_FIXED && step.physicalIndex==row->physicalIndex;
            }
            if(found!=1)goto invalid;
        }
    }
    return 1;
invalid:return xl6IrDiag(diags,NULL,"native descriptor packs require closed disjoint inputs, paired scoped fields and exact physical slots/types");
}
int xl6IrForeignPacksMatch(const XL6_IrModule* m,const XL6_IrCallTarget* t)
{
    if(!t || !xl6IrPackArraysValid(m))return 0;
    const XL6_IrNode* node=xl6IrNodeGet(m,t->declaration);if(!node)return 0;
    size_t found=0;
    for(size_t i=0;i<m->foreignPackCount;++i){const XL6_ForeignPackContract* pack=&m->foreignPacks[i];
        if(pack->declaration!=t->declaration)continue;
        ++found;
        if(t->kind!=XL6_IR_CALL_TARGET_EXTERN_C || t->runtimeBridge || t->variadicAbi || t->nativeContextAbi || t->nativeServicesRequired ||
            t->nativeResultAbi || t->nativeArrayResultAbi || pack->firstParameter>t->parameterCount || pack->parameterCount>t->parameterCount-pack->firstParameter)return 0;
        for(size_t p=pack->firstParameter;p<pack->firstParameter+pack->parameterCount;++p){
            if(t->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_BORROW && t->parameterAbis[p]!=XL6_IR_PARAMETER_ABI_VALUE)return 0;
            for(size_t f=0;f<m->foreignPackFieldCount;++f){const XL6_ForeignPackFieldContract* field=&m->foreignPackFields[f];
                if(field->declaration==t->declaration && field->packPhysicalIndex==pack->physicalIndex && field->parameter==p && field->logicalType!=t->parameterTypes[p])return 0;}
        }
    }
    if((found!=0)!=xl6ForeignInputHasPacks(node->foreignCall.inputMappingFacts))return 0;
    size_t nulls=0;
    for(size_t i=0;i<m->foreignFixedInputCount;++i){const XL6_ForeignFixedInputContract* row=&m->foreignFixedInputs[i];if(row->declaration!=t->declaration)continue;
        ++nulls;
        if(t->kind!=XL6_IR_CALL_TARGET_EXTERN_C || t->runtimeBridge || t->variadicAbi || t->nativeContextAbi || t->nativeServicesRequired ||
            t->nativeResultAbi || t->nativeArrayResultAbi || node->flags&XL6_AST_FLAG_FOREIGN_FIELD ||
            !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE))return 0;
    }
    return (nulls!=0)==xl6ForeignInputHasFixedInputs(node->foreignCall.inputMappingFacts);
}
