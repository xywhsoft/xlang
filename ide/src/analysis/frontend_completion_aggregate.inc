/* Completion-only enumeration. Keep native field lookup, type resolution,
 * canonical ownership, query filtering and limits; no C backing-name guesses. */
static void xlsSemCompletionAggregate(XL6_SemContext* ctx,const XL6_AstNode* declaration,const XL6_AstNode* concrete)
{
 XL6_CompletionQuery* q=ctx->completionQuery;
 const XL6_AstNode* body=declaration->childCount?declaration->children[0]:NULL;
 for(size_t i=0;body && i<body->childCount && !q->error;++i){
  const XL6_AstNode* field=body->children[i];
  if(!field || field->kind!=XL6_AST_FIELD || !field->text ||
     xl6SemFindUnionFieldNode(declaration,field->text)!=field)continue;
  XL6_AstNode* type=xl6SemCloneRecordFieldType(ctx,concrete,field->text);
  if(!type){q->error="resolved aggregate field type is unavailable";break;}
  xl6CompletionAdd(q,field->text,"field",field->semanticSymbolId,field,xl6SemCompletionType(ctx,type));
  xl6AstFree(type);
 }
}
