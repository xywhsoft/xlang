#define main priorMemoMain
#include "analysis_ir_memo_test.c"
#undef main
#include "analysis/frontend_ir_exit.h"
#if XLS_EXPERIMENTAL_IR_EXIT_REUSE
typedef struct xls_exit_test_work {uint64_t built,reused,declined,originals,keyBytes,outputBytes;}xls_exit_test_work;
extern void xlsExitTestReset(void);extern xls_exit_test_work xlsExitTestWork(void);extern void xlsExitTestLimit(size_t);
static int exitPairs(void)
{
 const char* a="func sum(int x) as int\n string name=\"outer\"\n try\n if x>1\n return x+3\n end\n return 4\n finally\n string final=\"final\"\n end\nend\nfunc main(...) as int\n return sum(5)\nend\n";
 const char* b="func sum(int x) as int\n string name=\"later value\"\n try\n if x>2\n return x+6\n end\n return 7\n finally\n string final=\"last\"\n end\nend\nfunc main(...) as int\n return sum(8)\nend\n";
 char* path=xrtPathAbs("exit-input-reuse.xl");CHECK(path);xls_semantic_check* old=checked(path,a);xls_semantic_check* current=checked(path,b);CHECK(old&&current&&xlsSemanticCheckInfo(old).accepted&&xlsSemanticCheckInfo(current).accepted);xls_ir_memo *prior=NULL,*next=NULL;CHECK(compare(old,NULL,&prior,32u*1024u*1024u,0));xlsSemanticCheckDestroy(old);xlsExitTestReset();CHECK(compare(current,prior,&next,32u*1024u*1024u,0));xls_exit_test_work work=xlsExitTestWork();printf("exit proof built=%llu reused=%llu originals=%llu declined=%llu keyBytes=%llu\n",(unsigned long long)work.built,(unsigned long long)work.reused,(unsigned long long)work.originals,(unsigned long long)work.declined,(unsigned long long)work.keyBytes);CHECK(work.reused==1&&!work.originals);xlsFrontendIrMemoDestroy(prior);prior=NULL;CHECK(compare(current,next,&prior,32u*1024u*1024u,0));xlsFrontendIrMemoDestroy(next);xlsFrontendIrMemoDestroy(prior);xlsSemanticCheckDestroy(current);xrtFree(path);return 1;
}
static int largeExitPairs(void)
{
 xstrbuf first={0},second={0};for(size_t i=0;i<512;++i){char text[256];snprintf(text,sizeof(text),"func f%zu(int x) as int\n string v=\"body%zu\"\n return x+%zu\nend\n",i,i,i+1);CHECK(xrtStrBufAppend(&first,xrtStrView(text)));snprintf(text,sizeof(text),"func f%zu(int x) as int\n string v=\"body%zu\"\n return x+%zu\nend\n",i,i,i+2);CHECK(xrtStrBufAppend(&second,xrtStrView(text)));}CHECK(xrtStrBufAppend(&first,xrtStrView("func main(...) as int\n return f0(2)\nend\n"))&&xrtStrBufAppend(&second,xrtStrView("func main(...) as int\n return f0(3)\nend\n")));char* path=xrtPathAbs("exit-large.xl");CHECK(path);xls_semantic_check* a=checked(path,first.Data);xls_semantic_check* b=checked(path,second.Data);CHECK(a&&b&&xlsSemanticCheckInfo(a).accepted&&xlsSemanticCheckInfo(b).accepted);xls_ir_memo *old=NULL,*next=NULL;CHECK(compare(a,NULL,&old,32u*1024u*1024u,0));xlsSemanticCheckDestroy(a);xlsExitTestReset();CHECK(compare(b,old,&next,32u*1024u*1024u,0));xls_exit_test_work work=xlsExitTestWork();printf("512 functions exit reused=%llu originals=%llu declined=%llu keyBytes=%llu\n",(unsigned long long)work.reused,(unsigned long long)work.originals,(unsigned long long)work.declined,(unsigned long long)work.keyBytes);CHECK(work.reused==1&&!work.originals);xlsFrontendIrMemoDestroy(old);xlsFrontendIrMemoDestroy(next);xlsSemanticCheckDestroy(b);xrtFree(path);xrtStrBufFree(&first);xrtStrBufFree(&second);return 1;
}
extern int xlsExitTestKey(const XL6_IrModule*,xstrbuf*,size_t);
static int keyEqual(const XL6_IrModule* a,const XL6_IrModule* b,int equal)
{xstrbuf x={0},y={0};CHECK(xlsExitTestKey(a,&x,1024u*1024u)&&xlsExitTestKey(b,&y,1024u*1024u));CHECK((x.Size==y.Size&&!memcmp(x.Data,y.Data,x.Size))==equal);xrtStrBufFree(&x);xrtStrBufFree(&y);return 1;}
static int keyPredicates(void)
{
 XL6_IrNode nodes[2]={{.kind=XL6_IR_NODE_NAME,.text="alpha"},{.kind=XL6_IR_NODE_NAME,.text="beta"}};XL6_IrModule a={.root=1,.nodes=nodes,.nodeCount=1,.nodeCapacity=1},b=a;b.nodes=nodes+1;CHECK(keyEqual(&a,&b,1));const char* values[]={NULL,"","_","destroy","other","another","中文"};
 for(size_t i=0;i<7;++i)for(size_t j=0;j<7;++j){nodes[0].text=(char*)values[i];nodes[1].text=(char*)values[j];CHECK(keyEqual(&a,&b,i==j||(i>=4&&j>=4)));}
 nodes[0]=(XL6_IrNode){.kind=XL6_IR_NODE_LITERAL,.text="12",.literalScalar=12,.span={1,2}};nodes[1]=(XL6_IrNode){.kind=XL6_IR_NODE_LITERAL,.text="34",.literalScalar=34,.span={77,88},.generatorClosePointAbi=1};CHECK(keyEqual(&a,&b,1));nodes[1].type=1;CHECK(keyEqual(&a,&b,0));nodes[1].type=0;nodes[1].flags=1;CHECK(keyEqual(&a,&b,1));nodes[1].flags=0;nodes[1].region=3;CHECK(keyEqual(&a,&b,0));nodes[1]=nodes[0];
 XL6_IrValueInitializer init[2]={{.errorAbi=XL6_IR_ERROR_ABI_INVALID},{.errorAbi=XL6_IR_ERROR_ABI_MAY_RAISE}};a.valueInitializers=init;b.valueInitializers=init+1;a.valueInitializerCount=a.valueInitializerCapacity=b.valueInitializerCount=b.valueInitializerCapacity=1;CHECK(keyEqual(&a,&b,0));return 1;
}
static int renamedPair(void)
{
 const char* a="func work(int x) as int\n string first=\"old\"\n return x+2\nend\nfunc main(...) as int\n return work(3)\nend\n";const char* b="func work(int y) as int\n string renamed=\"new\"\n return y+4\nend\nfunc main(...) as int\n return work(5)\nend\n";char* path=xrtPathAbs("exit-renamed.xl");CHECK(path);xls_semantic_check* old=checked(path,a);xls_semantic_check* current=checked(path,b);CHECK(old&&current&&xlsSemanticCheckInfo(old).accepted&&xlsSemanticCheckInfo(current).accepted);xls_ir_memo *prior=NULL,*next=NULL;CHECK(compare(old,NULL,&prior,32u*1024u*1024u,0));xlsSemanticCheckDestroy(old);xlsExitTestReset();CHECK(compare(current,prior,&next,32u*1024u*1024u,0));xls_exit_test_work work=xlsExitTestWork();printf("renamed source exit reused=%llu originals=%llu\n",(unsigned long long)work.reused,(unsigned long long)work.originals);CHECK(work.reused==1&&!work.originals);xlsFrontendIrMemoDestroy(prior);xlsFrontendIrMemoDestroy(next);xlsSemanticCheckDestroy(current);xrtFree(path);return 1;
}
static int exitMisses(void)
{
 const char* first="func work(int x) as int\n string value=\"a\"\n return x+2\nend\nfunc main(...) as int\n return work(3)\nend\n";
 const char* texts[]={"func work(int x) as int\n int value=3\n return x+2\nend\nfunc main(...) as int\n return work(3)\nend\n","func work(int x) as int\n string value=\"a\"\n return x/2\nend\nfunc main(...) as int\n return work(3)\nend\n"};char* path=xrtPathAbs("exit-misses.xl");CHECK(path);xls_semantic_check* old=checked(path,first);CHECK(old&&xlsSemanticCheckInfo(old).accepted);xls_ir_memo* prior=NULL;CHECK(compare(old,NULL,&prior,32u*1024u*1024u,0));xlsSemanticCheckDestroy(old);
 for(size_t i=0;i<2;++i){xls_semantic_check* current=checked(path,texts[i]);CHECK(current&&xlsSemanticCheckInfo(current).accepted);xls_ir_memo* next=NULL;xlsExitTestReset();CHECK(compare(current,prior,&next,32u*1024u*1024u,0));xls_exit_test_work work=xlsExitTestWork();CHECK(work.originals==1&&!work.reused&&work.built==1);xlsFrontendIrMemoDestroy(next);xlsSemanticCheckDestroy(current);}
 xlsFrontendIrMemoDestroy(prior);xrtFree(path);return 1;
}
static int prototypeMain(void){xlsExitTestLimit(8u*1024u*1024u);if(priorMemoMain()!=0||!exitPairs()||!largeExitPairs()||!keyPredicates()||!renamedPair()||!exitMisses())return 1;puts("exit prototype passed:");return 0;}

#include "analysis/frontend_ir_exit.h"
static int sameResult(const xls_ir_result* r,const char* expected)
{char* actual=r?dump(xlsFrontendIrResultModule(r)):NULL;int okay=actual&&!strcmp(actual,expected);xrtFree(actual);CHECK(okay);return 1;}
static int boundedExits(void)
{
 const char* text="func value(int x) as int\n string item=\"owned\"\n try\n return x+3\n finally\n string final=\"release\"\n end\nend\nfunc main(...) as int\n return value(2)\nend\n";
 char* path=xrtPathAbs("exit-bounds.xl");CHECK(path);xls_semantic_check* source=checked(path,text);CHECK(source&&xlsSemanticCheckInfo(source).accepted);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);XL6_DiagList diags;xl6DiagInit(&diags);
 xlsExitTestLimit(8u*1024u*1024u);xls_ir_result* first=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,NULL,32u*1024u*1024u);CHECK(first&&!diags.count&&sameResult(first,expected));xls_exit_result_work captured=xlsExitResultWork(first);CHECK(captured.captured==1&&captured.originals==1&&captured.knownBytes>512&&captured.knownBytes<=captured.limitBytes);
 const size_t limits[]={0,1,64,512,captured.knownBytes-1,captured.knownBytes,8u*1024u*1024u,SIZE_MAX};
 for(size_t i=0;i<sizeof(limits)/sizeof(*limits);++i){xlsExitTestLimit(limits[i]);xls_ir_result* next=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,first,32u*1024u*1024u);CHECK(next&&!diags.count&&sameResult(next,expected));xls_exit_result_work work=xlsExitResultWork(next);CHECK(work.knownBytes<=limits[i]&&work.knownBytes<=8u*1024u*1024u);if(limits[i]<captured.knownBytes)CHECK(!work.reused&&!work.knownBytes);else CHECK(work.reused==1&&!work.originals&&work.knownBytes==captured.knownBytes);xlsFrontendIrResultDestroy(next);}
 xlsExitTestLimit(8u*1024u*1024u);xls_ir_result* disabled=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,first,0);CHECK(disabled&&!diags.count&&sameResult(disabled,expected)&&!xlsExitResultWork(disabled).knownBytes);xlsFrontendIrResultDestroy(disabled);xlsFrontendIrResultDestroy(first);xlsSemanticCheckDestroy(source);xl6DiagFree(&diags);xrtFree(expected);xrtFree(path);return 1;
}
typedef struct exit_reader {const XL6_AstNode* root;const xls_ir_result* previous;const char* expected;int passed;}exit_reader;
static int32 exitReader(void* user)
{
 exit_reader* in=user;xlsExitTestLimit(8u*1024u*1024u);in->passed=1;
 for(size_t i=0;i<8;++i){XL6_DiagList diags;xl6DiagInit(&diags);xls_ir_result* r=xlsFrontendIrResultBuild(in->root,&diags,in->previous,32u*1024u*1024u);char* actual=r?dump(xlsFrontendIrResultModule(r)):NULL;xls_exit_result_work work=xlsExitResultWork(r);in->passed=in->passed&&r&&actual&&!strcmp(actual,in->expected)&&!diags.count&&work.reused==1&&!work.originals&&work.knownBytes&&work.knownBytes<=work.limitBytes;xrtFree(actual);xlsFrontendIrResultDestroy(r);xl6DiagFree(&diags);}
 return 0;
}
static int exitReaders(void)
{
 char* path=xrtPathAbs("exit-readers.xl");CHECK(path);const char* text="func values(int count) as generator<string>\n string suffix=\"item\"\n for i in range(count)\n yield suffix+string(i)\n end\nend\n";xls_semantic_check* source=checked(path,text);CHECK(source&&xlsSemanticCheckInfo(source).accepted);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);XL6_DiagList diags;xl6DiagInit(&diags);xlsExitTestLimit(8u*1024u*1024u);xls_ir_result* previous=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,NULL,32u*1024u*1024u);CHECK(previous&&!diags.count&&xlsExitResultWork(previous).captured==1);
 exit_reader readers[4];xthread* threads[4];for(size_t i=0;i<4;++i){readers[i]=(exit_reader){xlsSemanticCheckAst(source),previous,expected,0};threads[i]=xrtThreadCreate(exitReader,&readers[i],0);CHECK(threads[i]);}for(size_t i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK&&readers[i].passed);xrtThreadDestroy(threads[i]);}
 xls_ir_result* held=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,previous,32u*1024u*1024u);CHECK(held&&!diags.count&&xlsExitResultWork(held).reused==1);xlsFrontendIrResultDestroy(previous);xlsSemanticCheckDestroy(source);CHECK(sameResult(held,expected));xlsFrontendIrResultDestroy(held);xl6DiagFree(&diags);xrtFree(path);xrtFree(expected);return 1;
}
static int exitGenerations(void)
{
 xls_ir_result* previous=NULL;char* path=xrtPathAbs("exit-generations.xl");CHECK(path);size_t known=0;
 for(size_t i=0;i<32;++i){char text[512];snprintf(text,sizeof(text),"// generation %zu 中文😀\nfunc choose(int x) as string\n string outer=\"outer %zu\"\n if x>%zu\n return outer\n end\n return \"other %zu\"\nend\n",i,i,i,i);xls_semantic_check* source=checked(path,text);CHECK(source&&xlsSemanticCheckInfo(source).accepted);char* expected=dump(xlsSemanticCheckIr(source));CHECK(expected);XL6_DiagList diags;xl6DiagInit(&diags);xlsExitTestLimit(8u*1024u*1024u);xls_ir_result* next=xlsFrontendIrResultBuild(xlsSemanticCheckAst(source),&diags,previous,32u*1024u*1024u);CHECK(next&&!diags.count);xls_exit_result_work work=xlsExitResultWork(next);CHECK(work.knownBytes&&work.knownBytes<=work.limitBytes);if(i)CHECK(work.reused==1&&!work.originals&&known==work.knownBytes);else CHECK(work.captured==1&&work.originals==1);known=work.knownBytes;xlsFrontendIrResultDestroy(previous);xlsSemanticCheckDestroy(source);CHECK(sameResult(next,expected));previous=next;xrtFree(expected);xl6DiagFree(&diags);}
 xlsFrontendIrResultDestroy(previous);xrtFree(path);return 1;
}

static int productionAdmission(void)
{
 const char* body="func seed(int x) as int\n return x\nend\nfunc main(...) as int\n return seed(1)\nend\n";size_t base=strlen(body);char* path=xrtPathAbs("exit-admission.xl");CHECK(path);
 for(size_t edge=0;edge<3;++edge){size_t bytes=64u*1024u-1+edge;char* text=xrtMalloc(bytes+1);CHECK(text);text[0]='/';text[1]='/';memset(text+2,'x',bytes-base-3);text[bytes-base-1]='\n';memcpy(text+bytes-base,body,base+1);CHECK(strlen(text)==bytes);
  xls_check_session* session=xlsCheckSessionCreate(NULL);CHECK(session);xls_semantic_check* held=NULL;
  for(uint64_t version=1;version<=2;++version){char* value=strstr(text+bytes-base,"return seed(");CHECK(value);value+=strlen("return seed(");*value=(char)('0'+version);xls_text* source=xlsTextCreate(text,bytes,1024u*1024u,NULL);CHECK(source);xls_analysis_document document={"entry",path,source,1,version};char error[256];xls_semantic_check* current=xlsCheckSessionCheck(session,&document,1,"entry",1,NULL,NULL,error,sizeof(error));CHECK(current&&xlsSemanticCheckInfo(current).accepted);xls_exit_result_work work=xlsExitCheckWork(current);
   if(!edge)CHECK(!work.queried&&!work.reused&&!work.captured&&!work.knownBytes);else if(version==1)CHECK(work.queried==1&&work.captured==1&&work.originals==1&&work.knownBytes);else CHECK(work.queried==1&&work.reused==1&&!work.originals&&work.knownBytes);
   xls_semantic_check* fresh=checked(path,text);CHECK(fresh&&xlsSemanticCheckInfo(fresh).accepted);char *expected=dump(xlsSemanticCheckIr(fresh)),*actual=dump(xlsSemanticCheckIr(current));CHECK(expected&&actual&&!strcmp(expected,actual));xrtFree(expected);xrtFree(actual);xlsSemanticCheckDestroy(fresh);xlsSemanticCheckDestroy(held);held=current;xlsTextRelease(source);
  }
  xlsCheckSessionDestroy(session);CHECK(xlsSemanticCheckValidate(held));xlsSemanticCheckDestroy(held);xrtFree(text);
 }
 xrtFree(path);return 1;
}

int main(void){if(!xlsFrontendIrExitAvailable()){puts("exit reuse unsupported by this native cohort");return 0;}if(prototypeMain()||!boundedExits()||!exitReaders()||!exitGenerations()||!productionAdmission())return 1;printf("exit extended QA passed: %d total assertions; bounded cache, four readers, 32 retired generations\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;}
#else
int main(void){return priorMemoMain();}
#endif
