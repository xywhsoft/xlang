/* Data projection is a raising logical admission, not another C pointer ABI. */
static int xl6SemForeignDataSnapshots(XL6_SemContext* ctx,const XL6_AstNode* node,
    const XL6_AstNode* ownerClass,const XL6_AstNode* attrs,const XL6_ForeignCallFacts* facts)
{
    if(!xl6ForeignHasDataSnapshots(facts->inputMappingFacts))return 1;
    const XL6_AstNode* params=xl6SemExternFuncParams(node);
    if(!params || (node->kind!=XL6_AST_EXTERN_FUNC && node->kind!=XL6_AST_NATIVE_EXTERN_FUNC &&
        !(node->kind==XL6_AST_GET && (node->flags&XL6_AST_FLAG_EXTERN) &&
          !xl6ForeignInputHasDataSnapshots(facts->inputMappingFacts))) ||
        (node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_FIELD)) || facts->receiverDereference)goto invalid;
    char header[256]={0};XL6_ForeignClassDecl owner={0};
    if(ownerClass && xl6ForeignClassRead(ownerClass,&owner,NULL)==1)strcpy(header,owner.header);
    for(size_t i=0;attrs && i<attrs->childCount;++i)
        if(attrs->children[i] && xl6TextEquals(attrs->children[i]->text,"header") &&
            !xl6ForeignFunctionHeaderRead(attrs->children[i],header,sizeof(header)))goto invalid;
    if(!header[0])goto invalid;
    size_t count=0;
    for(size_t i=0;attrs && i<attrs->childCount;++i){uint32 p;
        if(xl6ForeignResultDataSnapshotRead(attrs->children[i],&p)){
            const XL6_AstNode* returns=xl6SemExternFuncReturns(node);
            if(!returns || p>=returns->childCount)goto invalid;
            const XL6_AstNode* type=xl6SemResolveAliasType(ctx,returns->children[p]);
            if(!type || type->childCount || !xl6TextEquals(type->text,"var") ||
                (!facts->valueOutput && facts->borrowedViewResult))goto invalid;
            ++count;continue;
        }
        if(!xl6ForeignDataSnapshotRead(attrs->children[i],&p))continue;
        ++count;
        if(p>=params->childCount || !params->children[p] || !params->children[p]->childCount)goto invalid;
        const XL6_AstNode* type=xl6SemResolveAliasType(ctx,params->children[p]->children[0]);
        if(!type || type->childCount || !xl6TextEquals(type->text,"var"))goto invalid;
        uint64 bit=UINT64_C(1)<<p;
        if(bit&(facts->constAggregateAddresses|facts->staticCallbackAddresses|facts->textViews|
            facts->bytesViews|facts->terminatedStrings|facts->physicalPairs|facts->retainedCallbacks))goto invalid;
    }
    if(count)return 1;
invalid:
    xl6SemErrorNode(ctx,node,"native data snapshots require exact var inputs or owned var results, an authoritative header and a non-pure synchronous function call");
    return 0;
}
