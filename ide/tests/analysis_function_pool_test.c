#define main functionPoolBaseMain
#include "analysis_ast_materialize_test.c"
#undef main
#include "analysis/typed_module_internal.h"
#include "analysis/ast_size.h"

static unsigned poolFaultKind,poolFaultDenied;
static size_t poolFaultAt,poolFaultVisits;
static int poolFaultEnabled;
static int poolRefuse(unsigned kind)
{if(poolFaultEnabled && kind==poolFaultKind && ++poolFaultVisits==poolFaultAt){++poolFaultDenied;return 1;}return 0;}
xls_ast_storage* __real_xlsAstStorageCreate(size_t);
xls_ast_storage* __wrap_xlsAstStorageCreate(size_t bytes)
{return poolRefuse(1) ? NULL:__real_xlsAstStorageCreate(bytes);}
XL6_AstNode* __real_xlsAstStorageNode(xls_ast_storage*,size_t);
XL6_AstNode* __wrap_xlsAstStorageNode(xls_ast_storage* storage,size_t bytes)
{return poolRefuse(2) ? NULL:__real_xlsAstStorageNode(storage,bytes);}
XL6_AstNode* __real_xlsAstCloneInStorage(const XL6_AstNode*,const xls_ast_clone_options*,xls_ast_storage*,size_t,size_t*);
XL6_AstNode* __wrap_xlsAstCloneInStorage(const XL6_AstNode* input,const xls_ast_clone_options* options,xls_ast_storage* storage,size_t limit,size_t* bytes)
{return poolRefuse(3) ? NULL:__real_xlsAstCloneInStorage(input,options,storage,limit,bytes);}

static XL6_AstNode* poolInput(void)
{
 xstrbuf text={0};CHECK(xrtStrBufAppend(&text,xrtStrView("func sample(int value) as int\n string text=\"a\\0中文😀\"\n int local=value\n")));
 for(unsigned i=0;i<32;++i)CHECK(xrtStrBufAppendFormat(&text," local=local+%u\n",i));
 CHECK(xrtStrBufAppend(&text,xrtStrView(" return local\nend\n")));XL6_ParseResult parsed;xl6ParseResultInit(&parsed);
 CHECK(xl6ParseSource(text.Data,"function-pool-中文😀.xl",&parsed) && !parsed.diags.count && parsed.root->childCount==1);
 XL6_AstNode* out=parsed.root->children[0];parsed.root->children[0]=NULL;xl6ParseResultFree(&parsed);xrtStrBufFree(&text);
 CHECK(nodes(out,0)>128 && out->kind==XL6_AST_FUNC && out->childCount>=4);return out;
}
static int pools(const XL6_AstNode* input)
{
 uint64 baseline=live();xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,SIZE_MAX);
 XL6_AstNode *a=xlsAstMaterialize(input,NULL,NULL),*b=xlsAstMaterialize(input,NULL,NULL),*body=xlsAstMaterialize(input->children[3],NULL,NULL);
 CHECK(a && b && body && a->xlsStorage && b->xlsStorage && body->xlsStorage);
 CHECK(a->xlsStorage!=b->xlsStorage && a->xlsStorage!=body->xlsStorage && b->xlsStorage!=body->xlsStorage);
 CHECK(equal(input,a,1) && equal(input,b,1) && equal(input->children[3],body,1));
 uint64 bytes=xlsAstStorageWork(a->xlsStorage).bytes+xlsAstStorageWork(b->xlsStorage).bytes+xlsAstStorageWork(body->xlsStorage).bytes;
 xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);
 CHECK(work.calls==3 && work.pooledCopies==3 && !work.heapCopies && !work.poolFallbacks && work.chargedBytes==bytes);
 CHECK(work.nodes==2*nodes(input,0)+nodes(input->children[3],0));
 xl6AstFree(a);CHECK(equal(input,b,1));xl6AstFree(body);CHECK(equal(input,b,1));xl6AstFree(b);CHECK(live()==baseline);
 /* Small declarations retain one shared pool and its allocation amortization. */
 XL6_AstNode* tiny=xl6AstNew(XL6_AST_FUNC,NULL);CHECK(tiny);xlsAstMaterializeBegin(&scope,SIZE_MAX);
 a=xlsAstMaterialize(tiny,NULL,NULL);b=xlsAstMaterialize(tiny,NULL,NULL);CHECK(a && b && a->xlsStorage==b->xlsStorage);xlsAstMaterializeEnd(&scope,&work);
 CHECK(work.calls==2 && work.pooledCopies==2 && work.pages==1);xl6AstFree(a);xl6AstFree(b);xl6AstFree(tiny);CHECK(live()==baseline);
 return 1;
}
static int quota(const XL6_AstNode* input)
{
 uint64 baseline=live();xls_ast_materialize_scope scope,child;xlsAstMaterializeBegin(&scope,SIZE_MAX);XL6_AstNode* a=xlsAstMaterialize(input,NULL,NULL);CHECK(a);
 size_t exact=xlsAstStorageWork(a->xlsStorage).bytes;xl6AstFree(a);xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);CHECK(live()==baseline);
 const size_t budgets[]={0,1,4096,exact-1,exact};
 for(size_t i=0;i<sizeof(budgets)/sizeof(*budgets);++i){
  xlsAstMaterializeBegin(&scope,budgets[i]);a=xlsAstMaterialize(input,NULL,NULL);CHECK(a && equal(input,a,1));xlsAstMaterializeEnd(&scope,&work);
  CHECK(work.calls==1 && work.chargedBytes<=budgets[i]);
  CHECK(i==4 ? a->xlsStorage && work.pooledCopies==1 && work.chargedBytes==exact:!a->xlsStorage && work.heapCopies==1 && work.poolFallbacks==(i!=0));
  xl6AstFree(a);CHECK(live()==baseline);
 }
 xlsAstMaterializeBegin(&scope,exact*2);a=xlsAstMaterialize(input,NULL,NULL);CHECK(a && xlsAstMaterializeBudget()==exact);
 xlsAstMaterializeBegin(&child,SIZE_MAX);XL6_AstNode* b=xlsAstMaterialize(input,NULL,NULL);CHECK(b && xlsAstMaterializeBudget()==0);xlsAstMaterializeEnd(&child,NULL);
 CHECK(xlsAstMaterializeBudget()==0);xlsAstMaterializeEnd(&scope,&work);CHECK(work.calls==2 && work.pooledCopies==2 && work.chargedBytes==exact*2);
 xl6AstFree(a);xl6AstFree(b);CHECK(live()==baseline);return 1;
}
static int boundaries(void)
{
 uint64 baseline=live();
 for(unsigned count=63;count<=65;++count){
  XL6_AstNode* input=xl6AstNew(XL6_AST_FUNC,NULL);CHECK(input);
  for(unsigned i=1;i<count;++i){XL6_AstNode* node=xl6AstNewText(XL6_AST_NAME,"value",5,NULL);CHECK(node);xl6AstAddChild(input,node);CHECK(input->childCount==i);}
  xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,SIZE_MAX);XL6_AstNode *a=xlsAstMaterialize(input,NULL,NULL),*b=xlsAstMaterialize(input,NULL,NULL);
  CHECK(a && b && equal(input,a,1) && equal(input,b,1));CHECK((a->xlsStorage!=b->xlsStorage)==(count>=64));
  xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);CHECK(work.calls==2 && work.pooledCopies==2 && work.nodes==2*count);
  xl6AstFree(a);xl6AstFree(b);xl6AstFree(input);CHECK(live()==baseline);
 }
 return 1;
}
typedef struct pool_reader {const XL6_AstNode* input;int okay;} pool_reader;
static int32 readFunctionPool(void* user)
{
 pool_reader* reader=user;reader->okay=1;
 for(unsigned i=0;i<12;++i){
  xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,64u*1024u*1024u);
  XL6_AstNode *a=xlsAstMaterialize(reader->input,NULL,NULL),*b=xlsAstMaterialize(reader->input,NULL,NULL);xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);
  if(!a || !b || a->xlsStorage==b->xlsStorage || !equal(reader->input,a,1) || !equal(reader->input,b,1) || work.calls!=2 || work.pooledCopies!=2 || !work.chargedBytes || work.chargedBytes>64u*1024u*1024u)reader->okay=0;
  xl6AstFree(a);xl6AstFree(b);
 }
 return 0;
}
static int concurrentPools(const XL6_AstNode* input)
{
 uint64 baseline=live();pool_reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(pool_reader){input,0};threads[i]=xrtThreadCreate(readFunctionPool,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].okay);xrtThreadDestroy(threads[i]);}
 CHECK(live()==baseline);return 1;
}
static int retention(const XL6_AstNode* input)
{
 uint64 baseline=live();xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,SIZE_MAX);
 XL6_AstNode *a=xlsAstMaterialize(input,NULL,NULL),*b=xlsAstMaterialize(input,NULL,NULL);CHECK(a && b);
 size_t backingA=xlsAstStorageWork(a->xlsStorage).bytes,backingB=xlsAstStorageWork(b->xlsStorage).bytes;
 xlsAstMaterializeEnd(&scope,NULL);xls_ast_size size={0};CHECK(xlsAstSizeBegin(&size,64u*1024u*1024u,1) && xlsAstSizeTree(&size,a,0,NULL,NULL));size_t own=size.bytes;xlsAstSizeEnd(&size);
 XL6_AstNode* root=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(root);xl6AstAddChild(root,a);xl6AstAddChild(root,b);CHECK(root->childCount==2);
 xls_typed_module* module=xlsTypedModuleTake(root);CHECK(module);xls_typed_function *ownerA=xlsTypedModuleFunction(module,0),*ownerB=xlsTypedModuleFunction(module,1);CHECK(ownerA && ownerB);
 size_t exact=xlsTypedFunctionMetadataBytes(ownerA)+own;CHECK(xlsTypedModuleMeasureFunctions(module,exact,NULL,NULL));
 CHECK(xlsTypedFunctionKnownBytes(ownerA)==exact && !xlsTypedFunctionKnownBytes(ownerB) && own>=backingA && own<backingA+backingB);
 xlsTypedModuleRelease(module);xlsTypedFunctionRelease(ownerB);CHECK(equal(input,xlsTypedFunctionRoot(ownerA),1));xlsTypedFunctionRelease(ownerA);CHECK(live()==baseline);return 1;
}
static int faults(const XL6_AstNode* input)
{
 uint64 baseline=live();
 for(unsigned kind=1;kind<=3;++kind){
  poolFaultKind=kind;poolFaultAt=SIZE_MAX;poolFaultVisits=poolFaultDenied=0;poolFaultEnabled=1;xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,SIZE_MAX);
  XL6_AstNode* a=xlsAstMaterialize(input,NULL,NULL);xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);poolFaultEnabled=0;
  size_t operations=poolFaultVisits;CHECK(a && operations && !poolFaultDenied);xl6AstFree(a);CHECK(live()==baseline);
  for(size_t at=1;at<=operations;++at){
   poolFaultAt=at;poolFaultVisits=poolFaultDenied=0;poolFaultEnabled=1;xlsAstMaterializeBegin(&scope,SIZE_MAX);a=xlsAstMaterialize(input,NULL,NULL);xlsAstMaterializeEnd(&scope,&work);poolFaultEnabled=0;
   CHECK(a && !a->xlsStorage && equal(input,a,1) && poolFaultDenied==1 && work.poolFallbacks==1 && work.heapCopies==1 && work.chargedBytes<=64u*1024u*1024u);
   xl6AstFree(a);CHECK(live()==baseline);
  }
  printf("Function-pool fault kind %u: %zu actual producer points\n",kind,operations);
 }
 return 1;
}
int main(void)
{
 if(functionPoolBaseMain())return 1;
 unsigned prior=checks;XL6_AstNode* input=poolInput();if(!input)return 1;
 if(!pools(input) || !quota(input) || !boundaries() || !retention(input) || !faults(input) || !concurrentPools(input))return 1;
 xl6AstFree(input);printf("analysis_function_pool_test passed: %u new checks; independent backing, exact nested quotas, retirement and producer failures\n",checks-prior);return 0;
}
