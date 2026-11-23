#include "analysis/frontend_internal.h"
#include "analysis/ir_native_name_cache.h"
static int namePrepared(xls_ir_native_pipeline*,const XL6_AstNode*,XL6_DiagList*,size_t,xls_ir_pipeline_status*);
#define xlsIrNativePipelinePrepare namePrepared
#define XLS_IR_PIPELINE_LIBRARY 1
#define __wrap_xrtArrayResize callBaseArrayResize
#include "analysis_ir_pipeline_test.c"
#undef __wrap_xrtArrayResize
#undef xlsIrNativePipelinePrepare
static xatomic32 cacheModels,eligibleModels;
static _Thread_local unsigned measureArrays,arrayResizes;
bool __wrap_xrtArrayResize(xarray* a,size_t n)
{if(measureArrays)++arrayResizes;return callBaseArrayResize(a,n);}
static int namePair(const XL6_AstNode* root,int countModel)
{
 XL6_IrModule reference;xl6IrModuleInit(&reference);XL6_DiagList d;xl6DiagInit(&d);
 CHECK(xlsFrontendIrBuildMemo(&reference,root,&d,NULL,NULL,NULL,0)&&!d.count);char* expected=dump(&reference);CHECK(expected);
 xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);
 int eligible=reference.classLayoutCount!=0;if(eligible&&countModel)xrtAtomic32FetchAdd(&eligibleModels,1,XMEMORY_RELAXED);
 xls_ir_name_cache *old=NULL,*next=NULL;xls_ir_name_cache_work cold,warm;
 CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b,&d,NULL,16u*1024u*1024u,&old,&cold,&status)&&!!old==eligible&&!d.count);
 char* first=dump(xlsIrNativePipelineModule(b));CHECK(first&&!strcmp(first,expected));xrtFree(first);xlsIrNativePipelineReset(b);
 CHECK(xlsIrNativePipelinePrepare(b,root,&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b,&d,old,16u*1024u*1024u,&next,&warm,&status)&&!!next==eligible&&!d.count);
 if(eligible)CHECK(warm.reused&&warm.nativeNodes<reference.nodeCount&&warm.bytes==xlsIrNameCacheBytes(next)&&warm.bytes<=16u*1024u*1024u);
 else CHECK(!warm.reused&&!warm.bytes&&!warm.captured&&warm.declined==1&&warm.nativeNodes==reference.nodeCount);
 xlsIrNameCacheDestroy(old);xlsIrNameCacheDestroy(next);CHECK(xl6IrVerifyModule(xlsIrNativePipelineModule(b),&d));char* second=dump(xlsIrNativePipelineModule(b));CHECK(second&&!strcmp(second,expected));
 xrtFree(second);xrtFree(expected);xlsIrNativePipelineDestroy(b);xl6IrModuleFree(&reference);xl6DiagFree(&d);return 1;
}
static int namePrepared(xls_ir_native_pipeline* b,const XL6_AstNode* root,XL6_DiagList* d,size_t bytes,xls_ir_pipeline_status* status)
{
 if(!faultActive&&!probePaused&&xrtAtomic32Load(&pipelineExpected,XMEMORY_RELAXED)>xrtAtomic32Load(&pipelineModels,XMEMORY_RELAXED)){
  CHECK(namePair(root,1));xrtAtomic32FetchAdd(&cacheModels,1,XMEMORY_RELAXED);
 }
 return xlsIrNativePipelinePrepare(b,root,d,bytes,status);
}
static char* nameSource(unsigned edit)
{
 xstrbuf b={0};
 if(edit==3&&!xrtStrBufAppend(&b,xrtStrView("// moved source 中文\n\n")))goto failed;
 if(!xrtStrBufAppend(&b,xrtStrView("class Anchor\n int item=1\n func read() as int\n return item\n end\nend\n")))goto failed;
 if(!xrtStrBufAppendFormat(&b,"func targetA(int value,int extra=%u) as int\n return value+extra\nend\nfunc targetB(int value) as int\n return value+2\nend\n",edit==2?8:4))goto failed;
 for(unsigned i=0;i<16;++i)if(!xrtStrBufAppendFormat(&b,"func f%u(int value) as int\n int result=targetA(value)\n return %s+%u\nend\n",i,i==4&&(edit==1||edit==3)?"value":"result",i))goto failed;
 return xrtStrBufTake(&b);
failed:xrtStrBufFree(&b);return NULL;
}
static int generations(const char* path)
{
 xls_ir_name_cache* previous=NULL;uint64_t restored=0;
 for(unsigned generation=0;generation<6;++generation){
  char* source=nameSource(generation==5?3:generation%3);CHECK(source);++probePaused;xls_semantic_check* model=checked(path,source);--probePaused;xrtFree(source);CHECK(model&&xlsSemanticCheckInfo(model).accepted);
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;
  CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
  CHECK(xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&status==XLS_PIPELINE_OK&&!d.count&&next);
  xlsIrNameCacheDestroy(previous);previous=next;
  if(generation){
   CHECK(work.reused&&work.restoredNodes&&work.nativeNodes<xlsSemanticCheckIr(model)->nodeCount);
   if(generation==3||generation==5)CHECK(!work.checked&&!work.nativeNodes);else CHECK(work.checked);
   restored+=work.restoredNodes;
  }
  char* expected=dump(xlsSemanticCheckIr(model));xlsSemanticCheckDestroy(model);char* actual=dump(xlsIrNativePipelineModule(b));CHECK(expected&&actual&&!strcmp(expected,actual));
  CHECK(xl6IrVerifyModule(xlsIrNativePipelineModule(b),&d));xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
  printf("name generation%u checked=%llu reused=%llu nativeNodes=%llu restoredNodes=%llu\n",generation,(unsigned long long)work.checked,(unsigned long long)work.reused,(unsigned long long)work.nativeNodes,(unsigned long long)work.restoredNodes);
 }
 xlsIrNameCacheDestroy(previous);CHECK(restored>0);return 1;
}
static int typeIdChanges(const char* path)
{
 xls_ir_name_cache* previous=NULL;XL6_IrTypeId first=0;
 for(unsigned edit=0;edit<2;++edit){
  char* source=nameSource(0);CHECK(source);if(edit){char* f=strstr(source,"func f4(");CHECK(f);char* target=strstr(f,"targetA(");CHECK(target);target[6]='B';}
  ++probePaused;xls_semantic_check* model=checked(path,source);--probePaused;xrtFree(source);CHECK(model&&xlsSemanticCheckInfo(model).accepted);
  const XL6_IrModule* reference=xlsSemanticCheckIr(model);CHECK(reference->classLayoutCount);if(!edit)first=reference->classLayouts[0].type;else CHECK(first!=reference->classLayouts[0].type);
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;
  CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&next&&!d.count);
  CHECK(work.checked&&!work.reused&&work.nativeNodes==reference->nodeCount);
  char* expected=dump(reference);xlsSemanticCheckDestroy(model);xlsIrNameCacheDestroy(previous);previous=next;char* actual=dump(xlsIrNativePipelineModule(b));CHECK(expected&&actual&&!strcmp(expected,actual));
  xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 xlsIrNameCacheDestroy(previous);return 1;
}
static const char* nameResourceSource="class ResourceBox\n string item=\"owned\"\n func value() as string\n string result=item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n result=result+item\n return result\n end\nend\nfunc run(ResourceBox value) as string\n return value.value()\nend\n";


static int cachedResources(const char* path,int hot)
{
 ++probePaused;xls_semantic_check* model=checked(path,nameResourceSource);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 char* expected=dump(xlsSemanticCheckIr(model));CHECK(expected);xls_ir_name_cache* previous=NULL;xls_ir_pipeline_status status;
 XL6_DiagList d;xl6DiagInit(&d);xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);
 if(hot){CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b,&d,NULL,16u*1024u*1024u,&previous,NULL,&status)&&previous);xlsIrNativePipelineReset(b);}
 CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
 xls_ir_name_cache* next=NULL;faultActive=1;faultAt=faultCalls=denied=0;
 int okay=xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,NULL,&status);faultActive=0;CHECK(okay&&!d.count&&!denied&&!overflow);size_t allocationsCount=faultCalls;
 xlsIrNativePipelineDestroy(b);xlsIrNameCacheDestroy(next);xl6DiagFree(&d);CHECK(!live);
 for(size_t i=1;i<=allocationsCount;++i){
  xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));next=NULL;
  faultActive=1;faultCalls=denied=0;faultAt=i;okay=xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,NULL,&status);faultActive=0;CHECK(denied==1&&!overflow);
  if(okay){char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected)&&!d.count);xrtFree(actual);}
  else CHECK(!next&&status!=XLS_PIPELINE_OK&&!xlsIrNativePipelineModule(b));
  xlsIrNativePipelineDestroy(b);xlsIrNameCacheDestroy(next);xl6DiagFree(&d);CHECK(!live);
 }
 pipeline_stop stop={0};xls_frontend_cancel_scope scope;xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));next=NULL;
 xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);CHECK(xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,NULL,&status));xlsFrontendCancelEnd(&scope,NULL);size_t polls=stop.calls;xlsIrNativePipelineDestroy(b);xlsIrNameCacheDestroy(next);xl6DiagFree(&d);
 for(size_t i=1;i<=polls;++i){
  xl6DiagInit(&d);b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));next=NULL;stop=(pipeline_stop){.at=i};
  faultActive=1;faultAt=faultCalls=denied=0;xlsFrontendCancelBegin(&scope,pipelineCancel,&stop);okay=xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,NULL,&status);xls_frontend_cancel_work work;xlsFrontendCancelEnd(&scope,&work);faultActive=0;
  CHECK(!okay&&work.cancelled&&status==XLS_PIPELINE_CANCELLED&&!next&&!xlsIrNativePipelineModule(b));xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);CHECK(!live&&!overflow);
 }
 printf("name-cache resources hot%d: %zu allocation/API rejection points;%zu cancellation points\n",hot,allocationsCount,polls);
 xlsIrNameCacheDestroy(previous);xlsSemanticCheckDestroy(model);xrtFree(expected);return 1;
}
static int budgets(const char* path)
{
 char* text=nameSource(0);CHECK(text);++probePaused;xls_semantic_check* model=checked(path,text);--probePaused;xrtFree(text);CHECK(model&&xlsSemanticCheckInfo(model).accepted);
 unsigned baseline=0;
 for(unsigned i=0;i<2;++i){
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
  xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;arrayResizes=0;measureArrays=1;
  int okay=xlsIrNativePipelineFinishCachedNames(b,&d,NULL,i?128:0,&next,&work,&status);measureArrays=0;
  CHECK(okay&&!next&&!d.count&&!work.bytes);
  if(!i)baseline=arrayResizes;else CHECK(arrayResizes==baseline&&work.declined&&work.nativeNodes==xlsSemanticCheckIr(model)->nodeCount);
  char *expected=dump(xlsSemanticCheckIr(model)),*actual=dump(xlsIrNativePipelineModule(b));CHECK(expected&&actual&&!strcmp(expected,actual));xrtFree(expected);xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 xlsSemanticCheckDestroy(model);return 1;
}
static int members(const char* path)
{
 xls_ir_name_cache* previous=NULL;
 for(unsigned edit=0;edit<4;++edit){
  xstrbuf text={0};if(edit==3)CHECK(xrtStrBufAppend(&text,xrtStrView("// source moved 中文\n\n")));
  CHECK(xrtStrBufAppendFormat(&text,"class ImplicitBox\n string item=\"%s\"\n string other=\"other\"\n func value() as string\n return %s\n end\nend\n",edit>=2?"fresh":"owned",edit?"other":"item"));
  for(unsigned i=0;i<16;++i)CHECK(xrtStrBufAppendFormat(&text,"func stable%u(int value) as int\n return value+%u\nend\n",i,i));
  ++probePaused;xls_semantic_check* model=checked(path,text.Data);--probePaused;xrtStrBufFree(&text);CHECK(model&&xlsSemanticCheckInfo(model).accepted);
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b);xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work;
  CHECK(xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status)&&xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&next,&work,&status)&&next&&!d.count);
  const XL6_IrModule* m=xlsIrNativePipelineModule(b);size_t positive=0;for(size_t i=0;i<m->nodeCount;++i)positive+=m->nodes[i].implicitNameAbi!=XL6_IR_IMPLICIT_NAME_NONE;CHECK(positive);
  if(edit)CHECK(work.reused&&work.restoredNodes&&work.nativeNodes<m->nodeCount);
  if(edit>=2)CHECK(!work.checked&&!work.nativeNodes);
  char* expected=dump(xlsSemanticCheckIr(model));xlsSemanticCheckDestroy(model);xlsIrNameCacheDestroy(previous);previous=next;
  char* actual=dump(m);CHECK(expected&&actual&&!strcmp(expected,actual)&&xl6IrVerifyModule(m,&d));xrtFree(expected);xrtFree(actual);
  printf("name member-edit%u checked=%llu reused=%llu nativeNodes=%llu totalNodes=%zu positive=%zu\n",edit,(unsigned long long)work.checked,(unsigned long long)work.reused,(unsigned long long)work.nativeNodes,m->nodeCount,positive);
  xls_ir_name_cache* ready=NULL;CHECK(xlsIrNativePipelineFinishCachedNames(b,&d,previous,16u*1024u*1024u,&ready,NULL,&status)&&!ready);
  xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);
 }
 xlsIrNameCacheDestroy(previous);return 1;
}

static int withoutClasses(const char* path)
{
 const char* text="func plain(int value) as int\n return value+1\nend\n";
 ++probePaused;xls_semantic_check* model=checked(path,text);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted&&!xlsSemanticCheckIr(model)->classLayoutCount);
 char* expected=dump(xlsSemanticCheckIr(model));CHECK(expected);size_t baseline=0;
 for(unsigned mode=0;mode<2;++mode){
  XL6_DiagList d;xl6DiagInit(&d);xls_ir_pipeline_status status;xls_ir_native_pipeline* b=xlsIrNativePipelineCreate(&status);CHECK(b&&xlsIrNativePipelinePrepare(b,xlsSemanticCheckAst(model),&d,0,&status));
  xls_ir_name_cache* next=NULL;xls_ir_name_cache_work work={0};faultAt=faultCalls=denied=0;faultActive=1;
  int okay=mode?xlsIrNativePipelineFinishCachedNames(b,&d,NULL,16u*1024u*1024u,&next,&work,&status):xlsIrNativePipelineFinish(b,&d,&status);faultActive=0;
  CHECK(okay&&!d.count&&!denied&&!overflow&&!next);
  if(!mode)baseline=faultCalls;else CHECK(faultCalls==baseline&&work.declined==1&&!work.checked&&!work.bytes&&work.nativeNodes==xlsSemanticCheckIr(model)->nodeCount);
  char* actual=dump(xlsIrNativePipelineModule(b));CHECK(actual&&!strcmp(actual,expected));xrtFree(actual);xlsIrNativePipelineDestroy(b);xl6DiagFree(&d);CHECK(!live);
 }
 xlsSemanticCheckDestroy(model);xrtFree(expected);return 1;
}
static int positiveFamilies(const char* path)
{
 const char* source[]={
  "class Parent\n func value() as int\n return 1\n end\nend\nclass Child extends Parent\n func read() as int\n return base.value()\n end\nend\n",
  "class Box<T>\n T item\n func read() as T\n return item\n end\nend\nfunc get(Box<int> box) as int\n return box.read()\nend\n"
 };
 for(unsigned c=0;c<2;++c){
  ++probePaused;xls_semantic_check* model=checked(path,source[c]);--probePaused;CHECK(model&&xlsSemanticCheckInfo(model).accepted);
  const XL6_IrModule* m=xlsSemanticCheckIr(model);size_t positive=0;
  for(size_t i=0;i<m->nodeCount;++i)positive+=m->nodes[i].implicitNameAbi==(c?XL6_IR_IMPLICIT_NAME_CLASS_FIELD:XL6_IR_IMPLICIT_NAME_BASE_RECEIVER_VIEW);
  CHECK(positive&&namePair(xlsSemanticCheckAst(model),0));xlsSemanticCheckDestroy(model);
 }
 return 1;
}

static int tests(void)
{
 CHECK(!xlsIrPipelineFixtureMain());CHECK(xrtAtomic32Load(&cacheModels,XMEMORY_RELAXED)==49);
 char* path=xrtPathAbs("name-cache-generations-中文.xl");CHECK(path);CHECK(generations(path));CHECK(typeIdChanges(path));CHECK(members(path));CHECK(withoutClasses(path));CHECK(positiveFamilies(path));CHECK(cachedResources(path,0));CHECK(cachedResources(path,1));CHECK(budgets(path));xrtFree(path);return 1;
}
#ifdef XLS_IR_NAME_CACHE_LIBRARY
int xlsIrNameCacheFixtureMain(void)
#else
int main(void)
#endif
{
 if(!xlsIrNativePipelineAvailable()){puts("analysis_ir_name_cache_test passed:component unavailable");return 0;}
 xrtAtomic32Init(&cacheModels,0);xrtAtomic32Init(&eligibleModels,0);if(!tests())return 1;
 printf("analysis_ir_name_cache_test passed: %u checks;%u full semantic inputs;%u eligible class inputs;actual cross-generation name production,retired source/cache ownership\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED),xrtAtomic32Load(&cacheModels,XMEMORY_RELAXED),xrtAtomic32Load(&eligibleModels,XMEMORY_RELAXED));return 0;
}
