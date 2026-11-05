#ifndef XL6_FOREIGN_PACK_DECL_H
#define XL6_FOREIGN_PACK_DECL_H

static inline int xl6ForeignPackRead(const XL6_AstNode* attr,XL6_ForeignPackContract* output)
{
    int32 physical,first,count;XL6_ForeignPackContract row={0};
    if(!attr || !attr->text || strcmp(attr->text,"c_pack") || attr->childCount!=4 ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignQuotedText(attr->children[1],row.recordCName,sizeof(row.recordCName)) ||
        !xl6ForeignIdentifier(row.recordCName) ||
        !xl6ForeignDecimalInt(attr->children[2],&first) || first<0 || first>=64 ||
        !xl6ForeignDecimalInt(attr->children[3],&count) || count<=0 || count>64 || first>64-count)return 0;
    row.physicalIndex=(uint32)physical;row.firstParameter=(uint32)first;row.parameterCount=(uint32)count;
    if(output)*output=row;
    return 1;
}
static inline int xl6ForeignPackFieldRead(const XL6_AstNode* attr,XL6_ForeignPackFieldContract* output)
{
    int32 physical,parameter;XL6_ForeignPackFieldContract row={0};char mode[24],count[32];
    if(!attr || !attr->text)return 0;
    int size=!strcmp(attr->text,"c_pack_size");
    if((!size && strcmp(attr->text,"c_pack_field")) ||
        (size ? attr->childCount!=3 : (attr->childCount!=4 && attr->childCount!=5)) ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignQuotedText(attr->children[1],row.field,sizeof(row.field)) || !xl6ForeignIdentifier(row.field))return 0;
    row.packPhysicalIndex=(uint32)physical;
    if(size){
        if(!xl6ForeignQuotedText(attr->children[2],row.physicalCName,sizeof(row.physicalCName)) ||
            (strcmp(row.physicalCName,"uint8_t") && strcmp(row.physicalCName,"uint16_t") &&
             strcmp(row.physicalCName,"uint32_t") && strcmp(row.physicalCName,"uint64_t") &&
             strcmp(row.physicalCName,"size_t")))return 0;
        row.parameter=UINT32_MAX;row.kind=XL6_FOREIGN_PACK_FIELD_SIZEOF;
    }else{
        if(!xl6ForeignDecimalInt(attr->children[2],&parameter) || parameter<0 || parameter>=64 ||
            !xl6ForeignQuotedText(attr->children[3],mode,sizeof(mode)))return 0;
        row.parameter=(uint32)parameter;
        row.kind=!strcmp(mode,"value") ? XL6_FOREIGN_PACK_FIELD_VALUE :
            !strcmp(mode,"view_raw") ? XL6_FOREIGN_PACK_FIELD_VIEW_RAW :
            !strcmp(mode,"text_data") ? XL6_FOREIGN_PACK_FIELD_TEXT_DATA :
            !strcmp(mode,"text_size") ? XL6_FOREIGN_PACK_FIELD_TEXT_SIZE :
            !strcmp(mode,"span_data") ? XL6_FOREIGN_PACK_FIELD_SPAN_DATA :
            !strcmp(mode,"span_count") ? XL6_FOREIGN_PACK_FIELD_SPAN_COUNT :
            !strcmp(mode,"c_string") ? XL6_FOREIGN_PACK_FIELD_C_STRING : XL6_FOREIGN_PACK_FIELD_INVALID;
        if(!row.kind)return 0;
        if(row.kind==XL6_FOREIGN_PACK_FIELD_TEXT_SIZE){
            if(attr->childCount!=5 || !xl6ForeignQuotedText(attr->children[4],count,sizeof(count)))return 0;
            row.countAbi=!strcmp(count,"int") ? XL6_FOREIGN_SPAN_COUNT_INT :
                !strcmp(count,"size_t") ? XL6_FOREIGN_SPAN_COUNT_SIZE:0;
            if(!row.countAbi)return 0;
        }else if(attr->childCount!=4)return 0;
    }
    if(output)*output=row;
    return 1;
}

/* Syntactic grouping is shared by semantics and AST-to-IR normalization.
 * Call emission uses the separately closed IR rows instead of these nodes. */
static inline int xl6ForeignPackForParameter(const XL6_AstNode* node,size_t parameter,XL6_ForeignPackContract* output)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t found=0;XL6_ForeignPackContract match={0};
    for(size_t i=0;attrs && i<attrs->childCount;++i){XL6_ForeignPackContract row;
        if(xl6ForeignPackRead(attrs->children[i],&row) && parameter>=row.firstParameter && parameter-row.firstParameter<row.parameterCount){match=row;++found;}}
    if(found!=1)return 0;
    if(output)*output=match;
    return 1;
}

static inline int xl6ForeignPhysicalOutputSlots(const XL6_AstNode* node,const XL6_ForeignCallFacts* facts,
    uint32* slots,size_t* outputCount)
{
    *outputCount=0;
    if(!facts->outputIndexPlusOne)return 1;
    slots[0]=facts->outputIndexPlusOne-1;*outputCount=1;
    XL6_ForeignValueOutputKind kind=xl6ForeignValueOutputBase(facts->valueOutput);
    if(kind==XL6_FOREIGN_VALUE_OUTPUT_NATIVE_SEQUENCE_STATUS){
        const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t found=0;XL6_ForeignNativeSequenceContract row={0};
        for(size_t i=0;attrs && i<attrs->childCount;++i)found+=xl6ForeignNativeSequenceRead(attrs->children[i],&row);
        if(found!=1 || row.dataIndex>=row.countIndex)return 0;
        slots[0]=row.dataIndex;slots[1]=row.countIndex;*outputCount=2;return 1;
    }
    if(kind!=XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS && kind!=XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID)return 1;
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t found=0;
    for(size_t i=0;attrs && i<attrs->childCount;++i)
        found+=xl6ForeignMultiOutputRead(attrs->children[i],slots,outputCount);
    return found==1;
}
static inline size_t xl6ForeignPhysicalStepCount(const XL6_AstNode* node,size_t parameters,size_t outputs)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);size_t count=parameters+outputs;
    for(size_t i=0;attrs && i<attrs->childCount;++i){XL6_ForeignPackContract pack;
        if(xl6ForeignPackRead(attrs->children[i],&pack)){
            if(pack.parameterCount-1>count)return SIZE_MAX;
            count-=pack.parameterCount-1;
        }XL6_ForeignArrayGroupContract group;
        if(xl6ForeignArrayGroupRead(attrs->children[i],&group)){
            if(group.arrayCount>count)return SIZE_MAX;
            count-=group.arrayCount;
        }}
    return count;
}
static inline int xl6ForeignPhysicalStepRead(const XL6_AstNode* node,const XL6_ForeignCallFacts* facts,
    size_t parameters,size_t receiver,const uint32* slots,size_t outputs,size_t wanted,XL6_ForeignPhysicalStep* output)
{
    size_t parameter=0,ordinal=0,physical=0;
    if(wanted>=xl6ForeignPhysicalStepCount(node,parameters,outputs))return 0;
    for(size_t event=0;event<=wanted;++event){
        XL6_ForeignPhysicalStep step={0};step.physicalIndex=(uint32)physical;
        if(ordinal<outputs && slots[ordinal]==physical){
            step.kind=XL6_FOREIGN_PHYSICAL_STEP_OUTPUT;step.ordinal=(uint32)ordinal;step.parameter=UINT32_MAX;
            ++ordinal;++physical;
        }else{
            if(parameter>=parameters || (ordinal<outputs && slots[ordinal]<physical))return 0;
            XL6_ForeignPackContract pack;size_t explicitParameter=parameter>=receiver ? parameter-receiver:SIZE_MAX;
            step.parameter=(uint32)parameter;
            XL6_ForeignArrayGroupContract group;
            if(explicitParameter<64 && xl6ForeignArrayForParameter(node,explicitParameter,&group)){
                size_t width=xl6ForeignArrayNativeWidth(node,&group);
                if(explicitParameter!=group.firstParameter || group.physicalIndex!=physical ||
                    group.arrayCount>=parameters-parameter || width==SIZE_MAX ||
                    (ordinal<outputs && slots[ordinal]>physical && slots[ordinal]-physical<width))return 0;
                step.kind=XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP;parameter+=(size_t)group.arrayCount+1;physical+=width;
            }else if(explicitParameter<64 && xl6ForeignPackForParameter(node,explicitParameter,&pack)){
                if(explicitParameter!=pack.firstParameter || pack.physicalIndex!=physical || pack.parameterCount>parameters-parameter)return 0;
                step.kind=XL6_FOREIGN_PHYSICAL_STEP_PACK;parameter+=pack.parameterCount;++physical;
            }else{
                step.kind=XL6_FOREIGN_PHYSICAL_STEP_INPUT;
                physical+=explicitParameter<64 && (facts->physicalPairs&(UINT64_C(1)<<explicitParameter)) ? 2:
                    explicitParameter<64 && (facts->retainedCallbacks&(UINT64_C(1)<<explicitParameter)) ? 3:1;
                ++parameter;
            }
        }
        if(event==wanted){if(output)*output=step;return 1;}
    }
    return 0;
}
static inline size_t xl6ForeignOutputLogicalForDeclaration(const XL6_AstNode* node,
    const XL6_ForeignCallFacts* facts,size_t parameters,size_t receiver)
{
    uint32 slots[64];size_t outputs;
    if(!xl6ForeignPhysicalOutputSlots(node,facts,slots,&outputs) || !outputs)return SIZE_MAX;
    size_t steps=xl6ForeignPhysicalStepCount(node,parameters,outputs);
    if(steps>parameters+outputs)return SIZE_MAX;
    for(size_t i=0;i<steps;++i){XL6_ForeignPhysicalStep step;
        if(!xl6ForeignPhysicalStepRead(node,facts,parameters,receiver,slots,outputs,i,&step))return SIZE_MAX;
        if(step.kind==XL6_FOREIGN_PHYSICAL_STEP_OUTPUT){
            size_t before=0;
            for(size_t old=0;old<i;++old){XL6_ForeignPhysicalStep previous;
                if(!xl6ForeignPhysicalStepRead(node,facts,parameters,receiver,slots,outputs,old,&previous))return SIZE_MAX;
                if(previous.kind==XL6_FOREIGN_PHYSICAL_STEP_INPUT)++before;
                else if(previous.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK){XL6_ForeignPackContract pack;
                    if(previous.parameter<receiver || !xl6ForeignPackForParameter(node,previous.parameter-receiver,&pack))return SIZE_MAX;
                    before+=pack.parameterCount;
                }else if(previous.kind==XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP){XL6_ForeignArrayGroupContract group;
                    if(previous.parameter<receiver || !xl6ForeignArrayForParameter(node,previous.parameter-receiver,&group))return SIZE_MAX;
                    before+=(size_t)group.arrayCount+1;
                }
            }
            return before;
        }
    }
    return SIZE_MAX;
}
#endif
