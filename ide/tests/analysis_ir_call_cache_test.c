#include "analysis/frontend_internal.h"
#include "analysis/ir_native_call_cache.h"
static int callPrepared(xls_ir_native_pipeline*,const XL6_AstNode*,XL6_DiagList*,size_t,xls_ir_pipeline_status*);
#define xlsIrNativePipelinePrepare callPrepared
#define XLS_IR_PIPELINE_LIBRARY 1
#define __wrap_xrtArrayResize callBaseArrayResize
#include "analysis_ir_pipeline_test.c"
#undef __wrap_xrtArrayResize
#undef xlsIrNativePipelinePrepare
static xatomic32 cacheModels;
static _Thread_local unsigned measureArrays,arrayResizes;
bool __wrap_xrtArrayResize(xarray* a,size_t n)
{if(measureArrays)++arrayResizes;return callBaseArrayResize(a,n);}
static int cachePair(const XL6_AstNode* root)
{
 XL6_IrModule reference;xl6IrModuleInit(&reference);XL6_DiagList d;xl6DiagInit(&d);
 CHECK(xlsFrontendIrBuildMemo(&reference,root,&d,NULL,NULL,NULL,0)&&!d.count);char* expected=dump(&reference);CHECK(expected);
 xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);
 xls_ir_call_cache *old=NULL,*next=NULL;xls_ir_call_cache_work cold,warm;
 CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&status)&&xlsIrNativePipelineFinishCachedCalls(b,&d,NULL,16u*1024u*1024u,&old,&cold,&status)&&old&&!d.count);
 char* first=dump(xlsIrNativePipelineModule(b));CHECK(first&&!strcmp(first,expected));xrtFree(first);xlsIrNativePipelineReset(b);
 CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&status)&&xlsIrNativePipelineFinishCachedCalls(b,&d,old,16u*1024u*1024u,&next,&warm,&status)&&next&&!d.count);
 CHECK(warm.reused&&warm.nativeNodes<reference.nodeCount&&warm.bytes==xlsIrCallCacheBytes(next)&&warm.bytes<=16u*1024u*1024u);
 xlsIrCallCacheDestroy(old);xlsIrCallCacheDestroy(next);CHECK(xl6IrVerifyModule(xlsIrNativePipelineModule(b),&d));char* second=dump(xlsIrNativePipelineModule(b));CHECK(second&&!strcmp(second,expected));
 xrtFree(second);xrtFree(expected);xlsIrNativePipelineDestroy(b);xl6IrModuleFree(&reference);xl6DiagFree(&d);return 1;
}
static int callPrepared(xls_ir_native_pipeline* b,const XL6_AstNode* root,XL6_DiagList* d,size_t bytes,xls_ir_pipeline_status* status)
{
 if(!faultActive&&!probePaused&&xrtAtomic32Load(&pipelineExpected,XMEMORY_RELAXED)>xrtAtomic32Load(&pipelineModels,XMEMORY_RELAXED)){
  CHECK(cachePair(root));xrtAtomic32FetchAdd(&cacheModels,1,XMEMORY_RELAXED);
 }
 return xlsIrNativePipelinePrepare(b,root,d,bytes,status);
}
static char* callSource(unsigned edit)
{
 xstrbuf b={0};if(!xrtStrBufAppendFormat(&b,"func targetA(int value,int extra=%u) as int\n return value+extra\nend\nfunc targetB(int value) as int\n return value+2\nend\n",edit==2?8:4))goto failed;
 for(unsigned i=0;i<16;++i)if(!xrtStrBufAppendFormat(&b,"func f%u(int value) as int\n int result=%s(value)\n return result+%u\nend\n",i,i==4&&edit==1?"targetB":"targetA",i))goto failed;
 return xrtStrBufTake(&b);
failed:xrtStrBufFree(&b);return NULL;
}
static int generations(const char* path)
{
 xls_ir_call_cache* previous=NULL;uint64_t restored=0;
 for(unsigned generation=0;generation<5;++generation){
  char* source=callSource(generation%3);CHECK(source);++probePaused;xls_semantic_check* model=checked(path,source);--probePaused;xrtFree(source);CHECK(model&&xlsSemanticCheckInfo(model).accepted);
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);xls_ir_call_cache* next=NULL;xls_ir_call_cache_work work;
  CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
  CHECK(xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&status==XLS_PIPELINE_OK&&!d.count&&next);
  xlsIrCallCacheDestroy(previous);previous=next;
  if(generation){
   CHECK(work.reused&&work.restoredSites&&work.nativeNodes<xlsSemanticCheckIr(model)->nodeCount);
   if(generation==3)CHECK(!work.checked&&!work.nativeNodes);else CHECK(work.checked);
   restored+=work.restoredSites;
  }
  char* expected=dump(xlsSemanticCheckIr(model));xlsSemanticCheckDestroy(model);char* actual=dump(xlsIrNativePipelineModule(b));CHECK(expected&&actual&&!strcmp(expected,actual));
  CHECK(xl6IrVerifyModule(xlsIrNativePipelineModule(b),&d));xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
  printf("call generation%u checked=%llu reused=%llu nativeNodes=%llu sites=%llu arguments=%llu\n",generation,(unsigned long long)work.checked,(unsigned long long)work.reused,(unsigned long long)work.nativeNodes,(unsigned long long)work.restoredSites,(unsigned long long)work.restoredArguments);
 }
 xlsIrCallCacheDestroy(previous);CHECK(restored>0);return 1;
}
static const char* callNamedSource="func options(***) as int\n return 7\nend\nfunc main(...) as int\n return options(first=1,second=2)\nend\n";
static int namedLifetime(const char* path)
{
 xls_ir_call_cache* previous=NULL;
 for(unsigned i=0;i<2;++i){
  ++probePaused;xls_semantic_check* model=checked(path,callNamedSource);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);xls_ir_call_cache* next=NULL;xls_ir_call_cache_work work;CHECK(b);
  CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&next);
  char* expected=dump(xlsSemanticCheckIr(model));xlsSemanticCheckDestroy(model);xlsIrCallCacheDestroy(previous);previous=next;
  const XL6_IrModule* m=xlsIrNativePipelineModule(b);size_t named=0;for(size_t j=0;j<m->callArgumentCount;++j)named+=m->callArguments[j].name!=NULL;CHECK(named==2);
  if(i){CHECK(work.restoredArguments>=2);xlsIrCallCacheDestroy(previous);previous=NULL;}
  char* actual=dump(m);CHECK(expected&&actual&&!strcmp(expected,actual)&&xl6IrVerifyModule(m,&d));xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 xlsIrCallCacheDestroy(previous);return 1;
}
static int namedChange(const char* path)
{
 xls_ir_call_cache* previous=NULL;
 for(unsigned i=0;i<2;++i){
  char* source=xrtStrDup(callNamedSource);CHECK(source);if(i){char* key=strstr(source,"first=1");CHECK(key);memcpy(key,"third",5);}
  ++probePaused;xls_semantic_check* model=checked(path,source);--probePaused;xrtFree(source);CHECK(model&&xlsSemanticCheckInfo(model).accepted);
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);xls_ir_call_cache* next=NULL;xls_ir_call_cache_work work;CHECK(b);
  CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&next);
  if(i)CHECK(work.checked&&work.reused);
  char *expected=dump(xlsSemanticCheckIr(model)),*actual=dump(xlsIrNativePipelineModule(b));CHECK(expected&&actual&&!strcmp(expected,actual));
  xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);xlsSemanticCheckDestroy(model);xlsIrCallCacheDestroy(previous);previous=next;
 }
 xlsIrCallCacheDestroy(previous);return 1;
}
static int cachedResources(const char* path,int hot)
{
 ++probePaused;xls_semantic_check* model=checked(path,callNamedSource);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 char* expected=dump(xlsSemanticCheckIr(model));CHECK(expected);xls_ir_call_cache* previous=NULL;xls_ir_pipeline_status status;
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);
 if(hot){CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedCalls(b,&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineReset(b);}
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
 xls_ir_call_cache* next=NULL;faultActive=1;faultAt=faultCalls=denied=0;
 int okay=xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,NULL,&status);faultActive=0;CHECK(okay&&!d.count&&!denied&&!overflow);size_t allocationsCount=faultCalls;
 xlsIrNativePipelineDestroy(b);xlsIrCallCacheDestroy(next);xl6DiagFree(&d);CHECK(!live);
 for(size_t i=1;i<=allocationsCount;++i){
  xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));next=NULL;
  faultActive=1;faultCalls=denied=0;faultAt=i;okay=xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,NULL,&status);faultActive=0;CHECK(denied==1&&!overflow);
  if(okay){char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected)&&!d.count);xrtFree(actual);}
  else CHECK(!next&&status!=XLS_PIPELINE_OK&&!xlsIrNativePipelineModule(b));
  xlsIrNativePipelineDestroy(b);xlsIrCallCacheDestroy(next);xl6DiagFree(&d);CHECK(!live);
 }
 pipeline_stop stop={0};xls_frontend_cancel_scope scope;xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));next=NULL;
 xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);CHECK(xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,NULL,&status));xlsFrontendCancelEnd(&scope,NULL);size_t polls=stop.calls;xlsIrNativePipelineDestroy(b);xlsIrCallCacheDestroy(next);xl6DiagFree(&d);
 for(size_t i=1;i<=polls;++i){
  xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));next=NULL;stop=(pipeline_stop){.at=i};
  faultActive=1;faultAt=faultCalls=denied=0;xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);okay=xlsIrNativePipelineFinishCachedCalls(b,&d,previous,16u*1024u*1024u,&next,NULL,&status);xls_frontend_cancel_work work;xlsFrontendCancelEnd(&scope,&work);faultActive=0;
  CHECK(!okay&&work.cancelled&&status==XLS_PIPELINE_CANCELLED&&!next&&!xlsIrNativePipelineModule(b));xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);CHECK(!live&&!overflow);
 }
 printf("call-cache resources hot%d: %zu allocation/API rejection points;%zu cancellation points\n",hot,allocationsCount,polls);
 xlsIrCallCacheDestroy(previous);xlsSemanticCheckDestroy(model);xrtFree(expected);return 1;
}
static int budgets(const char* path)
{
 char* text=callSource(0);CHECK(text);++probePaused;xls_semantic_check* model=checked(path,text);--probePaused;xrtFree(text);CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 unsigned baseline=0;
 for(unsigned i=0;i<2;++i){
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
  xls_ir_call_cache* next=NULL;xls_ir_call_cache_work work;arrayResizes=0;measureArrays=1;
  int okay=xlsIrNativePipelineFinishCachedCalls(b,&d,NULL,i?128:0,&next,&work,&status);measureArrays=0;
  CHECK(okay&&!next&&!d.count&&!work.bytes);
  if(!i)baseline=arrayResizes;else CHECK(arrayResizes==baseline&&work.declined&&work.nativeNodes==xlsSemanticCheckIr(model)->nodeCount);
  char *expected=dump(xlsSemanticCheckIr(model)),*actual=dump(xlsIrNativePipelineModule(b));CHECK(expected&&actual&&!strcmp(expected,actual));xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 xlsSemanticCheckDestroy(model);return 1;
}
static int tests(void)
{
 CHECK(!xlsIrPipelineFixtureMain());CHECK(xrtAtomic32Load(&cacheModels,XMEMORY_RELAXED)==49);
 char* path=xrtPathAbs("call-cache-generations-中文.xl");CHECK(path);CHECK(generations(path));CHECK(namedLifetime(path));CHECK(namedChange(path));CHECK(cachedResources(path,0));CHECK(cachedResources(path,1));CHECK(budgets(path));xrtFree(path);return 1;
}
#ifdef XLS_IR_CALL_CACHE_LIBRARY
int xlsIrCallCacheFixtureMain(void)
#else
int main(void)
#endif
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_call_cache_test passed:component unavailable");return 0;}
 xrtAtomic32Init(&cacheModels,0);if(!tests())return 1;
 printf("analysis_ir_call_cache_test passed: %u checks;%u full semantic inputs;actual cross-generation call production,relative argument rebasing,retired source/cache ownership\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&cacheModels,XMEMORY_RELAXED));return 0;
}
