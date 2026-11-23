#include "analysis/ir_function_exit.h"
static size_t functionLimit;
static void functionReferenceScope(int entering){xlsIrFunctionExitLimit(entering?0:functionLimit);}
#define XLS_REFERENCE_CHECK_SCOPE functionReferenceScope
#define XLS_IR_EXIT_RANGE_LIBRARY 1
#include "analysis_ir_exit_range_test.c"
#include "analysis/ir_function_exit.h"
static xls_ir_function_exit_work functionWork;
static size_t observedFunctionBuilds,observedFunctionReuses;
static const char* functionFailedApi;
bool __real_xrtArrayPush(xarray*,const void*);bool __real_xrtArrayResize(xarray*,size_t);
bool __real_xrtMapSet(xmap*,xbytesview,const void*);bool __real_xrtStrBufReserve(xstrbuf*,size_t);
bool __wrap_xrtArrayPush(xarray* a,const void* p){if(refuse()){functionFailedApi="ArrayPush";return false;}return __real_xrtArrayPush(a,p);}
bool __wrap_xrtArrayResize(xarray* a,size_t n){if(refuse()){functionFailedApi="ArrayResize";return false;}return __real_xrtArrayResize(a,n);}
bool __wrap_xrtMapSet(xmap* m,xbytesview k,const void* v){if(refuse()){functionFailedApi="MapSet";return false;}return __real_xrtMapSet(m,k,v);}
bool __wrap_xrtStrBufReserve(xstrbuf* b,size_t n){if(refuse()){functionFailedApi="StrBufReserve";return false;}return __real_xrtStrBufReserve(b,n);}
int __real_xlsFrontendIrBuildMemo(XL6_IrModule*,const XL6_AstNode*,XL6_DiagList*,const xls_ir_memo*,xls_ir_memo**,xls_ir_memo_work*,size_t);
int __wrap_xlsFrontendIrBuildMemo(XL6_IrModule* m,const XL6_AstNode* root,XL6_DiagList* diags,const xls_ir_memo* old,xls_ir_memo** next,xls_ir_memo_work* work,size_t bytes)
{
 int okay=__real_xlsFrontendIrBuildMemo(m,root,diags,old,next,work,bytes);
 if(next&&*next){xls_ir_function_exit_work s=xlsIrFunctionExitWork(*next);if(s.assembled){++observedFunctionBuilds;observedFunctionReuses+=s.reused;}}
 return okay;
}
static int functionCompare(const char* path,const char* text,const xls_ir_memo* previous,xls_ir_memo** next)
{
 xlsIrFunctionExitLimit(0);xls_semantic_check* ref=checked(path,text);CHECK(ref&&xlsSemanticCheckInfo(ref).accepted);
 char* expected=captureDump(xlsSemanticCheckIr(ref));CHECK(expected);xlsIrFunctionExitLimit(8u*1024u*1024u);
 XL6_IrModule module;xl6IrModuleInit(&module);XL6_DiagList diags;xl6DiagInit(&diags);xls_ir_memo_work raw;
 int accepted=xlsFrontendIrBuildMemo(&module,xlsSemanticCheckAst(ref),&diags,previous,next,&raw,32u*1024u*1024u);
 if(!accepted||diags.count)for(size_t i=0;i<diags.count;++i)fprintf(stderr,"function producer diagnostic: %s\n",diags.items[i].message);
 CHECK(accepted&&!diags.count&&*next);
 functionWork=xlsIrFunctionExitWork(*next);
 CHECK(functionWork.assembled&&!functionWork.fallbackNodes&&functionWork.bytes<=8u*1024u*1024u);
 xlsSemanticCheckDestroy(ref);CHECK(verifyCapture(&module,expected));xrtFree(expected);xl6DiagFree(&diags);xl6IrModuleFree(&module);return 1;
}
static const char* functionTail="func stable(int count) as int\n string value=\"outer\"\n try\n while count>0\n string item=\"item\"\n count=count-1\n if count>2\n continue\n end\n break\n end\n return count+2\n finally\n string final=\"final\"\n end\nend\nfunc main(...) as int\n return stable(4)\nend\n";
static int functionGenerations(void)
{
 char* path=xrtPathAbs("function-exit-中文.xl");CHECK(path);xls_ir_memo* old=NULL;
 for(size_t gen=0;gen<32;++gen){xstrbuf text={0};CHECK(xrtStrBufAppend(&text,xrtStrView("func changed(int x) as int\n")));
  for(size_t i=0;i<=gen;++i){char row[96];snprintf(row,sizeof(row)," if x>%zu\n int local%zu=x+%zu\n end\n",i,i,i);CHECK(xrtStrBufAppend(&text,xrtStrView(row)));}
  CHECK(xrtStrBufAppend(&text,xrtStrView(" return x\nend\n"))&&xrtStrBufAppend(&text,xrtStrView(functionTail)));
  xls_ir_memo* next=NULL;CHECK(functionCompare(path,text.Data,old,&next));xlsFrontendIrMemoDestroy(old);old=next;xrtStrBufFree(&text);
  printf("function generation%zu recorded=%zu reused=%zu fresh=%zu native=%zu/%zu bytes=%zu\n",gen,functionWork.recorded,functionWork.reused,functionWork.fresh,functionWork.nativeNodes,functionWork.moduleNodes,functionWork.bytes);
  if(gen)CHECK(functionWork.reused>=2&&functionWork.fresh==1&&functionWork.nativeNodes<functionWork.moduleNodes);
  CHECK(functionWork.bytes<128u*1024u);
 }
 xlsFrontendIrMemoDestroy(old);xrtFree(path);return 1;
}
static int functionLiteral(void)
{
 const char* a="func sum(int x) as int\n string name=\"😀first\"\n return x+1\nend\n";
 const char* b="func sum(int x) as int\n string name=\"中文later\"\n return x+9\nend\n";
 char* path=xrtPathAbs("function-exit-literal.xl");CHECK(path);xls_ir_memo *old=NULL,*next=NULL;
 CHECK(functionCompare(path,a,NULL,&old)&&functionCompare(path,b,old,&next));CHECK(functionWork.reused==1&&!functionWork.fresh&&functionWork.nativeNodes==1);
 xlsFrontendIrMemoDestroy(old);xlsFrontendIrMemoDestroy(next);xrtFree(path);return 1;
}
static int functionCorpus(void)
{
 const char* cases[]={
 "func guarded() as int\n string outer=\"outer\"\n try\n return 7\n catch e\n string message=e.message\n return 8\n finally\n string final=\"final\"\n end\nend\n",
 "func values(int count) as generator<string>\n string suffix=\"owned\"\n for i in range(count)\n string item=suffix+string(i)\n yield item\n end\nend\n",
 "enum Choice\n Some(string value)\n None\nend\nfunc chosen(Choice item) as string\n match item\n case Some(value)\n return value\n case None\n return \"empty\"\n end\nend\n",
 "func closure() as func() as int\n array values=[41]\n return func() as int\n return int(values[0])\n end\nend\n"};
 char* path=xrtPathAbs("function-exit-shapes.xl");CHECK(path);
 for(size_t i=0;i<sizeof(cases)/sizeof(*cases);++i){xls_ir_memo *old=NULL,*next=NULL;CHECK(functionCompare(path,cases[i],NULL,&old)&&functionCompare(path,cases[i],old,&next));CHECK(functionWork.reused>=1&&!functionWork.fresh);xlsFrontendIrMemoDestroy(old);xlsFrontendIrMemoDestroy(next);}
 xrtFree(path);return 1;
}
static int functionSharedChanges(void)
{
 const char* a="enum Choice\n Some(string value)\n None\nend\nfunc use(Choice item) as int\n return 1\nend\n";
 const char* b="enum Choice\n Some(string value)\n None\n More(int value)\nend\nfunc use(Choice item) as int\n return 1\nend\n";
 char* path=xrtPathAbs("function-exit-shared.xl");CHECK(path);xls_ir_memo *old=NULL,*next=NULL;
 CHECK(functionCompare(path,a,NULL,&old)&&functionCompare(path,b,old,&next));CHECK(!functionWork.reused&&functionWork.fresh==1);
 xlsFrontendIrMemoDestroy(old);xlsFrontendIrMemoDestroy(next);
 a="func sum(int x) as int\n return x\nend\n";b="func sum(int x) as string\n return string(x)\nend\n";
 old=next=NULL;CHECK(functionCompare(path,a,NULL,&old)&&functionCompare(path,b,old,&next));CHECK(!functionWork.reused&&functionWork.fresh==1);
 xlsFrontendIrMemoDestroy(old);xlsFrontendIrMemoDestroy(next);xrtFree(path);return 1;
}
static int functionResources(void)
{
 char* path=xrtPathAbs("function-exit-faults.xl");CHECK(path);xls_semantic_check* ref=checked(path,functionTail);CHECK(ref&&xlsSemanticCheckInfo(ref).accepted);
 const XL6_AstNode* root=xlsSemanticCheckAst(ref);char* expected=captureDump(xlsSemanticCheckIr(ref));CHECK(expected);
 xls_ir_memo* old=NULL;CHECK(functionCompare(path,functionTail,NULL,&old));
 for(unsigned hot=0;hot<2;++hot){
  XL6_IrModule m;XL6_DiagList diags;xls_ir_memo* next=NULL;xls_ir_memo_work raw;
  xl6IrModuleInit(&m);xl6DiagInit(&diags);faultActive=1;faultAt=faultCalls=denied=0;
  int accepted=xlsFrontendIrBuildMemo(&m,root,&diags,hot?old:NULL,&next,&raw,32u*1024u*1024u);faultActive=0;
  CHECK(accepted&&next&&!denied&&!overflow);size_t count=faultCalls;xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&m);xl6DiagFree(&diags);CHECK(!live&&!liveBytes);
  for(size_t at=1;at<=count;++at){
   xl6IrModuleInit(&m);xl6DiagInit(&diags);next=NULL;faultActive=1;faultAt=at;faultCalls=denied=0;functionFailedApi="malloc/calloc/realloc";
   accepted=xlsFrontendIrBuildMemo(&m,root,&diags,hot?old:NULL,&next,&raw,32u*1024u*1024u);faultActive=0;
   CHECK(denied==1&&!overflow);if(accepted){
    char* actual=captureDump(&m);CHECK(actual);
    if(strcmp(actual,expected)){
     fprintf(stderr,"function fault mismatch hot=%u at=%zu api=%s\n",hot,at,functionFailedApi);
     CHECK(xrtFileWriteAtomic("function-fault-actual.json",(xbytesview){(const unsigned char*)actual,strlen(actual)}));
     CHECK(xrtFileWriteAtomic("function-fault-expected.json",(xbytesview){(const unsigned char*)expected,strlen(expected)}));
    }
    xrtFree(actual);CHECK(verifyCapture(&m,expected));
   }else CHECK(!next);
   xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&m);xl6DiagFree(&diags);CHECK(!live&&!liveBytes);
  }
  capture_stop stop={0};xls_frontend_cancel_scope scope;
  xl6IrModuleInit(&m);xl6DiagInit(&diags);next=NULL;xlsFrontendCancelBegin(&scope,captureCancelled,&stop);
  accepted=xlsFrontendIrBuildMemo(&m,root,&diags,hot?old:NULL,&next,&raw,32u*1024u*1024u);xlsFrontendCancelEnd(&scope,NULL);CHECK(accepted&&next);
  size_t polls=stop.calls;xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&m);xl6DiagFree(&diags);
  for(size_t at=1;at<=polls;++at){
   xl6IrModuleInit(&m);xl6DiagInit(&diags);next=NULL;stop=(capture_stop){.at=at};faultActive=1;faultAt=faultCalls=denied=0;
   xlsFrontendCancelBegin(&scope,captureCancelled,&stop);
   (void)xlsFrontendIrBuildMemo(&m,root,&diags,hot?old:NULL,&next,&raw,32u*1024u*1024u);
   xls_frontend_cancel_work cancelWork;xlsFrontendCancelEnd(&scope,&cancelWork);faultActive=0;
   CHECK(cancelWork.cancelled&&!next);xl6IrModuleFree(&m);xl6DiagFree(&diags);CHECK(!live&&!liveBytes&&!overflow);
  }
  printf("function %s resources: %zu allocation/XRT API rejections,%zu cancellation points; original fallback/complete output preserved\n",hot?"hot":"cold",count,polls);
 }
 const size_t limits[]={0,1,256,4096,8192};
 for(size_t i=0;i<sizeof(limits)/sizeof(*limits);++i){
  XL6_IrModule m;XL6_DiagList diags;xls_ir_memo* next=NULL;xls_ir_memo_work raw;xl6IrModuleInit(&m);xl6DiagInit(&diags);xlsIrFunctionExitLimit(limits[i]);
  CHECK(xlsFrontendIrBuildMemo(&m,root,&diags,old,&next,&raw,32u*1024u*1024u)&&next&&verifyCapture(&m,expected));
  CHECK(xlsIrFunctionExitWork(next).bytes<=limits[i]);xlsFrontendIrMemoDestroy(next);xl6IrModuleFree(&m);xl6DiagFree(&diags);
 }
 xlsIrFunctionExitLimit(functionLimit);xlsFrontendIrMemoDestroy(old);xlsSemanticCheckDestroy(ref);xrtFree(expected);xrtFree(path);return 1;
}
static int functionTests(void)
{
 xlsIrFunctionExitLimit(0);if(!rangeTests())return 0;
 if(!xlsIrFunctionExitAvailable()){puts("PASS function exit reuse is disabled in this profile");return 1;}
 functionLimit=8u*1024u*1024u;xlsIrFunctionExitLimit(functionLimit);
 CHECK(functionLiteral()&&functionGenerations()&&functionCorpus()&&functionSharedChanges());
 char* path=xrtPathAbs("function-exit-full-corpus.xl");CHECK(path);size_t before=observedFunctionBuilds,hits=observedFunctionReuses;
 CHECK(corpus(path)&&regionCorpus(path));xrtFree(path);
 CHECK(observedFunctionBuilds-before>=20&&observedFunctionReuses-hits>=8);
 printf("function corpus observed: %zu selective builds,%zu reused function units; independent full original references\n",observedFunctionBuilds-before,observedFunctionReuses-hits);
 CHECK(functionResources());
 printf("analysis_ir_function_exit_test passed: %u checks;actual raw construction maps,32structural generations,full native/JSON oracles\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 1;
}
#ifdef XLS_IR_FUNCTION_EXIT_LIBRARY
int xlsIrFunctionExitFixtureMain(void){return functionTests()?0:1;}
#else
int main(void){return functionTests()?0:1;}
#endif
