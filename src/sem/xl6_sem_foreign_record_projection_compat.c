/* Borrowed output records permit independent logical field projections. The
 * producer, input mappings, C record, size initializer and effects stay exact. */
static int xl6SemForeignRecordProjectionCompatible(XL6_SemContext* ctx,const XL6_AstNode* a,
    const XL6_AstNode* b,const XL6_AstNode* owner)
{
    XL6_ForeignCallFacts af,bf;XL6_ForeignOutputRecordContract ar,br;
    if(!xl6ForeignFunctionRead(a,&af) || !xl6ForeignFunctionRead(b,&bf))return 0;
    if(!xl6ForeignValueOutputIsRecord(af.valueOutput) || !xl6ForeignValueOutputIsRecord(bf.valueOutput))return -1;
    /* Re-declaring one logical result keeps its sealed field mapping. Only
     * a distinct result projection admits a different field selection. */
    if(xl6SemExternCReturnsCompatible(ctx,xl6SemExternFuncReturns(a),xl6SemExternFuncReturns(b)))return -1;
    if(!xl6ForeignOutputRecordForDeclaration(a,&ar) || !xl6ForeignOutputRecordForDeclaration(b,&br) ||
        ar.physicalIndex!=br.physicalIndex || strcmp(ar.recordCName,br.recordCName))return 0;
    const uint64 flags=XL6_AST_FLAG_STATIC|XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_NO_RAISE|
        XL6_AST_FLAG_FOREIGN_NONBLOCKING|XL6_AST_FLAG_FOREIGN_OWNED_STRING;
    char ah[256],bh[256];
    if(a->kind!=b->kind || ((a->flags^b->flags)&flags) || !(a->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) ||
        !xl6SemNullHeader(a,owner,ah,sizeof(ah)) || !xl6SemNullHeader(b,owner,bh,sizeof(bh)) || strcmp(ah,bh))return 0;
    af.valueOutput&=~XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;
    bf.valueOutput&=~XL6_FOREIGN_VALUE_OUTPUT_VIEW_CONVERTED;
    if(!xl6ForeignCallFactsEqual(&af,&bf) || !xl6SemExternParameterListsCompatible(ctx,a,b))return 0;
    const XL6_AstNode* attrs[2]={xl6ForeignFunctionAttributes(a),xl6ForeignFunctionAttributes(b)};
    for(size_t side=0;side<2;++side){const XL6_AstNode* left=attrs[side];const XL6_AstNode* right=attrs[1-side];
        for(size_t i=0;left && i<left->childCount;++i){
            if(left->children[i] && xl6TextEquals(left->children[i]->text,"c_output_field"))continue;
            size_t lc=0,rc=0;
            for(size_t j=0;j<left->childCount;++j)lc+=xl6SemForeignAttributeEqual(left->children[i],left->children[j]);
            for(size_t j=0;right && j<right->childCount;++j)rc+=xl6SemForeignAttributeEqual(left->children[i],right->children[j]);
            if(lc!=rc)return 0;
        }
    }
    return 1;
}
