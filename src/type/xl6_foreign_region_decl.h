/* Included after the shared declaration parser vocabulary/prototypes. */
static inline int xl6ForeignRegionSelectorRead(const XL6_AstNode* value,XL6_ForeignRegionSelector* output)
{
    char text[96];XL6_ForeignRegionSelector result={0};
    if(!xl6ForeignQuotedText(value,text,sizeof(text)))return 0;
    if(!strncmp(text,"observer.",9)){
        result.kind=XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD;
        if(!xl6ForeignIdentifier(text+9) || strlen(text+9)>=sizeof(result.field))return 0;
        memcpy(result.field,text+9,strlen(text+9)+1);
    }else{
        const char* p=text;uint32 index=0;
        if(*p<'0' || *p>'9')return 0;
        while(*p>='0' && *p<='9'){index=index*10u+(uint32)(*p++-'0');if(index>=64)return 0;}
        result.physicalIndex=index;
        if(!*p)result.kind=XL6_FOREIGN_REGION_SELECTOR_INPUT;
        else if(*p=='.' && xl6ForeignIdentifier(p+1) && strlen(p+1)<sizeof(result.field)){
            result.kind=XL6_FOREIGN_REGION_SELECTOR_INPUT_FIELD;memcpy(result.field,p+1,strlen(p+1)+1);
        }else return 0;
    }
    if(output)*output=result;
    return 1;
}
static inline int xl6ForeignRegionRead(const XL6_AstNode* attr,XL6_ForeignRegionContract* output)
{
    XL6_ForeignRegionContract row={0};int32 physical,unit;char access[16],nullable[16];
    if(!attr || !attr->text || strcmp(attr->text,"c_region") || (attr->childCount!=6 && attr->childCount!=7) ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !xl6ForeignQuotedText(attr->children[1],access,sizeof(access)) ||
        !xl6ForeignRegionSelectorRead(attr->children[2],&row.width) ||
        !xl6ForeignRegionSelectorRead(attr->children[3],&row.height) ||
        !xl6ForeignRegionSelectorRead(attr->children[4],&row.stride) ||
        !xl6ForeignDecimalInt(attr->children[5],&unit) || unit<=0)return 0;
    row.access=!strcmp(access,"read") ? XL6_FOREIGN_REGION_ACCESS_READ:
        !strcmp(access,"write") ? XL6_FOREIGN_REGION_ACCESS_WRITE:XL6_FOREIGN_REGION_ACCESS_INVALID;
    if(!row.access)return 0;
    if(attr->childCount==7){
        if(row.access!=XL6_FOREIGN_REGION_ACCESS_READ || !xl6ForeignQuotedText(attr->children[6],nullable,sizeof(nullable)) || strcmp(nullable,"nullable"))return 0;
        row.nullable=1;
    }
    row.physicalIndex=(uint32)physical;row.unitBytes=(uint32)unit;if(output)*output=row;return 1;
}
static inline int xl6ForeignRegionObserverRead(const XL6_AstNode* attr,XL6_ForeignRegionObserverContract* output)
{
    XL6_ForeignRegionObserverContract row={0};int32 physical;
    if(!attr || !attr->text || strcmp(attr->text,"c_region_observer") || attr->childCount<2 ||
        attr->childCount>2+XL6_FOREIGN_REGION_OBSERVER_MAX_INPUTS ||
        !xl6ForeignDecimalInt(attr->children[0],&physical) || physical<0 || physical>=64 ||
        !attr->children[1] || attr->children[1]->kind!=XL6_AST_NAME || !xl6ForeignIdentifier(attr->children[1]->text))return 0;
    row.regionPhysicalIndex=(uint32)physical;row.querySymbol=attr->children[1]->semanticSymbolId;
    row.inputCount=(uint8)(attr->childCount-2);
    for(size_t i=0;i<row.inputCount;++i){
        int32 input;if(!xl6ForeignDecimalInt(attr->children[i+2],&input) || input<0 || input>=64)return 0;
        row.physicalInputs[i]=(uint32)input;
    }
    if(output)*output=row;
    return 1;
}
/* Shared physical signature walk. An input selector never denotes an output
 * or the middle of a pointer/count/callback group. */
static inline size_t xl6ForeignRegionPhysicalInput(const XL6_ForeignCallFacts* facts,
    size_t parameters,size_t receiver,const uint32* outputs,size_t outputCount,uint64 spanLimits,uint32 wanted)
{
    size_t physical=0,output=0;
    for(size_t parameter=0;parameter<parameters;++parameter){
        size_t explicitIndex=parameter>=receiver ? parameter-receiver:SIZE_MAX;
        if(explicitIndex<64 && (spanLimits&(UINT64_C(1)<<explicitIndex)))continue;
        while(output<outputCount && outputs[output]==physical){++physical;++output;}
        size_t width=explicitIndex<64 && (facts->physicalPairs&(UINT64_C(1)<<explicitIndex)) ? 2:
            explicitIndex<64 && (facts->retainedCallbacks&(UINT64_C(1)<<explicitIndex)) ? 3:1;
        if(physical==wanted)return width==1 ? parameter:SIZE_MAX;
        if(physical>wanted || wanted-physical<width)return SIZE_MAX;
        physical+=width;
    }
    return SIZE_MAX;
}
