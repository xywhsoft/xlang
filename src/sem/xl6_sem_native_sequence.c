/* Closed, owning immutable Native arrays. No implicit dynamic element conversion. */
static int xl6SemNativeSequenceContainerArguments(XL6_SemContext* ctx,const char* name,const XL6_AstNode* args,const XL6_AstNode* origin)
{
 name=xl6TypeCanonicalBuiltinName(name);
 bool container=xl6TextEquals(name,"array") || xl6TextEquals(name,"list") || xl6TextEquals(name,"dict") || xl6TextEquals(name,"set") ||
  xl6TextEquals(name,"Stack") || xl6TextEquals(name,"OrderedMap") || xl6TextEquals(name,"SlotMap") || xl6TextEquals(name,"channel") ||
  xl6TextEquals(name,"SpscQueue") || xl6TextEquals(name,"MpscQueue") || xl6TextEquals(name,"MpmcQueue") || xl6TextEquals(name,"WaitQueue") ||
  xl6TextEquals(name,"future") || xl6TextEquals(name,"generator");
 if(!container)return 1;
 for(size_t n=0;args && n<args->childCount;++n){const XL6_AstNode* item=xl6SemResolveAliasType(ctx,args->children[n]);
  const XL6_AstNode* inner=xl6SemNullableInnerType(item);if(inner)item=xl6SemResolveAliasType(ctx,inner);
  if(item && xl6TextEquals(item->text,"NativeSequence")){
   xl6SemErrorNode(ctx,origin,"NativeSequence elements in typed native containers/futures/generators require a separate closed domain and are not supported");return 0;
  }
 }
 return 1;
}
static const XL6_AstNode* xl6SemNativeSequenceElement(XL6_SemContext* ctx,const XL6_AstNode* type)
{
 type=xl6SemResolveAliasType(ctx,type);
 if(!type || xl6SemTypeIsNullable(type) || !xl6TextEquals(type->text,"NativeSequence"))return NULL;
 const XL6_AstNode* args=xl6SemTypeNodeGenericArgs(type);
 return args && args->childCount==1 ? xl6SemResolveAliasType(ctx,args->children[0]):NULL;
}
static int xl6SemNativeSequencePod(XL6_SemContext* ctx,const XL6_AstNode* type,size_t depth)
{
 type=xl6SemResolveAliasType(ctx,type);
 if(!type || depth>16 || xl6SemTypeIsNullable(type) || !type->text || type->childCount)return 0;
 if(!strncmp(type->text,"c_",2) && (xl6SemIsIntegerTypeName(type->text) || xl6TextEquals(type->text,"c_float") || xl6TextEquals(type->text,"c_double") || xl6TextEquals(type->text,"c_bool")))return 1;
 const XL6_AstNode* record=xl6SemFindTypeDecl(ctx,type->text);
 if(!record || record->kind!=XL6_AST_C_RECORD || !record->childCount || !xl6ForeignRecordRead(record,NULL,NULL))return 0;
 const XL6_AstNode* fields=record->children[0];
 for(size_t f=0;fields && f<fields->childCount;++f){const XL6_AstNode* field=fields->children[f];
  if(!field || field->kind!=XL6_AST_FIELD || !field->childCount || !xl6SemNativeSequencePod(ctx,field->children[0],depth+1))return 0;
 }
 return fields!=NULL;
}
static int xl6SemNativeSequenceSuspends(const XL6_AstNode* node)
{
 if(!node)return 0;
 if(node->kind==XL6_AST_ANON_FUNC || node->kind==XL6_AST_FUNC || node->kind==XL6_AST_EXTERN_FUNC || node->kind==XL6_AST_NATIVE_EXTERN_FUNC)return 0;
 if(node->kind==XL6_AST_AWAIT || node->kind==XL6_AST_YIELD)return 1;
 for(size_t i=0;i<node->childCount;++i)if(xl6SemNativeSequenceSuspends(node->children[i]))return 1;
 return 0;
}
static int xl6SemNativeSequenceWrite(XL6_SemContext* ctx,XL6_AstNode* place)
{
 if(!place)return 0;
 if((place->kind==XL6_AST_INDEX || place->kind==XL6_AST_SAFE_INDEX) && place->childCount){
  XL6_AstNode* type=xl6SemInferAutoTypeNode(ctx,place->children[0]);
  int found=xl6SemNativeSequenceElement(ctx,type)!=NULL;xl6AstFree(type);if(found)return 1;
 }
 return place->kind==XL6_AST_MEMBER && place->childCount && xl6SemNativeSequenceWrite(ctx,place->children[0]);
}
