/* Typed column inputs only. This schema never projects mutable object fields
 * or runs user getters while constructing a native descriptor array. */
static const XL6_AstNode* xl6SemForeignArrayElement(XL6_SemContext* ctx,const XL6_AstNode* type)
{
    type=xl6SemResolveAliasType(ctx,type);
    if(!type || xl6SemTypeIsNullable(type) || !xl6TextEquals(type->text,"array") || type->childCount!=1)return NULL;
    const XL6_AstNode* args=type->children[0];
    return args && args->kind==XL6_AST_LIST && args->childCount==1 ? xl6SemResolveAliasType(ctx,args->children[0]):NULL;
}
static int xl6SemForeignArrayElementValid(XL6_SemContext* ctx,const XL6_AstNode* element,XL6_ForeignArrayElementKind kind)
{
    if(!element)return 0;
    const XL6_AstNode* base=xl6SemTypeIsNullable(element) ? xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(element)):element;
    if(kind==XL6_FOREIGN_ARRAY_ELEMENT_VALUE)return xl6SemForeignTrivialCValue(ctx,element,0);
    if(kind==XL6_FOREIGN_ARRAY_ELEMENT_C_STRING)return base && !base->childCount && xl6TextEquals(base->text,"string");
    XL6_ForeignClassDecl view={0};
    return kind==XL6_FOREIGN_ARRAY_ELEMENT_VIEW_RAW && xl6SemForeignViewDeclaration(ctx,element,&view) &&
        (view.lifetime==XL6_FOREIGN_LIFETIME_OWNED || view.lifetime==XL6_FOREIGN_LIFETIME_OWNER);
}
static int xl6SemForeignArrayGroups(XL6_SemContext* ctx,XL6_AstNode* node,const XL6_AstNode* ownerClass,
    const XL6_AstNode* attrs,const XL6_ForeignCallFacts* facts)
{
    if(!xl6ForeignInputHasArrayGroups(facts->inputMappingFacts))return 1;
    const XL6_AstNode* params=xl6SemExternFuncParams(node);size_t count=params ? params->childCount:0;
    XL6_ForeignClassDecl owner={0};char header[256]={0};uint64 consumed=0;size_t groups=0,inputs=0,fields=0;
    if(ownerClass && xl6ForeignClassRead(ownerClass,&owner,NULL)==1)strcpy(header,owner.header);
    for(size_t a=0;attrs && a<attrs->childCount;++a)if(xl6ForeignFunctionHeaderRead(attrs->children[a],header,sizeof(header)))break;
    if(!header[0] || !params || (node->kind!=XL6_AST_EXTERN_FUNC && node->kind!=XL6_AST_NATIVE_EXTERN_FUNC) ||
        !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (node->flags&XL6_AST_FLAG_FOREIGN_FIELD) || facts->receiverDereference ||
        xl6ForeignInputHasPacks(facts->inputMappingFacts) || xl6ForeignInputHasRegions(facts->inputMappingFacts))goto invalid;
    for(size_t a=0;attrs && a<attrs->childCount;++a){XL6_ForeignArrayGroupContract group;
        if(!xl6ForeignArrayGroupRead(attrs->children[a],&group))continue;
        ++groups;if(group.sizeParameter>=count)goto invalid;
        for(size_t p=group.firstParameter;p<=group.sizeParameter;++p){
            if(p>=64 || (consumed&(UINT64_C(1)<<p)) || !params->children[p] ||
                params->children[p]->childCount<1 || params->children[p]->childCount>2)goto invalid;
            consumed|=UINT64_C(1)<<p;
        }
        const XL6_AstNode* size=xl6SemResolveAliasType(ctx,params->children[group.sizeParameter]->children[0]);
        if(!size || size->childCount || (!xl6TextEquals(size->text,"int") && !xl6TextEquals(size->text,"int64")))goto invalid;
        char record[64]={0};size_t records=0;uint64 used=0;
        for(size_t r=0;r<attrs->childCount;++r){uint32 slot;char name[64];
            if(xl6ForeignArrayRecordRead(attrs->children[r],&slot,name,sizeof(name)) && slot==group.physicalIndex){++records;strcpy(record,name);}}
        if(records>1)goto invalid;
        for(size_t column=0;column<group.arrayCount;++column){
            size_t p=group.firstParameter+column;
            const XL6_AstNode* element=xl6SemForeignArrayElement(ctx,params->children[p]->children[0]);size_t matches=0;
            if(!element)goto invalid;
            for(size_t f=0;f<attrs->childCount;++f){
                XL6_ForeignArrayInputContract input;XL6_ForeignArrayFieldContract field;
                if(xl6ForeignArrayInputRead(attrs->children[f],&input) && input.groupPhysicalIndex==group.physicalIndex && input.column==column){
                    if(records || !xl6SemForeignArrayElementValid(ctx,element,input.kind))goto invalid;
                    ++matches;++inputs;
                }
                if(xl6ForeignArrayFieldRead(attrs->children[f],&field) && field.groupPhysicalIndex==group.physicalIndex && field.column==column){
                    if(!records || !xl6SemForeignArrayElementValid(ctx,element,field.kind))goto invalid;
                    for(size_t previous=0;previous<f;++previous){XL6_ForeignArrayFieldContract old;
                        if(xl6ForeignArrayFieldRead(attrs->children[previous],&old) && old.groupPhysicalIndex==field.groupPhysicalIndex && !strcmp(old.field,field.field))goto invalid;}
                    ++matches;++fields;
                }
            }
            if(matches!=1)goto invalid;
            used|=UINT64_C(1)<<column;
        }
        if(used!=((UINT64_C(1)<<group.arrayCount)-1))goto invalid;
    }
    if(!groups || !(inputs+fields))goto invalid;
    for(size_t a=0;attrs && a<attrs->childCount;++a){
        XL6_ForeignArrayInputContract input;XL6_ForeignArrayFieldContract field;uint32 slot=0,column=0;int has=0;
        if(xl6ForeignArrayInputRead(attrs->children[a],&input)){slot=input.groupPhysicalIndex;column=input.column;has=1;}
        if(xl6ForeignArrayFieldRead(attrs->children[a],&field)){slot=field.groupPhysicalIndex;column=field.column;has=1;}
        if(has){size_t found=0;for(size_t g=0;g<attrs->childCount;++g){XL6_ForeignArrayGroupContract group;
            if(xl6ForeignArrayGroupRead(attrs->children[g],&group) && group.physicalIndex==slot && column<group.arrayCount)++found;}
            if(found!=1)goto invalid;}
        uint32 qualifier;XL6_ForeignValueContract value;
        size_t receiver=node->kind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
        if(xl6ForeignPointerQualifierRead(attrs->children[a],&qualifier) && qualifier>=receiver &&
            qualifier-receiver<64 && (consumed&(UINT64_C(1)<<(qualifier-receiver))))goto invalid;
        if(xl6ForeignValueRead(attrs->children[a],&value) && value.parameter>=receiver && value.parameter-receiver<64 &&
            (consumed&(UINT64_C(1)<<(value.parameter-receiver))))goto invalid;
    }
    if(consumed&(facts->constAggregateAddresses|facts->staticCallbackAddresses|facts->textViews|facts->bytesViews|
        facts->terminatedStrings|facts->physicalPairs|facts->retainedCallbacks))goto invalid;
    {
        size_t receiver=node->kind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
        if(receiver && ((consumed<<receiver)&facts->controlViews))goto invalid;
        uint32 outputs[64];size_t outputCount;if(!xl6ForeignPhysicalOutputSlots(node,facts,outputs,&outputCount))goto invalid;
        size_t steps=xl6ForeignPhysicalStepCount(node,count+receiver,outputCount);
        if(steps>count+receiver+outputCount)goto invalid;
        for(size_t event=0;event<steps;++event)if(!xl6ForeignPhysicalStepRead(node,facts,count+receiver,receiver,outputs,outputCount,event,NULL))goto invalid;
    }
    return 1;
invalid:xl6SemErrorNode(ctx,node,"native array projections require aligned typed columns, one shared bounded count, exact public C types and synchronous input-only storage");return 0;
}
