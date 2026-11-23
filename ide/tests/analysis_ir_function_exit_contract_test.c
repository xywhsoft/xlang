#include "analysis/frontend_internal.h"
#include "xl6_ir.c"
#define XLS_IR_FUNCTION_EXIT_LIBRARY 1
#include "analysis_ir_function_exit_test.c"
#if XLS_EXPERIMENTAL_IR_FUNCTION_EXITS && defined(XLS_NATIVE_IR_RANGE_PRODUCTION)
typedef struct fn_contract_field {size_t offset,width;const char* name;} fn_contract_field;
#define FN_FIELD(f) {offsetof(XL6_IrNode,f),sizeof(((XL6_IrNode*)0)->f),#f}
static const fn_contract_field fnContractFields[]={
 FN_FIELD(kind),FN_FIELD(sourceKind),FN_FIELD(opcode),FN_FIELD(operatorErrorAbi),
 FN_FIELD(castErrorAbi),FN_FIELD(implicitConversionErrorAbi),FN_FIELD(type),FN_FIELD(storageType),
 FN_FIELD(symbol),FN_FIELD(targetSymbol),FN_FIELD(compoundOperatorSymbol),FN_FIELD(callKind),
 FN_FIELD(implicitNameAbi),FN_FIELD(memberAbi),FN_FIELD(projectionErrorAbi),FN_FIELD(evaluationErrorAbi),
 FN_FIELD(constructionErrorAbi),FN_FIELD(indexWriteErrorAbi),FN_FIELD(lvalueKind),FN_FIELD(exitKind),
 FN_FIELD(scopeDepth),FN_FIELD(targetScopeDepth),FN_FIELD(region),FN_FIELD(targetRegion),
 FN_FIELD(parent),FN_FIELD(firstChild),FN_FIELD(childCount)
};
#undef FN_FIELD
static int fnContractCompare(const XL6_IrModule* m,const xls_fn_entry* e,const xls_fn_input* original,const unsigned char* initializers,int same)
{
 xls_fn_input input={0};int okay=xlsFnBuildInput(&input,m,e,NULL,initializers,8u*1024u*1024u);
 int equal=okay&&xlsIrMemoEqual(&input.key.data,&original->key.data);xlsFnInputClear(&input);CHECK(equal==same);return 1;
}
static int fnContextCompare(const XL6_IrModule* m,xls_ir_memo_build* build,const xls_fn_context* original,int same)
{
 xls_fn_context* c=xlsFnContextBuild(m,build,8u*1024u*1024u);CHECK(c);
 int equal=xlsIrMemoEqual(&c->key.data,&original->key.data);xlsFnContextRelease(c);CHECK(equal==same);return 1;
}
static int functionContract(void)
{
 xrtAtomic32Init(&checks,0);functionLimit=8u*1024u*1024u;
 char* path=xrtPathAbs("function-exit-input-contract.xl");CHECK(path);xls_semantic_check* ref=checked(path,functionTail);CHECK(ref&&xlsSemanticCheckInfo(ref).accepted);
 const XL6_AstNode* root=xlsSemanticCheckAst(ref);XL6_IrModule module;XL6_DiagList diags;xls_ir_memo* memo=NULL;xls_ir_memo_work raw;
 xl6IrModuleInit(&module);xl6DiagInit(&diags);xlsIrFunctionExitLimit(functionLimit);
 CHECK(xlsFrontendIrBuildMemo(&module,root,&diags,NULL,&memo,&raw,32u*1024u*1024u)&&memo&&!diags.count);
 xls_function_exit_cache* cache=memo->functionExits;CHECK(cache&&cache->work.assembled);
 size_t slot=0;while(slot<cache->entries.Count&&!((xls_fn_entry*)xrtArrayGet(&cache->entries,slot))->unit)++slot;
 CHECK(slot<cache->entries.Count);xls_fn_entry entry=*(xls_fn_entry*)xrtArrayGet(&cache->entries,slot);entry.lexical=(xls_lexical_symbols){0};
 CHECK(xlsLexicalSymbolsBuild(&entry.lexical,root->children[slot],XLS_LEXICAL_SYMBOL_BYTES));
 unsigned char* initializers=xrtCalloc(module.nodeCount,1);CHECK(initializers);
 for(size_t i=0;i<module.valueInitializerCount;++i){const XL6_IrValueInitializer* v=&module.valueInitializers[i];if(v->declaration&&!initializers[v->declaration-1])initializers[v->declaration-1]=v->errorAbi==XL6_IR_ERROR_ABI_MAY_RAISE?2:1;}
 xls_fn_input original={0};CHECK(xlsFnBuildInput(&original,&module,&entry,NULL,initializers,8u*1024u*1024u));CHECK(xlsIrMemoEqual(&original.key.data,&entry.unit->key));
 XL6_IrModule changed=module;changed.nodes=xrtMalloc(module.nodeCount*sizeof(*changed.nodes));CHECK(changed.nodes);memcpy(changed.nodes,module.nodes,module.nodeCount*sizeof(*changed.nodes));
 for(size_t field=0;field<sizeof(fnContractFields)/sizeof(*fnContractFields);++field){
  size_t at=entry.first;const fn_contract_field* f=&fnContractFields[field];CHECK(f->width<=8);
  if(f->offset==offsetof(XL6_IrNode,region))while(at<entry.first+entry.count&&(module.nodes[at].kind!=XL6_IR_NODE_LOOP||!module.nodes[at].region))++at;
  if(f->offset==offsetof(XL6_IrNode,targetRegion))while(at<entry.first+entry.count&&!module.nodes[at].targetRegion)++at;
  CHECK(at<entry.first+entry.count);
  if(f->offset==offsetof(XL6_IrNode,region)||f->offset==offsetof(XL6_IrNode,targetRegion))memset((unsigned char*)&changed.nodes[at]+f->offset,0,f->width);
  else *((unsigned char*)&changed.nodes[at]+f->offset)^=1;
  if(!fnContractCompare(&changed,&entry,&original,initializers,0)){fprintf(stderr,"function input field not guarded:%s\n",f->name);return 0;}
  changed.nodes[at]=module.nodes[at];
 }
 changed.nodes[entry.first].span.line+=7;CHECK(fnContractCompare(&changed,&entry,&original,initializers,1));changed.nodes[entry.first]=module.nodes[entry.first];
 size_t name=entry.first;while(name<entry.first+entry.count&&(!module.nodes[name].text||!module.nodes[name].text[0]||!strcmp(module.nodes[name].text,"_")||!strcmp(module.nodes[name].text,"destroy")))++name;CHECK(name<entry.first+entry.count);
 changed.nodes[name].text="another spelling";CHECK(fnContractCompare(&changed,&entry,&original,initializers,1));
 const char* categories[]={NULL,"","_","destroy"};for(size_t i=0;i<4;++i){changed.nodes[name].text=(char*)categories[i];CHECK(fnContractCompare(&changed,&entry,&original,initializers,0));}changed.nodes[name]=module.nodes[name];
 initializers[entry.first]=2;CHECK(fnContractCompare(&changed,&entry,&original,initializers,0));initializers[entry.first]=0;
 /* Bijective region renaming must preserve the key,including all references. */
 for(size_t i=entry.first;i<entry.first+entry.count;++i){if(changed.nodes[i].region)changed.nodes[i].region+=1000;if(changed.nodes[i].targetRegion)changed.nodes[i].targetRegion+=1000;}
 CHECK(fnContractCompare(&changed,&entry,&original,initializers,1));memcpy(changed.nodes,module.nodes,module.nodeCount*sizeof(*changed.nodes));
 xls_ir_memo_build build={.root=root,.module=&module,.memo=memo};xls_fn_context* context=xlsFnContextBuild(&module,&build,8u*1024u*1024u);CHECK(context);
 changed.types.items=xrtMalloc(module.types.count*sizeof(*changed.types.items));CHECK(changed.types.items);memcpy(changed.types.items,module.types.items,module.types.count*sizeof(*changed.types.items));
 changed.types.items[0].ownershipAbi=(XL6_IrOwnershipAbi)(changed.types.items[0].ownershipAbi^1);CHECK(fnContextCompare(&changed,&build,context,0));changed.types.items[0]=module.types.items[0];
 changed.runtimeTypeMap.fallbackTypeValueId^=1;CHECK(fnContextCompare(&changed,&build,context,0));changed.runtimeTypeMap=module.runtimeTypeMap;
 changed.nodes[entry.first].sourceKind=XL6_AST_NATIVE_EXTERN_SPECIAL;CHECK(fnContextCompare(&changed,&build,context,0));changed.nodes[entry.first]=module.nodes[entry.first];
 xrtFree(changed.types.items);xrtFree(changed.nodes);xlsFnContextRelease(context);xlsFnInputClear(&original);xlsLexicalSymbolsClear(&entry.lexical);xrtFree(initializers);
 xlsFrontendIrMemoDestroy(memo);xl6IrModuleFree(&module);xl6DiagFree(&diags);xlsSemanticCheckDestroy(ref);xrtFree(path);
 printf("analysis_ir_function_exit_contract_test passed: %u checks;27 fields,five text predicates,position/region invariance,initializer and shared context guards\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 1;
}
#else
static int functionContract(void){puts("PASS function exit contract disabled in this profile");return 1;}
#endif
int main(void){return functionContract()?0:1;}
