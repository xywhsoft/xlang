/* Synchronous input projections can have different logical arities while
 * naming one C entry. Results, effects and non-input contracts stay exact. */
static int xl6SemProjectionInputAttr(const XL6_AstNode* attr)
{
    static const char* const names[]={"c_pack","c_pack_field","c_pack_record","c_pack_size",
        "c_span","c_array_group","c_array_input","c_array_record","c_array_field"};
    for(size_t i=0;attr && attr->text && i<sizeof(names)/sizeof(*names);++i)
        if(!strcmp(attr->text,names[i]))return 1;
    return 0;
}
static int xl6SemProjectionOtherAttrs(const XL6_AstNode* a,const XL6_AstNode* b)
{
    a=xl6ForeignFunctionAttributes(a);b=xl6ForeignFunctionAttributes(b);
    size_t left=0,right=0;
    for(size_t i=0;a && i<a->childCount;++i){
        if(xl6SemProjectionInputAttr(a->children[i]))continue;
        ++left;size_t matches=0;
        for(size_t j=0;b && j<b->childCount;++j)
            if(!xl6SemProjectionInputAttr(b->children[j]))matches+=xl6SemForeignAttributeEqual(a->children[i],b->children[j]);
        if(matches!=1)return 0;
    }
    for(size_t j=0;b && j<b->childCount;++j)right+=!xl6SemProjectionInputAttr(b->children[j]);
    return left==right;
}
static int xl6SemProjectionReadonlySpan(const XL6_SemNullPhysical* map,const XL6_ForeignPhysicalStep* step,
    XL6_ForeignParameterContract* span)
{
    return step->kind==XL6_FOREIGN_PHYSICAL_STEP_INPUT && step->parameter>=map->receiver &&
        xl6SemExternParameterExpansion(map->node,step->parameter-map->receiver,span) &&
        span->kind==XL6_FOREIGN_PARAMETER_SPAN && span->readOnly;
}
static int xl6SemProjectionArraySpan(XL6_SemContext* ctx,const XL6_SemNullPhysical* map,
    const XL6_ForeignPhysicalStep* step,const XL6_ForeignParameterContract* span)
{
    XL6_ForeignArrayGroupContract group;
    if(step->kind!=XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP || step->parameter<map->receiver ||
        !xl6ForeignArrayForParameter(map->node,step->parameter-map->receiver,&group) ||
        group.arrayCount!=1 || group.countAbi!=span->countAbi || group.physicalIndex!=step->physicalIndex ||
        group.sizeParameter>=map->params->childCount)return 0;
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(map->node);size_t inputs=0;
    for(size_t i=0;attrs && i<attrs->childCount;++i){
        uint32 physical;char record[64];XL6_ForeignArrayInputContract input;
        if(xl6ForeignArrayRecordRead(attrs->children[i],&physical,record,sizeof(record)) && physical==group.physicalIndex)return 0;
        if(xl6ForeignArrayInputRead(attrs->children[i],&input) && input.groupPhysicalIndex==group.physicalIndex){
            if(input.column || input.kind!=XL6_FOREIGN_ARRAY_ELEMENT_VALUE || strcmp(input.pointerCName,span->pointerCName))return 0;
            ++inputs;
        }
    }
    const XL6_AstNode* size=map->params->children[group.sizeParameter];
    const XL6_AstNode* type=size && size->childCount ? xl6SemResolveAliasType(ctx,size->children[0]):NULL;
    return inputs==1 && type && !type->childCount && xl6TextEquals(type->text,"int") && !xl6SemParamMode(size);
}
static int xl6SemForeignProjectionCompatible(XL6_SemContext* ctx,const XL6_AstNode* left,
    const XL6_AstNode* right,const XL6_AstNode* owner)
{
    XL6_SemNullPhysical a,b;
    if(!xl6SemNullPhysicalRead(left,&a) || !xl6SemNullPhysicalRead(right,&b))return 0;
    if(xl6SemForeignAttributesEqual(left,right))return -1;
    if(!xl6ForeignInputHasPacks(a.facts.inputMappingFacts) && !xl6ForeignInputHasPacks(b.facts.inputMappingFacts) &&
        !xl6ForeignInputHasArrayGroups(a.facts.inputMappingFacts) && !xl6ForeignInputHasArrayGroups(b.facts.inputMappingFacts))return -1;
    const uint64 flags=XL6_AST_FLAG_STATIC|XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_NO_RAISE|
        XL6_AST_FLAG_FOREIGN_NONBLOCKING|XL6_AST_FLAG_FOREIGN_OWNED_STRING;
    char ah[256],bh[256];
    if(left->kind!=right->kind || (left->kind!=XL6_AST_EXTERN_FUNC && left->kind!=XL6_AST_NATIVE_EXTERN_FUNC) ||
        ((left->flags^right->flags)&flags) || !(left->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
        ((left->flags|right->flags)&XL6_AST_FLAG_FOREIGN_FIELD) || a.receiver!=b.receiver || a.steps!=b.steps ||
        !xl6SemNullHeader(left,owner,ah,sizeof(ah)) || !xl6SemNullHeader(right,owner,bh,sizeof(bh)) || strcmp(ah,bh) ||
        !xl6SemExternCReturnsCompatible(ctx,xl6SemExternFuncReturns(left),xl6SemExternFuncReturns(right)) ||
        !xl6SemProjectionOtherAttrs(left,right))return 0;
    XL6_ForeignCallFacts af=a.facts,bf=b.facts;
    if(af.staticCallbackAddresses || bf.staticCallbackAddresses || af.retainedCallbacks || bf.retainedCallbacks ||
        af.controlViews || bf.controlViews || af.resultOwnerIndexPlusOne || bf.resultOwnerIndexPlusOne ||
        xl6ForeignInputCallbackCount(af.inputMappingFacts) || xl6ForeignInputCallbackCount(bf.inputMappingFacts) ||
        xl6ForeignInputHasRegions(af.inputMappingFacts) || xl6ForeignInputHasRegions(bf.inputMappingFacts) ||
        xl6ForeignInputHasFixedInputs(af.inputMappingFacts) || xl6ForeignInputHasFixedInputs(bf.inputMappingFacts))return 0;
    size_t changed=0;
    for(size_t i=0;i<a.steps;++i){
        XL6_ForeignPhysicalStep x,y;
        if(!xl6SemNullStep(&a,i,&x) || !xl6SemNullStep(&b,i,&y) || x.physicalIndex!=y.physicalIndex)return 0;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_PACK && y.kind==x.kind){
            XL6_ForeignPackContract ap,bp;
            if(x.parameter<a.receiver || y.parameter<b.receiver ||
                !xl6ForeignPackForParameter(left,x.parameter-a.receiver,&ap) ||
                !xl6ForeignPackForParameter(right,y.parameter-b.receiver,&bp) || strcmp(ap.recordCName,bp.recordCName))return 0;
            ++changed;continue;
        }
        XL6_ForeignParameterContract xs={0},ys={0};
        int xspan=xl6SemProjectionReadonlySpan(&a,&x,&xs),yspan=xl6SemProjectionReadonlySpan(&b,&y,&ys);
        if(xspan && y.kind==XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP){if(!xl6SemProjectionArraySpan(ctx,&b,&y,&xs))return 0;++changed;continue;}
        if(yspan && x.kind==XL6_FOREIGN_PHYSICAL_STEP_ARRAY_GROUP){if(!xl6SemProjectionArraySpan(ctx,&a,&x,&ys))return 0;++changed;continue;}
        if(x.kind!=y.kind)return 0;
        if(x.kind==XL6_FOREIGN_PHYSICAL_STEP_OUTPUT){if(x.ordinal!=y.ordinal)return 0;continue;}
        if(x.kind!=XL6_FOREIGN_PHYSICAL_STEP_INPUT || !xl6SemNullParamMeta(&a,x.parameter,&b,y.parameter))return 0;
        if(x.parameter<a.receiver || y.parameter<b.receiver){if(x.parameter>=a.receiver || y.parameter>=b.receiver)return 0;continue;}
        const XL6_AstNode* ap=a.params->children[x.parameter-a.receiver];
        const XL6_AstNode* bp=b.params->children[y.parameter-b.receiver];
        if(!ap || !bp || !ap->childCount || !bp->childCount || xl6SemParamMode(ap)!=xl6SemParamMode(bp))return 0;
        if(xspan || yspan){
            if(!xspan || !yspan || strcmp(xs.pointerCName,ys.pointerCName) || xs.countAbi!=ys.countAbi)return 0;
        }else if(!xl6SemExternCTypeCompatible(ctx,ap->children[0],bp->children[0]))return 0;
    }
    if(!changed)return -1;
    af.constAggregateAddresses=bf.constAggregateAddresses=af.textViews=bf.textViews=0;
    af.bytesViews=bf.bytesViews=af.terminatedStrings=bf.terminatedStrings=0;
    af.physicalPairs=bf.physicalPairs=af.inputMappingFacts=bf.inputMappingFacts=0;
    return xl6ForeignCallFactsEqual(&af,&bf);
}
