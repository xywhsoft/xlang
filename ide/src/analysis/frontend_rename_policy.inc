/* Included in the original collector TU; native kinds/flags remain authoritative. */
#include "analysis/rename_policy.h"
int xlsSemanticIndexRenamePolicy(const xls_semantic_index* index,size_t at,unsigned* out)
{
 if(out)*out=XLS_RENAME_POLICY_UNSUPPORTED;
 if(!index || !out || at>=index->build.count)return 0;
 unsigned policy=0;
 for(size_t depth=0;depth<256;++depth){
  const XL6_IndexFact* fact=xlsIndexFactAt(&index->build,at);
  const XL6_AstNode* node=fact ? fact->node : NULL;if(!node)return 0;
#ifdef XLS_NATIVE_AST_PACKAGE_FIELDS
  if(node->compiledPackageRoot || node->compiledPackageLink)policy|=XLS_RENAME_POLICY_COMPILED;
#endif
  if(node->flags&XL6_AST_FLAG_EXTERN)policy|=XLS_RENAME_POLICY_FOREIGN;
  switch(node->kind){
  case XL6_AST_EXTERN_FUNC:case XL6_AST_EXTERN_VAR:case XL6_AST_EXTERN_TYPE_ALIAS:
   policy|=XLS_RENAME_POLICY_FOREIGN;break;
  case XL6_AST_EXPORT_FUNC:case XL6_AST_EXPORT_VAR:case XL6_AST_EXPORT_CONST:
   if(node->text && *node->text)policy|=XLS_RENAME_POLICY_FOREIGN;
   break;
#ifdef XLS_NATIVE_AST_EXTERNAL_MEMBER_DECLS
  case XL6_AST_NATIVE_EXTERN_FUNC:case XL6_AST_NATIVE_EXTERN_SPECIAL:
   policy|=XLS_RENAME_POLICY_FOREIGN;break;
#endif
#ifdef XLS_NATIVE_AST_C_RECORD
  case XL6_AST_C_RECORD:policy|=XLS_RENAME_POLICY_FOREIGN;break;
#endif
  default:break;
  }
  if(!fact->parent){*out=policy;return 1;}
  /* Parents are one-based and must strictly precede the current fact. */
  if(fact->parent>at)return 0;
  at=fact->parent-1;
 }
 return 0;
}
