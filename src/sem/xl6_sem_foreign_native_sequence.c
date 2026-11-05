/* Trusted Native output: immutable backing, independent exact owning Retain. */
static int xl6SemForeignNativeSequence(XL6_SemContext* ctx,XL6_AstNode* node,
 const XL6_AstNode* attrs,const XL6_AstNode* ownerClass,const XL6_ForeignCallFacts* facts,size_t inputs,size_t receiver)
{
 XL6_ForeignNativeSequenceContract sequence={0};size_t found=0;const XL6_AstNode* retainAttr=NULL;
 for(size_t a=0;attrs && a<attrs->childCount;++a){
  if(xl6ForeignNativeSequenceRead(attrs->children[a],&sequence))++found;
  if(attrs->children[a] && attrs->children[a]->text && !strcmp(attrs->children[a]->text,"c_retain"))retainAttr=attrs->children[a];
 }
 const XL6_AstNode* returns=node->childCount>2 ? node->children[2]:NULL;
 const XL6_AstNode* result=returns && returns->childCount==1 ? xl6SemResolveAliasType(ctx,returns->children[0]):NULL;
 const XL6_AstNode* element=xl6SemNativeSequenceElement(ctx,result);
 const XL6_AstNode* params=xl6SemExternFuncParams(node);XL6_ForeignClassDecl owner={0};char header[256]={0};
 if(ownerClass && xl6ForeignClassRead(ownerClass,&owner,NULL)==1)strcpy(header,owner.header);
 for(size_t a=0;attrs && a<attrs->childCount;++a)if(xl6ForeignFunctionHeaderRead(attrs->children[a],header,sizeof(header)))break;
 if(found!=1 || !retainAttr || !element || !xl6SemNativeSequencePod(ctx,element,0) || !header[0] ||
  !(node->flags&XL6_AST_FLAG_FOREIGN_NO_RAISE) || (node->flags&(XL6_AST_FLAG_FOREIGN_PURE|XL6_AST_FLAG_FOREIGN_FIELD|XL6_AST_FLAG_FOREIGN_OWNED_STRING)) ||
  sequence.dataIndex>=sequence.countIndex || sequence.ownerParameter>=inputs || facts->borrowedViewResult || facts->inplaceOutput || facts->receiverDereference ||
  xl6ForeignOutputLogicalForDeclaration(node,facts,inputs,receiver)==SIZE_MAX)goto invalid;
 const XL6_AstNode* ownerType=sequence.ownerParameter<receiver ? NULL :
  params->children[sequence.ownerParameter-receiver]->children[0];
 XL6_ForeignClassDecl contract={0};const XL6_AstNode* declaration=sequence.ownerParameter<receiver ? ownerClass:
  xl6SemForeignViewDeclaration(ctx,ownerType,&contract);
 if(sequence.ownerParameter<receiver && declaration)xl6ForeignClassRead(declaration,&contract,NULL);
 XL6_AstNode receiverType={0};receiverType.kind=XL6_AST_TYPE;receiverType.text=ownerClass ? ownerClass->text:NULL;
 const XL6_AstNode* valueType=sequence.ownerParameter<receiver ? &receiverType:xl6SemClassValueType(ctx,ownerType);
 const XL6_AstNode* hold=valueType ? xl6SemForeignOwnershipHelper(ctx,retainAttr->children[0],valueType,1):NULL;
 const XL6_AstNode* drop=valueType ? xl6SemForeignOwnershipHelper(ctx,retainAttr->children[2],valueType,0):NULL;
 if(!declaration || contract.lifetime!=XL6_FOREIGN_LIFETIME_OWNED || !contract.policy[0] || contract.watch[0] || !hold || !drop ||
  (ownerType && xl6SemTypeIsNullable(xl6SemResolveAliasType(ctx,ownerType))))goto invalid;
 retainAttr->children[0]->semanticSymbolId=hold->semanticSymbolId;retainAttr->children[2]->semanticSymbolId=drop->semanticSymbolId;
 return 1;
invalid:xl6SemErrorNode(ctx,node,"owned native sequence requires an exact immutable POD span, sealed count ABI, authoritative header and independent owning input with typed Retain/release helpers");return 0;
}
