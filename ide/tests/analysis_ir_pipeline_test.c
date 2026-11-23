#include "analysis/frontend_internal.h"
#include "analysis/ir_native_pipeline.h"
#include "analysis/frontend_cancel.h"
#include <stdio.h>
#include <string.h>
static xls_semantic_check* pipelineChecked(const char*,const xls_semantic_input*,size_t,int,int(*)(void*),void*,char*,size_t);
#define xlsSemanticCheckCreate pipelineChecked
#define main pipelinePriorMemoMain
#include "analysis_ir_memo_test.c"
#undef main
#undef xlsSemanticCheckCreate
static xatomic32 pipelineModels,pipelineExpected,pipelineErrors;
static _Thread_local int probePaused;
static int samePipeline(const xls_semantic_check* source)
{
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;
 xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b&&status==XLS_PIPELINE_OK);
 CHECK(xlsIrNativePipelineState(b)==XLS_PIPELINE_EMPTY&&!xlsIrNativePipelineModule(b));
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(source),&d,8u*1024u*1024u,&status)&&status==XLS_PIPELINE_OK);
 CHECK(xlsIrNativePipelineState(b)==XLS_PIPELINE_PREPARED&&xlsIrNativePipelinePhase(b)==0);
 const XL6_IrModule* raw=xlsIrNativePipelineModule(b);CHECK(raw&&raw->nodes&&raw->root&&!raw->ready);
 XL6_IrModule moved;xl6IrModuleInit(&moved);CHECK(!xlsIrNativePipelineTake(b,&moved));
 size_t count=xlsIrNativePipelinePhaseCount();CHECK(count==39&&!xlsIrNativePipelinePhaseName(count));
 unsigned mode=xrtAtomic32FetchAdd(&pipelineModels,1,XMEMORY_RELAXED)%3;
 if(mode!=0){
  size_t steps=mode==1?count:count/2;
  for(size_t i=0;i<steps;++i){CHECK(xlsIrNativePipelinePhaseName(i));CHECK(xlsIrNativePipelineAdvance(b,&d,&status)&&status==XLS_PIPELINE_OK&&xlsIrNativePipelinePhase(b)==i+1);}
 }
 CHECK(xlsIrNativePipelineFinish(b,&d,&status)&&!d.count&&status==XLS_PIPELINE_OK);
 CHECK(xlsIrNativePipelineState(b)==XLS_PIPELINE_READY&&xlsIrNativePipelinePhase(b)==count);
 const XL6_IrModule* final=xlsIrNativePipelineModule(b);CHECK(final&&final->ready);
 char *expected=dump(xlsSemanticCheckIr(source)),*actual=dump(final);CHECK(expected&&actual&&!strcmp(expected,actual));
 const XL6_IrNode* nodes=final->nodes;const XL6_IrType* types=final->types.items;const XL6_IrNodeId* children=final->children;
 CHECK(xlsIrNativePipelineAdvance(b,&d,&status)&&xlsIrNativePipelinePhase(b)==count);
 CHECK(xlsIrNativePipelineTake(b,&moved)&&moved.nodes==nodes&&moved.types.items==types&&moved.children==children);
 CHECK(xlsIrNativePipelineState(b)==XLS_PIPELINE_MOVED&&!xlsIrNativePipelineModule(b)&&!xlsIrNativePipelineTake(b,&moved));
 xlsIrNativePipelineDestroy(b);CHECK(xl6IrVerifyModule(&moved,&d)&&!d.count);
 char* taken=dump(&moved);CHECK(taken&&!strcmp(expected,taken));xrtFree(taken);xrtFree(expected);xrtFree(actual);xl6IrModuleFree(&moved);xl6DiagFree(&d);return 1;
}
static xls_semantic_check* pipelineChecked(const char* entry,const xls_semantic_input* in,size_t count,int capture,int(*cancelled)(void*),void* user,char* error,size_t capacity)
{
 xls_semantic_check* result=xlsSemanticCheckCreate(entry,in,count,capture,cancelled,user,error,capacity);
 if(result&&!probePaused&&xlsSemanticCheckInfo(result).accepted){
  xrtAtomic32FetchAdd(&pipelineExpected,1,XMEMORY_RELAXED);
  if(!samePipeline(result))xrtAtomic32FetchAdd(&pipelineErrors,1,XMEMORY_RELAXED);
 }
 return result;
}

/* External allocator/API failure injection,not XRT private allocator tracing. */
static _Thread_local size_t faultAt,faultCalls,denied,live;
static _Thread_local int faultActive,overflow;
static _Thread_local void* allocations[8192];
static int refuse(void){if(faultActive&&++faultCalls==faultAt){++denied;return 1;}return 0;}
static void remember(void* p)
{if(faultActive&&p){for(size_t i=0;i<8192;++i)if(allocations[i]==p)return;for(size_t i=0;i<8192;++i)if(!allocations[i]){allocations[i]=p;++live;return;}overflow=1;}}
static void forget(void* p){if(p)for(size_t i=0;i<8192;++i)if(allocations[i]==p){allocations[i]=NULL;--live;return;}}
void* __real_xrtMalloc(size_t);void* __real_xrtCalloc(size_t,size_t);void* __real_xrtRealloc(void*,size_t);void __real_xrtFree(void*);
char* __real_xrtStrDup(const char*);char* __real_xrtStrDupN(const char*,size_t);
void* __wrap_xrtMalloc(size_t n){if(refuse())return NULL;void* p=__real_xrtMalloc(n);remember(p);return p;}
void* __wrap_xrtCalloc(size_t n,size_t w){if(refuse())return NULL;void* p=__real_xrtCalloc(n,w);remember(p);return p;}
void* __wrap_xrtRealloc(void* p,size_t n){if(refuse())return NULL;void* q=__real_xrtRealloc(p,n);if(q){forget(p);remember(q);}return q;}
void __wrap_xrtFree(void* p){forget(p);__real_xrtFree(p);}
char* __wrap_xrtStrDup(const char* s){if(refuse())return NULL;char* p=__real_xrtStrDup(s);remember(p);return p;}
char* __wrap_xrtStrDupN(const char* s,size_t n){if(refuse())return NULL;char* p=__real_xrtStrDupN(s,n);remember(p);return p;}
bool __real_xrtArrayPush(xarray*,const void*);bool __real_xrtArrayResize(xarray*,size_t);bool __real_xrtMapSet(xmap*,xbytesview,const void*);bool __real_xrtStrBufReserve(xstrbuf*,size_t);
bool __wrap_xrtArrayPush(xarray* a,const void* v){return !refuse()&&__real_xrtArrayPush(a,v);}
bool __wrap_xrtArrayResize(xarray* a,size_t n){return !refuse()&&__real_xrtArrayResize(a,n);}
bool __wrap_xrtMapSet(xmap* m,xbytesview k,const void* v){return !refuse()&&__real_xrtMapSet(m,k,v);}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){return !refuse()&&__real_xrtStrBufReserve(b,n);}
static const char* resourceSource="func stable(int count) as int\n string value=\"outer\"\n try\n while count>0\n string item=\"item\"\n count=count-1\n if count>2\n continue\n end\n break\n end\n return count+2\n finally\n string final=\"final\"\n end\nend\nfunc main(...) as int\n return stable(4)\nend\n";
static int wholeMode(const XL6_AstNode* root,XL6_DiagList* d,xls_ir_native_pipeline** out,xls_ir_pipeline_status* status,unsigned mode)
{
 *out=xlsIrNativePipelineCreate(status);if(!*out||!xlsIrNativePipelinePrepare(*out,root,d,8u*1024u*1024u,status))return 0;
 size_t steps=mode==1?xlsIrNativePipelinePhaseCount():mode==2?13:0;
 for(size_t i=0;i<steps;++i)if(!xlsIrNativePipelineAdvance(*out,d,status))return 0;
 return xlsIrNativePipelineFinish(*out,d,status);
}
static int whole(const XL6_AstNode* root,XL6_DiagList* d,xls_ir_native_pipeline** out,xls_ir_pipeline_status* status)
{return wholeMode(root,d,out,status,0);}
typedef struct pipeline_stop {size_t at,calls;} pipeline_stop;
static int pipelineCancel(void* p){pipeline_stop* stop=p;return ++stop->calls==stop->at;}
static int resources(const char* path,unsigned mode)
{
 ++probePaused;xls_semantic_check* source=checked(path,resourceSource);--probePaused;CHECK(source&&xlsSemanticCheckInfo(source).accepted);
 const XL6_AstNode* root=xlsSemanticCheckAst(source);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_native_pipeline* b=NULL;xls_ir_pipeline_status status;
 faultActive=1;faultCalls=faultAt=denied=0;int okay=wholeMode(root,&d,&b,&status,mode);faultActive=0;CHECK(okay&&!d.count&&!denied&&!overflow);size_t count=faultCalls;
 xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);CHECK(!live);
 for(size_t i=1;i<=count;++i){
  xl6DiagInit(&d);faultActive=1;faultCalls=denied=0;faultAt=i;b=NULL;okay=wholeMode(root,&d,&b,&status,mode);faultActive=0;CHECK(denied==1&&!overflow);
  if(okay){char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected)&&!d.count);xrtFree(actual);}
  else CHECK(status!=XLS_PIPELINE_OK&&!xlsIrNativePipelineModule(b));
  xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);CHECK(!live);
 }
 /* A completed descriptor transfers arrays/payloads without an allocation. */
 xl6DiagInit(&d);CHECK(whole(root,&d,&b,&status));XL6_IrModule taken;xl6IrModuleInit(&taken);faultActive=1;faultAt=1;faultCalls=denied=0;
 CHECK(xlsIrNativePipelineTake(b,&taken));faultActive=0;CHECK(!faultCalls&&!denied);xlsIrNativePipelineDestroy(b);xl6IrModuleFree(&taken);xl6DiagFree(&d);
 pipeline_stop stop={0};xls_frontend_cancel_scope scope;xl6DiagInit(&d);xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);CHECK(wholeMode(root,&d,&b,&status,mode));xlsFrontendCancelEnd(&scope,NULL);size_t polls=stop.calls;xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 for(size_t i=1;i<=polls;++i){
  xl6DiagInit(&d);stop=(pipeline_stop){.at=i};xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);faultActive=1;faultCalls=faultAt=denied=0;b=NULL;
  okay=wholeMode(root,&d,&b,&status,mode);faultActive=0;xls_frontend_cancel_work work;xlsFrontendCancelEnd(&scope,&work);
  CHECK(!okay&&work.cancelled&&status==XLS_PIPELINE_CANCELLED&&!xlsIrNativePipelineModule(b));xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);CHECK(!live&&!overflow);
 }
 printf("pipeline resources mode%u: %zu allocation/API rejections,%zu cancellation points\n",mode,count,polls);
 xrtFree(expected);xlsSemanticCheckDestroy(source);return 1;
}
static int lifetimeAndVerification(const char* path)
{
 ++probePaused;xls_semantic_check* source=checked(path,resourceSource);--probePaused;CHECK(source&&xlsSemanticCheckInfo(source).accepted);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);
 CHECK(!xlsIrNativePipelinePrepare(b,NULL,&d,0,&status)&&status==XLS_PIPELINE_INPUT);
 for(unsigned mode=0;mode<3;++mode){
  CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(source),&d,mode?8u*1024u*1024u:0,&status));
  CHECK(!xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(source),&d,0,&status)&&status==XLS_PIPELINE_INPUT);
  for(size_t i=0;i+1<xlsIrNativePipelinePhaseCount();++i)CHECK(xlsIrNativePipelineAdvance(b,&d,&status));
  /* A bad producer result must be rejected by the mandatory final verifier. */
  XL6_IrModule* bad=(XL6_IrModule*)xlsIrNativePipelineModule(b);CHECK(bad&&bad->nodeCount>3&&!bad->ready);bad->nodes[2].parent=UINT32_MAX;
  CHECK(!xlsIrNativePipelineAdvance(b,&d,&status)&&status==XLS_PIPELINE_NATIVE_ERROR&&xlsIrNativePipelineState(b)==XLS_PIPELINE_FAILED&&!xlsIrNativePipelineModule(b));
  xl6DiagFree(&d);xl6DiagInit(&d);xlsIrNativePipelineReset(b);CHECK(xlsIrNativePipelineState(b)==XLS_PIPELINE_EMPTY);
 }
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(source),&d,SIZE_MAX,&status)&&xlsIrNativePipelineFinish(b,&d,&status));
 xlsSemanticCheckDestroy(source);char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected));xrtFree(actual);
 XL6_IrModule moved;xl6IrModuleInit(&moved);CHECK(xlsIrNativePipelineTake(b,&moved));xlsIrNativePipelineDestroy(b);CHECK(xl6IrVerifyModule(&moved,&d)&&!d.count);xl6IrModuleFree(&moved);xl6DiagFree(&d);xrtFree(expected);return 1;
}
static int pipelineTests(void)
{
 CHECK(run());CHECK(xrtAtomic32Load(&pipelineModels,XMEMORY_RELAXED)>0&&xrtAtomic32Load(&pipelineModels,XMEMORY_RELAXED)==xrtAtomic32Load(&pipelineExpected,XMEMORY_RELAXED)&&!xrtAtomic32Load(&pipelineErrors,XMEMORY_RELAXED));
 char* path=xrtPathAbs("pipeline-中文.xl");CHECK(path);for(unsigned mode=0;mode<3;++mode)CHECK(resources(path,mode));CHECK(lifetimeAndVerification(path));xrtFree(path);
 return 1;
}
#ifdef XLS_IR_PIPELINE_LIBRARY
int xlsIrPipelineFixtureMain(void)
#else
int main(void)
#endif
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_pipeline_test passed:component unavailable in ordinary profile");return 0;}
 xrtAtomic32Init(&pipelineModels,0);xrtAtomic32Init(&pipelineExpected,0);xrtAtomic32Init(&pipelineErrors,0);xrtAtomic32Init(&checks,0);
 if(!pipelineTests())return 1;
 printf("analysis_ir_pipeline_test passed: %u checks;%u complete semantic inputs;raw prelude,38original phases,complete verifier,independent moved native ownership\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&pipelineModels,XMEMORY_RELAXED));return 0;
}
