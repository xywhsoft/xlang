/* Nullable storage is limited to the protected Buffer family. A non-null
 * empty Buffer still takes the ordinary read/write span scope. */
static int xl6SemForeignSpanType(XL6_SemContext* ctx,const XL6_AstNode* type,
    const XL6_ForeignParameterContract* contract)
{
    type=xl6SemResolveAliasType(ctx,type);
    if(!type)return 0;
    int nullable=xl6SemTypeIsNullable(type);
    if(nullable)type=xl6SemResolveAliasType(ctx,xl6SemNullableInnerType(type));
    if(!type)return 0;
    if(!nullable){
        if(!type->childCount && (xl6TextEquals(type->text,"string") || xl6TextEquals(type->text,"bytes")))return contract->readOnly;
        if(contract->readOnly && xl6ForeignRecordRead(xl6SemFindTypeDecl(ctx,type->text),NULL,NULL)==1)return 1;
        uint64 extent;
        if(type->childCount==1 && xl6ForeignArrayExtent(type->text,&extent))return contract->readOnly;
    }
    const XL6_AstNode* value=xl6SemClassValueType(ctx,type);
    const XL6_AstNode* declaration=value ? xl6SemFindTypeDecl(ctx,value->text) : NULL;
    const XL6_AstNode* body=declaration && declaration->kind==XL6_AST_CLASS && declaration->childCount>1 ? declaration->children[1] : NULL;
    for(size_t i=0;body && i<body->childCount;++i){
        const XL6_AstNode* member=body->children[i];
        if(member && member->kind==XL6_AST_NATIVE_EXTERN_SPECIAL && member->text &&
            (!strcmp(member->text,"\"xl6BufferDestroyV1\"") || !strcmp(member->text,"xl6BufferDestroyV1")))return 1;
    }
    return 0;
}
