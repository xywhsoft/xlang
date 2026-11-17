static int xl6SemForeignSpanType(XL6_SemContext*,const XL6_AstNode*,const XL6_ForeignParameterContract*);
static int xl6SemForeignHasRetainedOutputs(const XL6_AstNode* node)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);
    for(size_t i=0;attrs && i<attrs->childCount;++i){
        XL6_ForeignOutputViewContract output={0};
        if(attrs->children[i] && xl6TextEquals(attrs->children[i]->text,"c_output_retain"))return 1;
        if(xl6ForeignOutputViewRead(attrs->children[i],&output) &&
            output.conversion==XL6_FOREIGN_OUTPUT_CONVERSION_OWNED_VALUE)return 1;
    }
    return 0;
}
/* Compare physical attributes as an unordered multiset. Data snapshots are
 * logical admissions; each declaration keeps its independently checked
 * rows/effects while the native pointer signature stays identical. */
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
    size_t ac=0,bc=0;
    for(size_t i=0;a && i<a->childCount;++i)ac+=!(xl6ForeignDataSnapshotRead(a->children[i],NULL) || xl6ForeignResultDataSnapshotRead(a->children[i],NULL));
    for(size_t i=0;b && i<b->childCount;++i)bc+=!(xl6ForeignDataSnapshotRead(b->children[i],NULL) || xl6ForeignResultDataSnapshotRead(b->children[i],NULL));
    if(ac!=bc)return 0;
    if(!ac)return 1;
    for(size_t i=0;i<a->childCount;++i){
        if(xl6ForeignDataSnapshotRead(a->children[i],NULL) || xl6ForeignResultDataSnapshotRead(a->children[i],NULL))continue;
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
    if(xl6ForeignValueOutputIsMulti(kind)){
        outputCount=0;for(size_t i=0;attrs && i<attrs->childCount;++i)
            if(xl6ForeignMultiOutputRead(attrs->children[i],outputs,&outputCount))break;
        if(!outputCount)return 0;
        if(xl6ForeignValueOutputHasReturn(kind)){--outputCount;memmove(outputs,outputs+1,outputCount*sizeof(*outputs));}
    }
    for(size_t i=0;attrs && i<attrs->childCount;++i){
        XL6_ForeignRegionContract row;
        if(xl6ForeignRegionRead(attrs->children[i],&row) &&
            xl6ForeignRegionPhysicalInput(&facts,params->childCount+receiver,receiver,outputs,outputCount,xl6ForeignSpanLimitParameters(node),row.physicalIndex)==parameter+receiver){
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

static int xl6SemForeignManagedInputCompatible(XL6_SemContext* ctx,const XL6_AstNode* left,const XL6_AstNode* right)
{
    left=xl6SemResolveAliasType(ctx,left);right=xl6SemResolveAliasType(ctx,right);
    if(xl6SemTypeIsNullable(left) || xl6SemTypeIsNullable(right))return 0;
    XL6_ForeignClassDecl a={0},b={0};
    if(!xl6SemForeignViewDeclaration(ctx,left,&a) || !xl6SemForeignViewDeclaration(ctx,right,&b))return 0;
    return ((a.lifetime==XL6_FOREIGN_LIFETIME_OWNED && b.lifetime==XL6_FOREIGN_LIFETIME_CALLBACK) ||
            (b.lifetime==XL6_FOREIGN_LIFETIME_OWNED && a.lifetime==XL6_FOREIGN_LIFETIME_CALLBACK)) &&
        !strcmp(a.cName,b.cName) && !strcmp(a.header,b.header);
}
static int xl6SemForeignManagedInputVariants(XL6_SemContext* ctx,const XL6_AstNode* a,const XL6_AstNode* b)
{
    const XL6_AstNode* left=xl6SemExternFuncParams(a);const XL6_AstNode* right=xl6SemExternFuncParams(b);
    if(!left || !right || left->childCount!=right->childCount)return 0;
    for(size_t i=0;i<left->childCount;++i)
        if(left->children[i]->childCount && right->children[i]->childCount &&
            (xl6SemForeignManagedInputCompatible(ctx,left->children[i]->children[0],right->children[i]->children[0]) ||
             (!xl6SemExternCTypeCompatible(ctx,left->children[i]->children[0],right->children[i]->children[0]) &&
              xl6SemExternOwnedPolicyInputCompatible(ctx,left->children[i]->children[0],right->children[i]->children[0]))))return 1;
    return 0;
}
static int xl6SemForeignHasSpans(const XL6_AstNode* node)
{
    const XL6_AstNode* attrs=xl6ForeignFunctionAttributes(node);
    for(size_t i=0;attrs && i<attrs->childCount;++i){XL6_ForeignParameterContract row;
        if(xl6ForeignParameterRead(attrs->children[i],&row) && row.kind==XL6_FOREIGN_PARAMETER_SPAN)return 1;
    }
    return 0;
}
static int xl6SemForeignReadonlySpanInputCompatible(XL6_SemContext* ctx,
    const XL6_AstNode* left,const XL6_AstNode* right,const XL6_ForeignParameterContract* a,const XL6_ForeignParameterContract* b)
{
    if(!a->readOnly || !b->readOnly || a->kind!=XL6_FOREIGN_PARAMETER_SPAN || b->kind!=a->kind ||
        strcmp(a->pointerCName,b->pointerCName) || strcmp(a->elementCName,b->elementCName) ||
        strcmp(a->header,b->header) || a->countAbi!=b->countAbi || a->spanLimitIndexPlusOne!=b->spanLimitIndexPlusOne)return 0;
    left=xl6SemResolveAliasType(ctx,left);right=xl6SemResolveAliasType(ctx,right);
    if(!left || !right || !xl6SemForeignSpanType(ctx,left,a) || !xl6SemForeignSpanType(ctx,right,b))return 0;
    int ln=xl6SemTypeIsNullable(left),rn=xl6SemTypeIsNullable(right);
    if(ln)left=xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(left));
    if(rn)right=xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(right));
    if(!left || !right)return 0;
    int lk=!left->childCount && xl6TextEquals(left->text,"string") ? 1:
        !left->childCount && xl6TextEquals(left->text,"bytes") ? 2:0;
    int rk=!right->childCount && xl6TextEquals(right->text,"string") ? 1:
        !right->childCount && xl6TextEquals(right->text,"bytes") ? 2:0;
    const XL6_AstNode* lv=xl6SemClassValueType(ctx,left);const XL6_AstNode* rv=xl6SemClassValueType(ctx,right);
    const XL6_AstNode* ld=lv && lv->text ? xl6SemFindTypeDecl(ctx,lv->text):NULL;
    const XL6_AstNode* rd=rv && rv->text ? xl6SemFindTypeDecl(ctx,rv->text):NULL;
    if(!lk && ld && ld->kind==XL6_AST_CLASS)lk=3;
    if(!rk && rd && rd->kind==XL6_AST_CLASS)rk=3;
    return lk && rk && (lk!=rk || ln!=rn);
}
static int xl6SemForeignArraySpanInputCompatible(XL6_SemContext* ctx,const XL6_AstNode* a,const XL6_AstNode* b,
    size_t parameter,const XL6_AstNode* left,const XL6_AstNode* right)
{
    XL6_ForeignArrayGroupContract x,y;
    if(!xl6ForeignArrayForParameter(a,parameter,&x) || !xl6ForeignArrayForParameter(b,parameter,&y) ||
        parameter>=x.sizeParameter || parameter>=y.sizeParameter || memcmp(&x,&y,sizeof(x)))return 0;
    const XL6_AstNode* aa=xl6ForeignFunctionAttributes(a);const XL6_AstNode* bb=xl6ForeignFunctionAttributes(b);
    XL6_ForeignArrayFieldContract u={0},v={0};size_t ux=0,vy=0;
    for(size_t i=0;aa && i<aa->childCount;++i){XL6_ForeignArrayFieldContract f;
        if(xl6ForeignArrayFieldRead(aa->children[i],&f) && f.groupPhysicalIndex==x.physicalIndex && f.column==parameter-x.firstParameter){u=f;++ux;}}
    for(size_t i=0;bb && i<bb->childCount;++i){XL6_ForeignArrayFieldContract f;
        if(xl6ForeignArrayFieldRead(bb->children[i],&f) && f.groupPhysicalIndex==y.physicalIndex && f.column==parameter-y.firstParameter){v=f;++vy;}}
    if(ux!=1 || vy!=1 || u.kind!=XL6_FOREIGN_ARRAY_ELEMENT_SPAN || memcmp(&u,&v,sizeof(u)))return 0;
    const XL6_AstNode* types[2]={left,right};
    for(size_t i=0;i<2;++i){const XL6_AstNode* t=xl6SemResolveAliasType(ctx,types[i]);
        if(!t || xl6SemTypeIsNullable(t) || !xl6TextEquals(t->text,"array") || t->childCount!=1 ||
            t->children[0]->kind!=XL6_AST_LIST || t->children[0]->childCount!=1)return 0;
        t=xl6SemResolveAliasType(ctx,t->children[0]->children[0]);
        if(!t || t->childCount || (!xl6TextEquals(t->text,"string") && !xl6TextEquals(t->text,"bytes")))return 0;
    }
    return 1;
}
