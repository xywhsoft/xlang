#include "analysis/frontend_internal.h"
#include "analysis/ir_native_handle_group.h"
static void handlePoison(void*,size_t);
#define XLS_HANDLE_GROUP_UNIT_FREE_TEST handlePoison
#include "xl6_ir.c"
#define XLS_IR_HANDLE_GROUP_LIBRARY 1
#include "analysis_ir_handle_group_test.c"
static xatomic32 handlePoisoned;
static void handlePoison(void* p,size_t n){xrtAtomic32FetchAdd(&handlePoisoned,1,XMEMORY_RELAXED);memset(p,0,n);}
#if defined(XLS_NATIVE_IR_PIPELINE_COMPONENT) && XLS_NATIVE_IR_PIPELINE_COMPONENT && \
    defined(XLS_NATIVE_IR_PIPELINE_SOURCE) && defined(XLS_NATIVE_IR_TYPE_RESOURCE_GUARD)
static int handleAt28(xls_ir_native_pipeline* b,const XL6_AstNode* root,XL6_DiagList* d)
{
 xls_ir_pipeline_status s;CHECK(xlsIrNativePipelinePrepare(b,root,d,0,&s));
 while(xlsIrNativePipelinePhase(b)<28)CHECK(xlsIrNativePipelineAdvance(b,d,&s));
 return 1;
}
static size_t handleRetainedBytes(const xls_ir_handle_group* c)
{
 size_t n=sizeof(*c)+c->units.Capacity*sizeof(xls_hg_unit*)+(c->context.Data?c->context.Capacity+1:0);
 for(size_t i=0;i<c->units.Count;++i){const xls_hg_unit* u=*(xls_hg_unit*const*)xrtArrayConstGet(&c->units,i);if(u)n+=u->bytes;}
 return n;
}
static int handlePrivate(const char* path)
{
 ++probePaused;xls_semantic_check* model=checked(path,handleSource);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 const XL6_AstNode* root=xlsSemanticCheckAst(model);xls_ir_pipeline_status s;XL6_DiagList d;xl6DiagInit(&d);
 xls_ir_native_pipeline* b[2]={xlsIrNativePipelineCreate(&s),xlsIrNativePipelineCreate(&s)};CHECK(b[0]&&b[1]);
 xls_ir_handle_group* old=NULL;CHECK(xlsIrNativePipelinePrepare(b[0],root,&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b[0],&d,NULL,16u*1024u*1024u,&old,NULL,&s)&&old);xlsIrNativePipelineReset(b[0]);
 CHECK(old->bytes==handleRetainedBytes(old));
 CHECK(handleAt28(b[0],root,&d)&&handleAt28(b[1],root,&d));
 unsigned char* key=xrtMalloc(16u*1024u*1024u);CHECK(key);xls_ir_input_trace trace={.module=&b[0]->module,.key=key,.capacity=16u*1024u*1024u};
 xls_hg_unit* selected=NULL;size_t recorded=0,repeatedInvalidQueries=0;
 for(size_t i=0;i<old->units.Count;++i){xls_hg_unit* u=*(xls_hg_unit**)xrtArrayGet(&old->units,i);CHECK(u&&xlsHGMatches(&trace,u,u->first,u->end));
  size_t owned=0;for(size_t j=0;j<u->reads;++j){
   if(u->ids[j].kind!=1&&u->ids[j].id>u->first&&u->ids[j].id<=u->end)++owned;
   for(size_t k=0;k<j;++k)if(u->ids[k].id==u->ids[j].id&&(u->ids[k].kind==1)==(u->ids[j].kind==1)){
    /* Invalid type queries preserve every presence read;they have no slot. */
    CHECK(u->ids[j].kind==1&&(!u->ids[j].id||u->ids[j].id>b[0]->module.types.count));++repeatedInvalidQueries;
   }
  }
  CHECK(owned==u->end-u->first);recorded+=u->reads;if(u->projects)selected=u;
 }
 CHECK(selected&&recorded);printf("handle dependency records=%zu repeatedInvalidQueries=%zu\n",recorded,repeatedInvalidQueries);
 XL6_IrNodeId member=selected->project[0].origin;CHECK(member&&member<=b[0]->module.nodeCount);
 char* saved[2];for(unsigned i=0;i<2;++i){saved[i]=b[i]->module.nodes[member-1].text;b[i]->module.nodes[member-1].text="absent";}
 CHECK(!xlsHGMatches(&trace,selected,selected->first,selected->end));
 xls_handle_build group={.previous=old,.limit=16u*1024u*1024u};xls_ir_type_resource_scope resource={.previous=xlsActiveTypeResource};xlsActiveTypeResource=&resource;
 xls_ir_read_log outer={.input={.failed=1,.reads=123}};xlsHGActiveTrace=&outer;
 int okay=xlsHGBinds(&b[0]->module,&group);CHECK(okay&&xlsHGActiveTrace==&outer&&outer.input.reads==123&&group.work.checked&&group.work.proofChecks==old->units.Count);
 uint64_t proofs=group.work.proofChecks;CHECK(xlsHGProjects(&b[0]->module,&group)&&group.work.proofChecks==proofs&&group.work.reused&&!group.ready&&!group.scratch&&!group.pending.Count);
 CHECK(group.next->bytes==handleRetainedBytes(group.next)&&group.next->bytes<=group.limit);
 CHECK(xl6IrBuildNativeHandleBinds(&b[1]->module)&&xl6IrBuildNativeHandleProjects(&b[1]->module)&&!resource.failed);
 xlsHGActiveTrace=NULL;xlsActiveTypeResource=resource.previous;
 CHECK(b[0]->module.nativeHandleProjectCount<b[1]->module.nodeCount&&b[0]->module.nativeHandleProjectCount<xlsSemanticCheckIr(model)->nativeHandleProjectCount);
 CHECK(b[0]->module.nativeHandleBindCount==b[1]->module.nativeHandleBindCount&&b[0]->module.nativeHandleProjectCount==b[1]->module.nativeHandleProjectCount);
 CHECK(!memcmp(b[0]->module.nativeHandleBinds,b[1]->module.nativeHandleBinds,b[0]->module.nativeHandleBindCount*sizeof(*b[0]->module.nativeHandleBinds)));
 CHECK(!memcmp(b[0]->module.nativeHandleProjects,b[1]->module.nativeHandleProjects,b[0]->module.nativeHandleProjectCount*sizeof(*b[0]->module.nativeHandleProjects)));
 xlsIrHandleGroupDestroy(group.next);xlsHGWorkingClear(&group);
 for(unsigned i=0;i<2;++i){b[i]->module.nodes[member-1].text=saved[i];xlsIrNativePipelineReset(b[i]);}
 CHECK(handleAt28(b[0],root,&d));trace.module=&b[0]->module;
 int typeChecked=0;for(size_t j=0;j<selected->reads&&!typeChecked;++j)if(selected->ids[j].kind==1){XL6_IrTypeId id=selected->ids[j].id;if(!id||id>b[0]->module.types.count)continue;
  XL6_IrType* t=b[0]->module.types.items+id-1;XL6_IrCAbi prior=t->cAbi;t->cAbi=(XL6_IrCAbi)(prior^1u);
  CHECK(!xlsHGMatches(&trace,selected,selected->first,selected->end));t->cAbi=prior;CHECK(xlsHGMatches(&trace,selected,selected->first,selected->end));typeChecked=1;
 }
 CHECK(typeChecked);xstrbuf common={0};CHECK(xlsHGContext(&common,&b[0]->module,16u*1024u*1024u)&&xlsIrMemoEqual(&common,&old->context));xrtStrBufFree(&common);
 size_t destructor=SIZE_MAX;for(size_t i=0;i<b[0]->module.nodeCount;++i)if(b[0]->module.nodes[i].sourceKind==XL6_AST_NATIVE_EXTERN_SPECIAL&&b[0]->module.nodes[i].kind==XL6_IR_NODE_FUNC_DECL){destructor=i;break;}
 CHECK(destructor!=SIZE_MAX);XL6_AstKind sourceKind=b[0]->module.nodes[destructor].sourceKind;b[0]->module.nodes[destructor].sourceKind=XL6_AST_NAME;
 CHECK(xlsHGContext(&common,&b[0]->module,16u*1024u*1024u)&&!xlsIrMemoEqual(&common,&old->context));xrtStrBufFree(&common);b[0]->module.nodes[destructor].sourceKind=sourceKind;
 xls_ir_handle_group* next=NULL;xls_ir_handle_group_work w;
 CHECK(xlsIrNativePipelineFinishCachedHandles(b[0],&d,old,old->bytes-1,&next,&w,&s)&&next&&w.bytes<=old->bytes-1&&w.bindNodes&&w.projectNodes);
 CHECK(w.bytes==handleRetainedBytes(next));
 char *expected=dump(xlsSemanticCheckIr(model)),*actual=dump(&b[0]->module);CHECK(expected&&actual&&!strcmp(expected,actual));xrtFree(actual);
 xlsIrHandleGroupDestroy(next);xlsIrNativePipelineReset(b[0]);CHECK(handleAt28(b[0],root,&d));
 CHECK(xlsIrNativePipelineAdvanceNodes(b[0],1,&d,&s));next=NULL;CHECK(!xlsIrNativePipelineFinishCachedHandles(b[0],&d,old,16u*1024u*1024u,&next,NULL,&s)&&s==XLS_PIPELINE_INPUT&&!next);
 CHECK(xlsIrNativePipelineFinish(b[0],&d,&s));actual=dump(&b[0]->module);CHECK(actual&&!strcmp(actual,expected));xrtFree(actual);
 xrtFree(expected);xrtFree(key);xlsIrHandleGroupDestroy(old);for(unsigned i=0;i<2;++i)xlsIrNativePipelineDestroy(b[i]);xl6DiagFree(&d);xlsSemanticCheckDestroy(model);return 1;
}
typedef struct handle_reader {const XL6_AstNode* root;const char* expected;const xls_ir_handle_group* old;int passed;} handle_reader;
static int handleReadCheck(handle_reader* r)
{
 for(unsigned i=0;i<3;++i){XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status s;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&s);xls_ir_handle_group* next=NULL;xls_ir_handle_group_work w;
  CHECK(b&&!xlsHGActiveTrace&&xlsIrNativePipelinePrepare(b,r->root,&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b,&d,r->old,16u*1024u*1024u,&next,&w,&s)&&next&&w.reused&&!w.checked&&!b->handles&&!xlsHGActiveTrace);
  CHECK(w.bytes==handleRetainedBytes(next));
  char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,r->expected));xrtFree(actual);xlsIrHandleGroupDestroy(next);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }return 1;
}
static int32 handleReadThread(void* p){handle_reader* r=p;r->passed=handleReadCheck(r);return r->passed?0:1;}
static int handleConcurrent(const char* path)
{
 ++probePaused;xls_semantic_check* m=checked(path,handleSource);--probePaused;CHECK(m&&xlsSemanticCheckInfo(m).accepted);char* expected=dump(xlsSemanticCheckIr(m));CHECK(expected);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status s;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&s);xls_ir_handle_group* old=NULL;
 CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(m),&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b,&d,NULL,16u*1024u*1024u,&old,NULL,&s)&&old);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 handle_reader readers[4];xthread* threads[4];for(unsigned i=0;i<4;++i){readers[i]=(handle_reader){xlsSemanticCheckAst(m),expected,old,0};threads[i]=xrtThreadCreate(handleReadThread,readers+i,0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].passed);xrtThreadDestroy(threads[i]);}
 xlsIrHandleGroupDestroy(old);xlsSemanticCheckDestroy(m);xrtFree(expected);return 1;
}
static int handleContract(void){CHECK(!xlsIrHandleGroupFixtureMain());char* path=xrtPathAbs("handle-contract-中文.xl");CHECK(path);CHECK(handlePrivate(path));CHECK(handleConcurrent(path));xrtFree(path);CHECK(xrtAtomic32Load(&handlePoisoned,XMEMORY_RELAXED)>0);return 1;}
#else
static int handleContract(void){return 1;}
#endif
int main(void)
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_handle_group_contract_test passed:component unavailable");return 0;}
 xrtAtomic32Init(&handlePoisoned,0);if(!handleContract())return 1;
 printf("analysis_ir_handle_group_contract_test passed: %u checks;%u retired units poisoned;project-only dependency invalidates shared proof,type/lifecycle changes,one proof per group,partial scan rejection,four readers\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&handlePoisoned,XMEMORY_RELAXED));return 0;
}
