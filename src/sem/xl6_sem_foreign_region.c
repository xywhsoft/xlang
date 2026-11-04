static size_t xl6SemForeignRegionOutputs(const XL6_AstNode* attrs,const XL6_ForeignCallFacts* facts,uint32* slots)
{
    if(!facts->outputIndexPlusOne)return 0;
    size_t count=1;slots[0]=facts->outputIndexPlusOne-1;
    XL6_ForeignValueOutputKind kind=xl6ForeignValueOutputBase(facts->valueOutput);
    if(kind==XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS || kind==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID){
        for(size_t i=0;attrs && i<attrs->childCount;++i)
            if(xl6ForeignMultiOutputRead(attrs->children[i],slots,&count))return count;
        return SIZE_MAX;
    }
    return count;
}
static const XL6_AstNode* xl6SemForeignRegionInputType(XL6_SemContext* ctx,const XL6_AstNode* params,
    const XL6_AstNode* owner,size_t receiver,size_t parameter)
{
    (void)owner;
    if(parameter<receiver)return NULL;
    parameter-=receiver;
    return params && parameter<params->childCount && params->children[parameter]->childCount ?
        xl6SemResolveAliasType(ctx,params->children[parameter]->children[0]):NULL;
}
static int xl6SemForeignRegionInteger(XL6_SemContext* ctx,const XL6_AstNode* type)
{
    type=xl6SemResolveAliasType(ctx,type);
    static const char* const names[]={"c_char","c_schar","c_uchar","c_short","c_ushort","c_int","c_uint",
        "c_long","c_ulong","c_longlong","c_ulonglong","c_size","c_ssize","c_ptrdiff","c_intptr","c_uintptr","c_wchar",
        "int","int8","uint8","int16","uint16","int32","uint32","int64","uint64"};
    if(!type || type->childCount || !type->text)return 0;
    for(size_t i=0;i<XL6_ARRAY_COUNT(names);++i)if(!strcmp(type->text,names[i]))return 1;
    return 0;
}
static const XL6_AstNode* xl6SemForeignRegionField(XL6_SemContext* ctx,const XL6_AstNode* type,const char* name)
{
    type=xl6SemResolveAliasType(ctx,type);
    const XL6_AstNode* record=type && type->text && !type->childCount ? xl6SemFindTypeDecl(ctx,type->text):NULL;
    if(xl6ForeignRecordRead(record,NULL,NULL)!=1 || !record->childCount)return NULL;
    const XL6_AstNode* fields=record->children[0];const XL6_AstNode* found=NULL;
    for(size_t i=0;fields && i<fields->childCount;++i){
        const XL6_AstNode* field=fields->children[i];char cName[64];
        if(xl6ForeignRecordFieldRead(field,cName,sizeof(cName),NULL) && !strcmp(cName,name)){
            if(found || !field->childCount)return NULL;
            found=field->children[0];
        }
    }
    return xl6SemForeignRegionInteger(ctx,found) ? found:NULL;
}
static const XL6_AstNode* xl6SemForeignRegionQuery(XL6_SemContext* ctx,const XL6_AstNode* owner,const char* name)
{
    const XL6_AstNode* found=NULL;size_t matches=0;
    const XL6_AstNode* body=owner && owner->childCount>1 ? owner->children[1]:NULL;
    for(size_t i=0;body && i<body->childCount;++i){
        const XL6_AstNode* method=body->children[i];
        if(method && method->kind==XL6_AST_NATIVE_EXTERN_FUNC && !(method->flags&XL6_AST_FLAG_STATIC) &&
            xl6TextEquals(xl6SemMemberName(method),name)){found=method;++matches;}
    }
    if(matches)return matches==1 ? found:NULL;
    for(size_t i=0;ctx->root && i<ctx->root->childCount;++i){
        const XL6_AstNode* node=ctx->root->children[i];
        if(node && node->kind==XL6_AST_EXTERN_FUNC && xl6TextEquals(xl6SemFuncName(node),name)){found=node;++matches;}
    }
    return matches==1 ? found:NULL;
}
static int xl6SemForeignRegions(XL6_SemContext* ctx,XL6_AstNode* node,const XL6_AstNode* owner,
    const XL6_AstNode* attrs,const XL6_ForeignCallFacts* facts)
{
    if(!xl6ForeignInputHasRegions(facts->inputMappingFacts))return 1;
    const XL6_AstNode* params=node->childCount>1 ? node->children[1]:NULL;
    size_t receiver=node->kind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    size_t inputs=params ? params->childCount+receiver:0;uint32 slots[64];
    size_t outputCount=xl6SemForeignRegionOutputs(attrs,facts,slots);
    if(!params || inputs>64 || outputCount==SIZE_MAX || !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE))goto invalid;
    if(node->kind==XL6_AST_EXTERN_FUNC){
        int header=0;for(size_t i=0;attrs && i<attrs->childCount;++i){char declared[256];
            header|=xl6ForeignFunctionHeaderRead(attrs->children[i],declared,sizeof(declared))==1;}
        if(!header)goto invalid;
    }
    for(size_t a=0;attrs && a<attrs->childCount;++a){
        const XL6_AstNode* attr=attrs->children[a];XL6_ForeignRegionContract row;
        if(!xl6ForeignRegionRead(attr,&row))continue;
        size_t parameter=xl6ForeignRegionPhysicalInput(facts,inputs,receiver,slots,outputCount,row.physicalIndex);
        if(parameter==SIZE_MAX || parameter<receiver)goto invalid;
        uint64 explicitBit=UINT64_C(1)<<(parameter-receiver);
        if((facts->constRecordAddresses|facts->staticCallbackAddresses|facts->textViews|facts->bytesViews|
            facts->terminatedStrings|facts->physicalPairs|facts->retainedCallbacks)&explicitBit ||
            (facts->controlViews&(UINT64_C(1)<<parameter)))goto invalid;
        const XL6_AstNode* data=xl6SemForeignRegionInputType(ctx,params,owner,receiver,parameter);
        int nullable=xl6SemTypeIsNullable(data);
        if(nullable)data=xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(data));
        if(!data || nullable!=row.nullable)goto invalid;
        if(!data->childCount && xl6TextEquals(data->text,"bytes")){
            if(row.access!=XL6_FOREIGN_REGION_ACCESS_READ)goto invalid;
        }else{
            const XL6_AstNode* value=xl6SemClassValueType(ctx,data);
            const XL6_AstNode* declaration=value && value->text ? xl6SemFindTypeDecl(ctx,value->text):NULL;
            XL6_ForeignParameterContract span={0};span.readOnly=row.access==XL6_FOREIGN_REGION_ACCESS_READ;
            if(nullable || !declaration || declaration->kind!=XL6_AST_CLASS || !xl6SemForeignSpanType(ctx,data,&span))goto invalid;
        }
        if(row.access==XL6_FOREIGN_REGION_ACCESS_WRITE && (node->flags&XL6_AST_FLAG_FOREIGN_PURE))goto invalid;
        const XL6_AstNode* observed=NULL;size_t observerCount=0;
        XL6_ForeignRegionObserverContract observer={0};const XL6_AstNode* observerAttr=NULL;
        for(size_t o=0;o<attrs->childCount;++o){
            XL6_ForeignRegionObserverContract candidate;
            if(xl6ForeignRegionObserverRead(attrs->children[o],&candidate) && candidate.regionPhysicalIndex==row.physicalIndex){
                observer=candidate;observerAttr=attrs->children[o];++observerCount;
            }
        }
        if(observerCount>1)goto invalid;
        if(observerCount){
            const XL6_AstNode* query=xl6SemForeignRegionQuery(ctx,owner,observerAttr->children[1]->text);
            XL6_ForeignCallFacts queryFacts={0};
            if(!query || query==node || !query->semanticSymbolId || !xl6ForeignFunctionRead(query,&queryFacts) ||
                !xl6ForeignInputMappingsEmpty(queryFacts.inputMappingFacts) || !queryFacts.valueOutput ||
                (xl6ForeignValueOutputBase(queryFacts.valueOutput)!=XL6_FOREIGN_VALUE_OUTPUT_STATUS &&
                 xl6ForeignValueOutputBase(queryFacts.valueOutput)!=XL6_FOREIGN_VALUE_OUTPUT_VOID) ||
                queryFacts.rollbackSymbol || queryFacts.borrowedViewResult || queryFacts.physicalPairs || queryFacts.retainedCallbacks ||
                queryFacts.controlViews || queryFacts.constRecordAddresses || queryFacts.staticCallbackAddresses || queryFacts.textViews ||
                queryFacts.bytesViews || queryFacts.terminatedStrings || !(query->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
                (query->flags&(XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
                ((node->flags&XL6_AST_FLAG_FOREIGN_PURE) && !(query->flags&XL6_AST_FLAG_FOREIGN_PURE)) ||
                ((node->flags&XL6_AST_FLAG_FOREIGN_NONBLOCKING) && !(query->flags&XL6_AST_FLAG_FOREIGN_NONBLOCKING)))goto invalid;
            const XL6_AstNode* queryParams=query->childCount>1 ? query->children[1]:NULL;
            const XL6_AstNode* returns=query->childCount>2 ? query->children[2]:NULL;
            size_t queryReceiver=query->kind==XL6_AST_NATIVE_EXTERN_FUNC ? 1:0;
            if(!queryParams || queryParams->childCount+queryReceiver!=observer.inputCount || !returns || returns->childCount!=1 ||
                !xl6SemIsCAbiRecordType(ctx,returns->children[0]))goto invalid;
            observed=returns->children[0];
            for(size_t p=0;p<observer.inputCount;++p){
                size_t source=xl6ForeignRegionPhysicalInput(facts,inputs,receiver,slots,outputCount,observer.physicalInputs[p]);
                if(source==SIZE_MAX || source==parameter || (p<queryReceiver && source>=receiver))goto invalid;
                if(p<queryReceiver)continue;
                const XL6_AstNode* actual=xl6SemForeignRegionInputType(ctx,params,owner,receiver,source);
                const XL6_AstNode* expected=xl6SemForeignRegionInputType(ctx,queryParams,owner,queryReceiver,p);
                if(source<receiver){
                    if(!expected || xl6SemForeignViewDeclaration(ctx,expected,NULL)!=owner)goto invalid;
                    continue;
                }
                if(!actual || !expected || !xl6SemTypeEquals(ctx,actual,expected))goto invalid;
            }
            observerAttr->children[1]->semanticSymbolId=query->semanticSymbolId;
        }
        const XL6_ForeignRegionSelector* selectors[]={&row.width,&row.height,&row.stride};size_t observedSelectors=0;
        for(size_t s=0;s<XL6_ARRAY_COUNT(selectors);++s){
            const XL6_ForeignRegionSelector* select=selectors[s];
            if(select->kind==XL6_FOREIGN_REGION_SELECTOR_OBSERVER_FIELD){
                if(!observed || !xl6SemForeignRegionField(ctx,observed,select->field))goto invalid;
                ++observedSelectors;continue;
            }
            size_t source=xl6ForeignRegionPhysicalInput(facts,inputs,receiver,slots,outputCount,select->physicalIndex);
            if(source==SIZE_MAX || source==parameter)goto invalid;
            const XL6_AstNode* type=xl6SemForeignRegionInputType(ctx,params,owner,receiver,source);
            if(select->kind==XL6_FOREIGN_REGION_SELECTOR_INPUT ? !xl6SemForeignRegionInteger(ctx,type):
                !xl6SemForeignRegionField(ctx,type,select->field))goto invalid;
        }
        if((observedSelectors!=0)!=(observerCount!=0))goto invalid;
    }
    return 1;
invalid:xl6SemErrorNode(ctx,node,"fixed C regions require exact bytes/Buffer inputs, complete typed scalar/record selectors and a nonrecursive checked native record observer");return 0;
}
static int xl6SemForeignRegionParameter(const XL6_AstNode* node,size_t wanted)
{
    XL6_ForeignCallFacts facts={0};const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);
    const XL6_AstNode* params=node && node->childCount>1 ? node->children[1]:NULL;
    if(!params || !xl6ForeignFunctionRead(node,&facts) || !xl6ForeignInputHasRegions(facts.inputMappingFacts))return 0;
    size_t receiver=node->kind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    uint32 slots[64];size_t outputs=xl6SemForeignRegionOutputs(attrs,&facts,slots);
    if(outputs==SIZE_MAX)return 0;
    for(size_t i=0;attrs && i<attrs->childCount;++i){
        XL6_ForeignRegionContract row;
        if(xl6ForeignRegionRead(attrs->children[i],&row) &&
            xl6ForeignRegionPhysicalInput(&facts,params->childCount+receiver,receiver,slots,outputs,row.physicalIndex)==wanted+receiver)return 1;
    }
    return 0;
}
