#include "analysis/frontend_internal.h"
#include "analysis/ir_native_call_cache.h"
static void callPoison(void*,size_t);
#define XLS_CALL_CACHE_UNIT_FREE_TEST callPoison
#include "xl6_ir.c"
#define XLS_IR_CALL_CACHE_LIBRARY 1
#include "analysis_ir_call_cache_test.c"
static xatomic32 poisoned;
static void callPoison(void* p,size_t bytes)
{xrtAtomic32FetchAdd(&poisoned,1,XMEMORY_RELAXED);memset(p,0,bytes);}
#if defined(XLS_NATIVE_IR_PIPELINE_COMPONENT) && XLS_NATIVE_IR_PIPELINE_COMPONENT && \
    defined(XLS_NATIVE_IR_PIPELINE_SOURCE) && defined(XLS_NATIVE_IR_TYPE_RESOURCE_GUARD)
static int privateContexts(const char* path)
{
 const char* source="func apply(func(int) as int action,int value) as int\n return action(value)\nend\nfunc run() as int\n return apply(func(int value) as int\n return value+1\nend,7)\nend\n";
 ++probePaused;xls_semantic_check* model=checked(path,source);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);xls_ir_call_cache* previous=NULL;CHECK(b);
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedCalls(b,&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineReset(b);
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));while(xlsIrNativePipelinePhase(b)<26)CHECK(xlsIrNativePipelineAdvance(b,&d,&status));
 unsigned char* key=xrtMalloc(16u*1024u*1024u);CHECK(key);xls_call_trace trace={.module=&b->module,.key=key,.capacity=16u*1024u*1024u};int queried=0;
 for(size_t i=0;i<previous->units.Count&&!queried;++i){xls_call_unit* unit=*(xls_call_unit**)xrtArrayGet(&previous->units,i);if(!unit)continue;
  for(size_t j=0;j<unit->reads;++j)if(unit->ids[j].kind==1){
   XL6_IrTypeId id=unit->ids[j].id;if(!id||id>b->module.types.count)continue;CHECK(xlsCallMatches(&trace,unit,unit->first,unit->end));
   XL6_IrCAbi saved=b->module.types.items[id-1].cAbi;b->module.types.items[id-1].cAbi=(XL6_IrCAbi)((unsigned)saved^1u);
   CHECK(!xlsCallMatches(&trace,unit,unit->first,unit->end));b->module.types.items[id-1].cAbi=saved;CHECK(xlsCallMatches(&trace,unit,unit->first,unit->end));queried=1;break;
  }
 }
 CHECK(queried);xrtFree(key);
 size_t capacity=b->module.symbolNodeCapacity;CHECK(capacity<SIZE_MAX/sizeof(*b->module.symbolNodes)-1);
 XL6_IrNodeId* nodes=xrtRealloc(b->module.symbolNodes,(capacity+1)*sizeof(*nodes));CHECK(nodes);b->module.symbolNodes=nodes;nodes[capacity]=0;b->module.symbolNodeCapacity=capacity+1;
 xls_ir_call_cache* next=NULL;xls_ir_call_cache_work work;
 CHECK(xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&next&&!work.reused&&work.checked);
 xlsIrCallCacheDestroy(previous);xlsIrCallCacheDestroy(next);CHECK(xl6IrVerifyModule(&b->module,&d));xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);xlsSemanticCheckDestroy(model);return 1;
}
typedef struct call_reader {const XL6_AstNode* root;const char* expected;const xls_ir_call_cache* previous;int passed;} call_reader;
static int callReaderCheck(call_reader* reader)
{
 for(unsigned pass=0;pass<3;++pass){
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);xls_ir_call_cache* next=NULL;xls_ir_call_cache_work work;
  CHECK(!xlsCallsActiveTrace&&xlsIrNativePipelinePrepare(b,reader->root,&d,0,&status));
  xls_call_trace outer={.failed=1,.reads=123};xlsCallsActiveTrace=&outer;
  int okay=xlsIrNativePipelineFinishCachedCalls(b,&d,reader->previous,16u*1024u*1024u,&next,&work,&status);
  CHECK(xlsCallsActiveTrace==&outer&&outer.failed==1&&outer.reads==123&&!b->calls);xlsCallsActiveTrace=NULL;
  CHECK(okay&&next&&work.reused&&!d.count);char* actual=dump(&b->module);CHECK(actual&&!strcmp(actual,reader->expected));xrtFree(actual);xlsIrCallCacheDestroy(next);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 return 1;
}
static int32 callReaderThread(void* p){call_reader* r=p;r->passed=callReaderCheck(r);return r->passed?0:1;}
static int concurrent(const char* path)
{
 xls_semantic_check* model[2];char* expected[2];
 for(unsigned i=0;i<2;++i){char* source=callSource(i);CHECK(source);++probePaused;model[i]=checked(path,source);--probePaused;xrtFree(source);CHECK(model[i]&&xlsSemanticCheckInfo(model[i]).accepted);expected[i]=dump(xlsSemanticCheckIr(model[i]));CHECK(expected[i]);}
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);xls_ir_call_cache* previous=NULL;
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model[0]),&d,0,&status)&&xlsIrNativePipelineFinishCachedCalls(b,&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 call_reader readers[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){readers[i]=(call_reader){xlsSemanticCheckAst(model[i%2]),expected[i%2],previous,0};threads[i]=xrtThreadCreate(callReaderThread,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].passed);xrtThreadDestroy(threads[i]);}
 xlsIrCallCacheDestroy(previous);
 for(unsigned i=0;i<2;++i){CHECK(xlsSemanticCheckValidate(model[i]));xlsSemanticCheckDestroy(model[i]);xrtFree(expected[i]);}
 return 1;
}
static int contractTests(void)
{
 CHECK(!xlsIrCallCacheFixtureMain());char* path=xrtPathAbs("call-cache-contract-中文.xl");CHECK(path);CHECK(privateContexts(path));CHECK(concurrent(path));xrtFree(path);CHECK(xrtAtomic32Load(&poisoned,XMEMORY_RELAXED)>0);return 1;
}
#else
static int contractTests(void){return 1;}
#endif
int main(void)
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_call_cache_contract_test passed:component unavailable");return 0;}
 xrtAtomic32Init(&poisoned,0);if(!contractTests())return 1;
 printf("analysis_ir_call_cache_contract_test passed: %u checks;%u retired units poisoned;queried types,common context,four concurrent readers,trace restoration\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&poisoned,XMEMORY_RELAXED));return 0;
}
