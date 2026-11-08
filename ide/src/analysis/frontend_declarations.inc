/* Native counterpart of the shared compatibility spelling table. Preserve
 * original facts and derive source names only from checked grammar positions. */
#include "analysis/declaration.h"
static xls_declaration_shape xlsFrontendDeclarationShape(XL6_AstKind kind)
{
 switch(kind){
#define XLS_DECLARATION_NATIVE_CASE(native,text,category,selection,scope) \
 case XL6_AST_##native:return (xls_declaration_shape){XLS_DECL_##category,XLS_DECL_NAME_##selection,scope};
 XLS_DECLARATION_COMMON_KINDS(XLS_DECLARATION_NATIVE_CASE)
#ifdef XLS_NATIVE_AST_EXTERNAL_MEMBER_DECLS
 XLS_DECLARATION_EXTERNAL_MEMBER_KINDS(XLS_DECLARATION_NATIVE_CASE)
#endif
#ifdef XLS_NATIVE_AST_C_RECORD
 case XL6_AST_C_RECORD:return (xls_declaration_shape){XLS_DECL_RECORD,XLS_DECL_NAME_TOKEN,0};
#endif
#undef XLS_DECLARATION_NATIVE_CASE
 default:return (xls_declaration_shape){0};
 }
}
int xlsSemanticIndexDeclarationFact(const xls_semantic_index* index,size_t at,xls_declaration_fact* out)
{
 if(!out)return 0;
 *out=(xls_declaration_fact){0};
 if(!xlsSemanticIndexFact(index,at,&out->fact))return 0;
 const XL6_IndexBuild* b=&index->build;const XL6_IndexFact* f=xlsIndexFactAt(b,at);const XL6_AstNode* n=f->node;
 out->declarationSource=f->source;out->declarationNode=(unsigned)at+1;out->declarationRange=out->fact.range;
 out->shape=xlsFrontendDeclarationShape(n->kind);
 if(n->kind==XL6_AST_NAME){
  const XL6_IndexFact* parent=f->parent ? xlsIndexFactAt(b,f->parent-1) : NULL;
  if(out->fact.declaredType && parent && parent->node->kind==XL6_AST_TYPE_PARAMS){}
  else if(parent && parent->node->kind==XL6_AST_LIST && parent->parent){
   const XL6_IndexFact* owner=xlsIndexFactAt(b,parent->parent-1);
   if(owner->node->kind==XL6_AST_MULTI_VAR_DECL && owner->node->childCount && owner->node->children[0]==parent->node){
    out->shape=(xls_declaration_shape){XLS_DECL_VARIABLE,XLS_DECL_NAME_TOKEN,0};
    out->declarationNode=parent->parent;out->declarationSource=owner->source;
    out->declarationRange=xlsFrontendRange(b->report,owner->source,&owner->node->range);
   }else out->shape=(xls_declaration_shape){0};
  }else out->shape=(xls_declaration_shape){0};
 }
 /* Anonymous records flatten their source-backed members into the enclosing
    declaration. A keyword token alone is not a record or field name. */
 if(out->shape.name==XLS_DECL_NAME_TOKEN && (!n->text || !*n->text))out->shape.name=XLS_DECL_NAME_NONE;
 if(out->shape.name==XLS_DECL_NAME_TOKEN){
  out->nameNode=(unsigned)at+1;out->nameSource=f->source;out->nameRange=out->fact.tokenRange;
 } else if(out->shape.name==XLS_DECL_NAME_CHILD && n->childCount && n->children[0] && n->children[0]->kind==XL6_AST_NAME){
  if(at+1>=b->count)return 0;
  const XL6_IndexFact* name=xlsIndexFactAt(b,at+1);
  if(name->node!=n->children[0] || name->parent!=at+1)return 0;
  out->nameNode=(unsigned)at+2;out->nameSource=name->source;
  out->nameRange=xlsFrontendRange(b->report,name->source,&name->node->tokenRange);
 }
 return 1;
}
