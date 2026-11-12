/* One physical producer can have distinct owning text/bytes admissions. */
static int xl6SemForeignOwnedOutputCompatible(XL6_SemContext* ctx,const XL6_AstNode* a,
    const XL6_AstNode* b,const XL6_AstNode* owner)
{
    XL6_ForeignCallFacts af,bf;
    if(!xl6ForeignFunctionRead(a,&af) || !xl6ForeignFunctionRead(b,&bf))return 0;
    if(!xl6ForeignValueOutputIsOwned(af.valueOutput) && !xl6ForeignValueOutputIsOwned(bf.valueOutput))return -1;
    if(!xl6ForeignValueOutputIsOwned(af.valueOutput) || !xl6ForeignValueOutputIsOwned(bf.valueOutput))return 0;
    const XL6_AstNode* aa=xl6ForeignFunctionAttributes(a);const XL6_AstNode* ba=xl6ForeignFunctionAttributes(b);
    XL6_ForeignOwnedOutputContract ar={0},br={0};size_t ac=0,bc=0;
    for(size_t i=0;aa && i<aa->childCount;++i)ac+=xl6ForeignOwnedOutputRead(aa->children[i],&ar);
    for(size_t i=0;ba && i<ba->childCount;++i)bc+=xl6ForeignOwnedOutputRead(ba->children[i],&br);
    if(ac!=1 || bc!=1 || ar.dataIndex!=br.dataIndex || ar.sizeIndex!=br.sizeIndex ||
       ar.pointerAbi!=br.pointerAbi || ar.sizeAbi!=br.sizeAbi || ar.successCode!=br.successCode)return 0;
    const uint64 flags=XL6_AST_FLAG_STATIC|XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_NO_RAISE|
        XL6_AST_FLAG_FOREIGN_NONBLOCKING|XL6_AST_FLAG_FOREIGN_OWNED_STRING;
    char ah[256],bh[256];
    if(a->kind!=b->kind || ((a->flags^b->flags)&flags) ||
       !xl6SemNullHeader(a,owner,ah,sizeof(ah)) || !xl6SemNullHeader(b,owner,bh,sizeof(bh)) || strcmp(ah,bh))return 0;
    af.valueOutput=bf.valueOutput=XL6_FOREIGN_VALUE_OUTPUT_OWNED_BYTES_STATUS;
    if(!xl6ForeignCallFactsEqual(&af,&bf) || !xl6SemExternParameterListsCompatible(ctx,a,b) ||
       !aa || !ba || aa->childCount!=ba->childCount)return 0;
    for(size_t i=0;i<aa->childCount;++i){
        XL6_ForeignOwnedOutputContract ignored;
        if(xl6ForeignOwnedOutputRead(aa->children[i],&ignored))continue;
        size_t left=0,right=0;
        for(size_t j=0;j<aa->childCount;++j)left+=xl6SemForeignAttributeEqual(aa->children[i],aa->children[j]);
        for(size_t j=0;j<ba->childCount;++j)right+=xl6SemForeignAttributeEqual(aa->children[i],ba->children[j]);
        if(left!=right)return 0;
    }
    const XL6_AstNode* nodes[2]={a,b};XL6_ForeignOwnedOutputKind kinds[2]={ar.kind,br.kind};
    for(size_t i=0;i<2;++i){
        const XL6_AstNode* returns=xl6SemExternFuncReturns(nodes[i]);
        const XL6_AstNode* type=returns && returns->childCount==1 ? xl6SemResolveAliasType(ctx,returns->children[0]):NULL;
        if(!type || type->childCount || !xl6TextEquals(type->text,kinds[i]==XL6_FOREIGN_OWNED_OUTPUT_TEXT ? "string":"bytes"))return 0;
    }
    return 1;
}
