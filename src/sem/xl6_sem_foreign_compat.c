static int xl6SemForeignSpanType(XL6_SemContext*,const XL6_AstNode*,const XL6_ForeignParameterContract*);
/* Compare attribute trees as an unordered multiset. Logical names/defaults
 * are deliberately outside the physical native contract. No attributes are
 * silently discarded when admitting a second declaration of one C target. */
static int xl6SemForeignAttributeEqual(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b || a->kind!=b->kind || a->childCount!=b->childCount ||
        ((a->text || b->text) && (!a->text || !b->text || strcmp(a->text,b->text))))return 0;
    for(size_t i=0;i<a->childCount;++i)if(!xl6SemForeignAttributeEqual(a->children[i],b->children[i]))return 0;
    return 1;
}
static int xl6SemForeignAttributesEqual(const XL6_AstNode* a,const XL6_AstNode* b)
{
    a=xl6ForeignFunctionAttributes(a);b=xl6ForeignFunctionAttributes(b);
    if(!a || !b)return a==b;
    if(a->childCount!=b->childCount)return 0;
    for(size_t i=0;i<a->childCount;++i){
        size_t left=0,right=0;
        for(size_t j=0;j<a->childCount;++j)left+=xl6SemForeignAttributeEqual(a->children[i],a->children[j]);
        for(size_t j=0;j<b->childCount;++j)right+=xl6SemForeignAttributeEqual(a->children[i],b->children[j]);
        if(left!=right)return 0;
    }
    return 1;
}
static int xl6SemForeignRegionForParameter(const XL6_AstNode* node,size_t parameter,XL6_ForeignRegionContract* result)
{
    XL6_ForeignCallFacts facts={0};const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);
    const XL6_AstNode* params=xl6SemExternFuncParams(node);uint32 outputs[64];size_t outputCount=0;
    if(!params || !xl6ForeignFunctionRead(node,&facts))return 0;
    size_t receiver=node->kind==XL6_AST_NATIVE_EXTERN_FUNC && !(node->flags&XL6_AST_FLAG_STATIC) ? 1:0;
    if(facts.outputIndexPlusOne){outputs[0]=facts.outputIndexPlusOne-1;outputCount=1;}
    XL6_ForeignValueOutputKind kind=xl6ForeignValueOutputBase(facts.valueOutput);
    if(kind==XL6_FOREIGN_VALUE_OUTPUT_MULTI_STATUS || kind==XL6_FOREIGN_VALUE_OUTPUT_MULTI_VOID){
        outputCount=0;for(size_t i=0;attrs && i<attrs->childCount;++i)
            if(xl6ForeignMultiOutputRead(attrs->children[i],outputs,&outputCount))break;
        if(!outputCount)return 0;
    }
    for(size_t i=0;attrs && i<attrs->childCount;++i){
        XL6_ForeignRegionContract row;
        if(xl6ForeignRegionRead(attrs->children[i],&row) &&
            xl6ForeignRegionPhysicalInput(&facts,params->childCount+receiver,receiver,outputs,outputCount,row.physicalIndex)==parameter+receiver){
            *result=row;return 1;
        }
    }
    return 0;
}
static int xl6SemForeignRegionInputCompatible(XL6_SemContext* ctx,const XL6_AstNode* a,const XL6_AstNode* b,
    size_t parameter,const XL6_AstNode* left,const XL6_AstNode* right)
{
    XL6_ForeignRegionContract x,y;
    if(!xl6SemForeignRegionForParameter(a,parameter,&x) || !xl6SemForeignRegionForParameter(b,parameter,&y) ||
        x.access!=XL6_FOREIGN_REGION_ACCESS_READ || y.access!=x.access || x.nullable!=y.nullable)return 0;
    XL6_ForeignParameterContract span={0};span.readOnly=1;
    left=xl6SemResolveAliasType(ctx,left);right=xl6SemResolveAliasType(ctx,right);
    if(x.nullable){left=xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(left));right=xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(right));}
    int lbytes=left && !left->childCount && xl6TextEquals(left->text,"bytes");
    int rbytes=right && !right->childCount && xl6TextEquals(right->text,"bytes");
    return lbytes!=rbytes && xl6SemForeignSpanType(ctx,left,&span) && xl6SemForeignSpanType(ctx,right,&span);
}
