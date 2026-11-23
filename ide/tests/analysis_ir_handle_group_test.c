#include "analysis/frontend_internal.h"
#include "analysis/ir_native_handle_group.h"
static int handlePrepared(xls_ir_native_pipeline*,const XL6_AstNode*,XL6_DiagList*,size_t,xls_ir_pipeline_status*);
#define xlsIrNativePipelinePrepare handlePrepared
#define XLS_IR_PIPELINE_LIBRARY 1
#include "analysis_ir_pipeline_test.c"
#undef xlsIrNativePipelinePrepare
static xatomic32 handleModels;
static const char* handleSource=
 "class ForeignBox\n native point handle\n extern \"box_close\" destroy()\nend\n"
 "extern \"void*\" class RawBox @header(\"handle.h\") @lifetime(\"caller\")\n native point handle\nend\n"
 "extern \"box_make\" func makeBox() as point\n"
 "extern \"box_alt\" func makeAlt() as point\n"
 "func readHandle(RawBox box) as point\n return box.handle\nend\n"
 "func create() as point\n point raw=makeBox()\n ForeignBox box=ForeignBox(raw)\n return raw\nend\n";
static int handlePair(const XL6_AstNode* root,int positive)
{
 XL6_IrModule reference;xl6IrModuleInit(&reference);XL6_DiagList d;xl6DiagInit(&d);
 CHECK(xlsFrontendIrBuildMemo(&reference,root,&d,NULL,NULL,NULL,0)&&!d.count);
 if(positive)CHECK(reference.nativeHandleBindCount&&reference.nativeHandleProjectCount);
 char* expected=dump(&reference);CHECK(expected);
 xls_ir_pipeline_status s;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&s);CHECK(b);
 xls_ir_handle_group *old=NULL,*next=NULL;xls_ir_handle_group_work cold,warm;
 CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b,&d,NULL,16u*1024u*1024u,&old,&cold,&s)&&old&&!d.count);
 CHECK(cold.captured&&!cold.reused&&cold.bindNodes==reference.nodeCount&&cold.projectNodes==reference.nodeCount);
 char* first=dump(xlsIrNativePipelineModule(b));CHECK(first&&!strcmp(first,expected));xrtFree(first);xlsIrNativePipelineReset(b);
 CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b,&d,old,16u*1024u*1024u,&next,&warm,&s)&&next&&!d.count);
 CHECK(warm.reused==cold.captured&&warm.proofChecks==warm.reused&&!warm.checked&&!warm.bindNodes&&!warm.projectNodes);
 CHECK(warm.restoredBinds==reference.nativeHandleBindCount&&warm.restoredProjects==reference.nativeHandleProjectCount);
 CHECK(warm.bytes==xlsIrHandleGroupBytes(next)&&warm.bytes<=16u*1024u*1024u);
 xlsIrHandleGroupDestroy(old);xlsIrHandleGroupDestroy(next);
 CHECK(xl6IrVerifyModule(xlsIrNativePipelineModule(b),&d));char* second=dump(xlsIrNativePipelineModule(b));CHECK(second&&!strcmp(second,expected));
 xls_ir_handle_group* ready=NULL;CHECK(xlsIrNativePipelineFinishCachedHandles(b,&d,NULL,1,&ready,NULL,&s)&&!ready);
 if(positive)printf("handle positive: binds=%zu projects=%zu sharedProofs=%llu\n",reference.nativeHandleBindCount,reference.nativeHandleProjectCount,(unsigned long long)warm.proofChecks);
 xrtFree(second);xrtFree(expected);xlsIrNativePipelineDestroy(b);xl6IrModuleFree(&reference);xl6DiagFree(&d);return 1;
}
static int handlePrepared(xls_ir_native_pipeline* b,const XL6_AstNode* root,XL6_DiagList* d,size_t bytes,xls_ir_pipeline_status* s)
{
 if(!faultActive&&!probePaused&&xrtAtomic32Load(&pipelineExpected,XMEMORY_RELAXED)>xrtAtomic32Load(&pipelineModels,XMEMORY_RELAXED)){
  CHECK(handlePair(root,0));xrtAtomic32FetchAdd(&handleModels,1,XMEMORY_RELAXED);
 }
 return xlsIrNativePipelinePrepare(b,root,d,bytes,s);
}
static int handleBudgets(const XL6_AstNode* root,const char* expected)
{
 const size_t limits[]={0,1,128,2048,8192,65536,16u*1024u*1024u};
 for(size_t i=0;i<sizeof(limits)/sizeof(*limits);++i){
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status s;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&s);xls_ir_handle_group* next=NULL;xls_ir_handle_group_work w;
  CHECK(b&&xlsIrNativePipelinePrepare(b,root,&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b,&d,NULL,limits[i],&next,&w,&s)&&!d.count&&w.bytes<=limits[i]);
  char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected));xrtFree(actual);xlsIrHandleGroupDestroy(next);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 return 1;
}
static int handleResources(const XL6_AstNode* root,const char* expected,int hot)
{
 xls_ir_handle_group* previous=NULL;xls_ir_pipeline_status s;XL6_DiagList d;xl6DiagInit(&d);xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&s);CHECK(b);
 if(hot){CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b,&d,NULL,16u*1024u*1024u,&previous,NULL,&s)&&previous);xlsIrNativePipelineReset(b);}
 CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&s));xls_ir_handle_group* next=NULL;faultActive=1;faultAt=faultCalls=denied=0;
 int okay=xlsIrNativePipelineFinishCachedHandles(b,&d,previous,16u*1024u*1024u,&next,NULL,&s);faultActive=0;CHECK(okay&&!d.count&&!denied&&!overflow);size_t count=faultCalls;
 xlsIrNativePipelineDestroy(b);xlsIrHandleGroupDestroy(next);xl6DiagFree(&d);CHECK(!live);
 for(size_t i=1;i<=count;++i){
  xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&s);CHECK(b&&xlsIrNativePipelinePrepare(b,root,&d,0,&s));next=NULL;faultActive=1;faultCalls=denied=0;faultAt=i;
  okay=xlsIrNativePipelineFinishCachedHandles(b,&d,previous,16u*1024u*1024u,&next,NULL,&s);faultActive=0;CHECK(denied==1&&!overflow);
  if(okay){char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected)&&!d.count);xrtFree(actual);}
  else CHECK(!next&&s!=XLS_PIPELINE_OK&&!xlsIrNativePipelineModule(b));
  xlsIrNativePipelineDestroy(b);xlsIrHandleGroupDestroy(next);xl6DiagFree(&d);CHECK(!live);
 }
 pipeline_stop stop={0};xls_frontend_cancel_scope scope;xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&s);CHECK(b&&xlsIrNativePipelinePrepare(b,root,&d,0,&s));next=NULL;
 xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);CHECK(xlsIrNativePipelineFinishCachedHandles(b,&d,previous,16u*1024u*1024u,&next,NULL,&s));xlsFrontendCancelEnd(&scope,NULL);size_t polls=stop.calls;
 xlsIrNativePipelineDestroy(b);xlsIrHandleGroupDestroy(next);xl6DiagFree(&d);
 for(size_t i=1;i<=polls;++i){
  xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&s);CHECK(b&&xlsIrNativePipelinePrepare(b,root,&d,0,&s));next=NULL;stop=(pipeline_stop){.at=i};
  faultActive=1;faultAt=faultCalls=denied=0;xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);okay=xlsIrNativePipelineFinishCachedHandles(b,&d,previous,16u*1024u*1024u,&next,NULL,&s);xls_frontend_cancel_work work;xlsFrontendCancelEnd(&scope,&work);faultActive=0;
  CHECK(!okay&&work.cancelled&&s==XLS_PIPELINE_CANCELLED&&!next&&!xlsIrNativePipelineModule(b));xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);CHECK(!live&&!overflow);
 }
 printf("handle resources hot%d: %zu allocation/API rejections;%zu cancellation points\n",hot,count,polls);xlsIrHandleGroupDestroy(previous);return 1;
}
static int handleGenerations(const char* path)
{
 xls_ir_handle_group* previous=NULL;
 for(unsigned edit=0;edit<4;++edit){
  xstrbuf text={0};if(edit>=2)CHECK(xrtStrBufAppend(&text,xrtStrView("// moved 中文\n\n")));
  CHECK(xrtStrBufAppend(&text,xrtStrView(handleSource)));
  if(edit){char* call=strstr(text.Data,"raw=makeBox");CHECK(call);memcpy(call+4,"makeAlt",7);}
  if(edit==3)CHECK(xrtStrBufAppend(&text,xrtStrView("func added(int value) as int\n return value+1\nend\n")));
  ++probePaused;xls_semantic_check* m=checked(path,text.Data);--probePaused;xrtStrBufFree(&text);CHECK(m&&xlsSemanticCheckInfo(m).accepted);
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status s;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&s);xls_ir_handle_group* next=NULL;xls_ir_handle_group_work w;
  CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(m),&d,0,&s)&&xlsIrNativePipelineFinishCachedHandles(b,&d,previous,16u*1024u*1024u,&next,&w,&s)&&next&&!d.count);
  if(edit==1)CHECK(w.checked&&w.reused&&w.bindNodes&&w.projectNodes&&w.bindNodes<xlsSemanticCheckIr(m)->nodeCount);
  if(edit==2)CHECK(w.reused&&!w.checked&&!w.bindNodes&&!w.projectNodes);
  if(edit==3)CHECK(w.checked&&!w.reused);
  const XL6_IrModule* actualModule=xlsIrNativePipelineModule(b);CHECK(actualModule->nativeHandleBindCount&&actualModule->nativeHandleProjectCount);
  char* expected=dump(xlsSemanticCheckIr(m));xlsSemanticCheckDestroy(m);xlsIrHandleGroupDestroy(previous);previous=next;
  char* actual=dump(actualModule);CHECK(expected&&actual&&!strcmp(expected,actual)&&xl6IrVerifyModule(actualModule,&d));xrtFree(expected);xrtFree(actual);
  printf("handle generation%u checked=%llu reused=%llu proofs=%llu bindNodes=%llu projectNodes=%llu nodes=%zu\n",edit,(unsigned long long)w.checked,(unsigned long long)w.reused,(unsigned long long)w.proofChecks,(unsigned long long)w.bindNodes,(unsigned long long)w.projectNodes,actualModule->nodeCount);
  xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 xlsIrHandleGroupDestroy(previous);return 1;
}
static int handleTests(void)
{
 CHECK(!xlsIrPipelineFixtureMain());CHECK(xrtAtomic32Load(&handleModels,XMEMORY_RELAXED)==49);
 char* path=xrtPathAbs("handle-cache-中文.xl");CHECK(path);++probePaused;xls_semantic_check* m=checked(path,handleSource);--probePaused;CHECK(m&&xlsSemanticCheckInfo(m).accepted);
 CHECK(handlePair(xlsSemanticCheckAst(m),1));char* expected=dump(xlsSemanticCheckIr(m));CHECK(expected);
 CHECK(handleBudgets(xlsSemanticCheckAst(m),expected));CHECK(handleResources(xlsSemanticCheckAst(m),expected,0));CHECK(handleResources(xlsSemanticCheckAst(m),expected,1));
 CHECK(handleGenerations(path));
 xrtFree(expected);xlsSemanticCheckDestroy(m);xrtFree(path);return 1;
}
#ifdef XLS_IR_HANDLE_GROUP_LIBRARY
int xlsIrHandleGroupFixtureMain(void)
#else
int main(void)
#endif
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_handle_group_test passed:component unavailable");return 0;}
 xrtAtomic32Init(&handleModels,0);if(!handleTests())return 1;
 printf("analysis_ir_handle_group_test passed: %u checks;49 full semantic inputs;shared two-phase proofs,positive bind/project,retired ownership,budgets,OOM,cancellation\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
