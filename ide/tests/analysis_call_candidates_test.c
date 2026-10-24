#include "analysis/frontend_internal.h"
#include "src/parser/xl6_parser_api.h"
#include "src/sem/xl6_sem.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 checks;
#define CHECK(v) do{xrtAtomic32FetchAdd(&checks,1,XMEMORY_RELAXED);if(!(v)){fprintf(stderr,"analysis_call_candidates_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int sameText(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static int sameRange(XL6_AstRange a,XL6_AstRange b)
{ return a.known==b.known && a.startOffset==b.startOffset && a.endOffset==b.endOffset && a.startLine==b.startLine && a.startColumn==b.startColumn && a.endLine==b.endLine && a.endColumn==b.endColumn; }
static int sameAst(const XL6_AstNode* a,const XL6_AstNode* b)
{
    if(!a || !b) return a==b;
    if(a->kind!=b->kind || a->flags!=b->flags || !sameText(a->text,b->text) || !sameText(a->doc,b->doc) ||
       a->literalSize!=b->literalSize || a->literalScalar!=b->literalScalar || a->literalDecoded!=b->literalDecoded || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize)) || a->line!=b->line || a->column!=b->column ||
       !sameRange(a->range,b->range) || !sameRange(a->tokenRange,b->tokenRange) || !sameText(xl6AstSourcePath(a),xl6AstSourcePath(b)) ||
       !!a->source!=!!b->source || (a->source && (a->source->completionQuery!=b->source->completionQuery || a->source->completionStart!=b->source->completionStart || a->source->completionEnd!=b->source->completionEnd)) || a->semanticTypeId!=b->semanticTypeId || a->semanticTypeAbiId!=b->semanticTypeAbiId || a->semanticSymbolId!=b->semanticSymbolId ||
       a->semanticDeclaredSymbolId!=b->semanticDeclaredSymbolId || a->semanticTargetSymbolId!=b->semanticTargetSymbolId ||
       a->semanticCompoundOperatorSymbolId!=b->semanticCompoundOperatorSymbolId || a->semanticCallKind!=b->semanticCallKind ||
       a->semanticLvalueKind!=b->semanticLvalueKind || a->semanticOwnership!=b->semanticOwnership || a->semanticExitKind!=b->semanticExitKind ||
       a->semanticScopeDepth!=b->semanticScopeDepth || a->semanticTargetScopeDepth!=b->semanticTargetScopeDepth ||
       a->semanticRegionId!=b->semanticRegionId || a->semanticTargetRegionId!=b->semanticTargetRegionId || a->childCount!=b->childCount) return 0;
    if(!sameAst(a->inferredType,b->inferredType) || !sameAst(a->semanticDeclaredType,b->semanticDeclaredType)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!sameAst(a->children[i],b->children[i])) return 0;
    return 1;
}


static int corpus(const char* definitions,const char* body,int expected,int indexed)
{
 xstrbuf text={0};CHECK(xrtStrBufAppend(&text,xrtStrView(definitions)));
 for(unsigned i=0;i<100;++i)CHECK(xrtStrBufAppendFormat(&text,"func pad_%u(int value) as int\n return value+%u\nend\n",i,i));
 CHECK(xrtStrBufAppend(&text,xrtStrView(body)));
 const size_t budgets[]={8u*1024u*1024u,0,1,8192};XL6_ParseResult parsed[4];int accepted[4];xls_sem_call_work work[4]={{0}};
 for(unsigned k=0;k<4;++k){
  xl6ParseResultInit(&parsed[k]);CHECK(xl6ParseSource(text.Data,"call-中文.xl",&parsed[k]));
  xls_sem_call_control control;xlsFrontendSemCallsBegin(&control,budgets[k]);
  accepted[k]=xl6SemCheckModuleWithIndex(parsed[k].root,&parsed[k].diags,"call",1);
  xlsFrontendSemCallsEnd(&control,&work[k]);CHECK(accepted[k]==expected);
 }
 for(unsigned k=0;k<4;++k){
  CHECK(parsed[k].diags.count==parsed[1].diags.count && sameAst(parsed[k].root,parsed[1].root));
  for(size_t d=0;d<parsed[k].diags.count;++d){const XL6_Diag *a=&parsed[k].diags.items[d],*b=&parsed[1].diags.items[d];CHECK(a->line==b->line && a->column==b->column && sameText(a->message,b->message) && sameText(a->file,b->file));}
 }
 CHECK(!work[1].indexes && !work[1].indexedQueries && !work[2].indexes && !work[2].indexedQueries);
 CHECK(work[1].candidateVisits==work[1].linearQueries*parsed[1].root->childCount);
 if(indexed){CHECK(work[0].indexes==1 && work[0].indexedQueries>0 && !work[0].linearQueries && work[0].rootVisits==parsed[0].root->childCount);CHECK(work[0].indexedQueries==work[1].linearQueries && work[0].candidateVisits<work[1].candidateVisits/10);}
 else CHECK(!work[0].indexes && !work[0].indexedQueries);
 for(unsigned k=0;k<4;++k)xl6ParseResultFree(&parsed[k]);
 xrtStrBufFree(&text);return 1;
}
static int completion(void)
{
 char* path=xrtPathAbs("call-cursor.xl");CHECK(path);xstrbuf body={0};
 CHECK(xrtStrBufAppend(&body,xrtStrView("func identity(int v) as int\n return v\nend\n")));
 for(unsigned i=0;i<90;++i)CHECK(xrtStrBufAppendFormat(&body,"func helper_%u(int v) as int\n return identity(v)+%u\nend\n",i,i));
 CHECK(xrtStrBufAppend(&body,xrtStrView("func main(...) as int\n int member = identity(7)\n return mem\nend\n")));
 size_t at=(size_t)(strstr(body.Data,"return mem")-body.Data)+strlen("return mem");xls_semantic_input input={path,body.Data,body.Size};
 xls_semantic_completion* results[2]={0};xls_sem_call_work work[2]={{0}};
 for(unsigned k=0;k<2;++k){xls_sem_call_control control;xlsFrontendSemCallsBegin(&control,k?0:8u*1024u*1024u);results[k]=xlsSemanticCompletionCreate(path,path,at,&input,1,NULL,NULL,NULL,0);xlsFrontendSemCallsEnd(&control,&work[k]);CHECK(results[k] && xlsSemanticCompletionInfo(results[k]).available);}
 char *a=xlsSemanticCompletionJson(results[0],NULL),*b=xlsSemanticCompletionJson(results[1],NULL);CHECK(a && b && !strcmp(a,b));CHECK(sameAst(xlsSemanticCompletionAst(results[0]),xlsSemanticCompletionAst(results[1])));
 CHECK(work[0].indexes==1 && work[0].indexedQueries>0 && !work[1].indexes && work[1].linearQueries==work[0].indexedQueries && work[1].candidateVisits>work[0].candidateVisits*20);
 CHECK(xlsSemanticCompletionCallWork(results[0]).candidateVisits==work[0].candidateVisits && xlsSemanticCompletionCallWork(results[1]).linearQueries==work[1].linearQueries);
 printf("call candidates indexed=%llu original=%llu, root=%llu queries=%llu data=%llu bytes\n",(unsigned long long)work[0].candidateVisits,(unsigned long long)work[1].candidateVisits,(unsigned long long)work[0].rootVisits,(unsigned long long)work[0].indexedQueries,(unsigned long long)work[0].bytes);
 xlsSemanticStringFree(a);xlsSemanticStringFree(b);for(unsigned k=0;k<2;++k)xlsSemanticCompletionDestroy(results[k]);xrtStrBufFree(&body);xrtFree(path);return 1;
}
/* Exercise the shared candidate builder's optional operations. Classification
 * helpers here are the unchanged audited compiler helpers. Full language
 * checks above execute the real TU with enabled/disabled candidate indexes. */
static const char* xl6SemFuncName(const XL6_AstNode* node)
{return node && node->childCount>0 && node->children[0] ? node->children[0]->text : NULL;}
static int xl6SemIsFuncDecl(const XL6_AstNode* node)
{return node && (node->kind==XL6_AST_FUNC || node->kind==XL6_AST_ASYNC_FUNC || node->kind==XL6_AST_EXTERN_FUNC || node->kind==XL6_AST_EXPORT_FUNC);}
static unsigned operation,failedAt;
static int fault(void){return ++operation==failedAt;}
static void* testMalloc(size_t n){return fault()?NULL:xrtMalloc(n);}
static bool testMapInit(xmap* m,size_t n){return !fault() && xrtMapInit(m,n);}
static bool testMapSet(xmap* m,xbytesview k,const void* v){return !fault() && xrtMapSet(m,k,v);}
#define xrtMalloc testMalloc
#define xrtMapInit testMapInit
#define xrtMapSet testMapSet
#include "analysis/frontend_sem_names.inc"
#undef xrtMalloc
#undef xrtMapInit
#undef xrtMapSet
static int failures(void)
{
 XL6_ParseResult parse;xl6ParseResultInit(&parse);xstrbuf body={0};
 for(unsigned i=0;i<100;++i)CHECK(xrtStrBufAppendFormat(&body,"func name_%u(int value) as int\n return value\nend\n",i));
 CHECK(xl6ParseSource(body.Data,"names.xl",&parse));operation=failedAt=0;xmap heads={0};xls_sem_overload_work work={0};xls_sem_overload_scan scan=xlsSemNamesBuild(parse.root,&heads,8u*1024u*1024u,&work);CHECK(scan.next && work.indexes==1);unsigned seams=operation;xrtFree(scan.next);xrtMapUnit(&heads);
 for(unsigned i=1;i<=seams;++i){operation=0;failedAt=i;memset(&heads,0,sizeof(heads));memset(&work,0,sizeof(work));scan=xlsSemNamesBuild(parse.root,&heads,8u*1024u*1024u,&work);CHECK(!scan.next && !work.indexes);xrtMapUnit(&heads);}
 failedAt=0;operation=0;scan=xlsSemNamesBuild(parse.root,&heads,8u*1024u*1024u,&work);CHECK(scan.next);xrtFree(scan.next);xrtMapUnit(&heads);printf("call name-index optional seams=%u,all failed/recovered\n",seams);xl6ParseResultFree(&parse);xrtStrBufFree(&body);return 1;
}
static int nested(void)
{
 xstrbuf text={0};
 for(unsigned i=0;i<80;++i)CHECK(xrtStrBufAppendFormat(&text,"func n_%u() as int\n return 1\nend\n",i));
 CHECK(xrtStrBufAppend(&text,xrtStrView("func main(...) as int\n return n_7()\nend\n")));
 XL6_ParseResult parsed[2];for(unsigned i=0;i<2;++i){xl6ParseResultInit(&parsed[i]);CHECK(xl6ParseSource(text.Data,"nested.xl",&parsed[i]));}
 xls_sem_call_control outer,inner;xls_sem_call_work first={0},all={0};
 xlsFrontendSemCallsBegin(&outer,0);xlsFrontendSemCallsBegin(&inner,8u*1024u*1024u);
 CHECK(xl6SemCheckModuleWithIndex(parsed[0].root,&parsed[0].diags,"nested",1));xlsFrontendSemCallsEnd(&inner,&first);
 CHECK(xl6SemCheckModuleWithIndex(parsed[1].root,&parsed[1].diags,"nested",1));xlsFrontendSemCallsEnd(&outer,&all);
 CHECK(first.indexes==1 && !first.linearQueries && all.indexes==1 && all.linearQueries>0 && all.indexedQueries==first.indexedQueries);
 CHECK(sameAst(parsed[0].root,parsed[1].root));for(unsigned i=0;i<2;++i)xl6ParseResultFree(&parsed[i]);xrtStrBufFree(&text);return 1;
}
typedef struct reader {int passed;} reader;
static int32 parallel(void* user)
{
 reader* r=user;r->passed=corpus("func 求值(int value) as int\n return value\nend\nfunc 求值(string value) as int\n return 0\nend\n",
     "func main(...) as int\n return 求值(7)\nend\n",1,1);return 0;
}
static int extra(void)
{
 const char* defs="func choose(int value) as int\n return value\nend\nfunc choose(...) as int\n return 0\nend\n";
 CHECK(corpus(defs,"func main(...) as int\n return choose(7)+choose(1,2,3)\nend\n",1,1));
 CHECK(corpus("func choose(int a) as int\n return a\nend\nfunc choose(int a,int b=7) as int\n return a+b\nend\n","func main(...) as int\n return choose(7)\nend\n",0,1));
 CHECK(corpus("export func visible(int value) as int\n return value\nend\n","func main(...) as int\n return visible(7)\nend\n",1,1));
 char name[301];memset(name,'z',300);name[300]=0;xstrbuf declaration={0};
 CHECK(xrtStrBufAppendFormat(&declaration,"func %s() as int\n return 0\nend\n",name));
 CHECK(corpus(declaration.Data,"func main(...) as int\n return pad_0(7)\nend\n",1,0));xrtStrBufFree(&declaration);
 CHECK(nested());reader readers[4]={{0}};xthread* threads[4];
 for(unsigned i=0;i<4;++i){threads[i]=xrtThreadCreate(parallel,&readers[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && readers[i].passed);xrtThreadDestroy(threads[i]);}
 return 1;
}
int main(void)
{
 const char* defs="func choose(int v) as int\n return v\nend\nfunc choose(string v) as string\n return v\nend\nfunc identity<T>(T v) as T\n return v\nend\n";
 if(!corpus(defs,"func main(...) as int\n return choose(7)+identity::<int>(8)\nend\n",1,1) ||
    !corpus(defs,"func main(...) as int\n return choose()\nend\n",0,1) ||
    !corpus(defs,"func main(...) as int\n return identity::<int>(\"wrong\")\nend\n",0,1) ||
    !corpus("func select(int a,int b=7) as int\n return a+b\nend\n", "func main(...) as int\n return select(1)\nend\n",1,1) ||
    !corpus("func dup(int v) as int\n return v\nend\nfunc dup(int v) as int\n return v\nend\n", "func main(...) as int\n return dup(1)\nend\n",0,1) ||
    !completion() || !failures() || !extra())return 1;
 printf("analysis_call_candidates_test passed: %d checks\n",xrtAtomic32Load(&checks,XMEMORY_RELAXED));return 0;
}
