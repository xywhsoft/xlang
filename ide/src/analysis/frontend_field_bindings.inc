/* Observe an already selected native field and successfully cloned/substituted
 * type. No lookup, allocation or second AST walk. Callable/property plans keep
 * precedence; a previous field binding can be replaced during rechecking. */
static XL6_AstNode* xlsSemBoundFieldType(XL6_AstNode* reference,
 const XL6_AstNode* field,XL6_AstNode* result)
{
 if(reference && field && result){
  if(reference->semanticTargetSymbolId &&
     reference->semanticTargetSymbolId!=reference->semanticSymbolId)
   reference->semanticSymbolId=0;
  else reference->semanticSymbolId=reference->semanticTargetSymbolId=field->semanticSymbolId;
 }
 return result;
}
