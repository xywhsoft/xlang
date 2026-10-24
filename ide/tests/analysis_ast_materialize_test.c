
#include "analysis/ast_materialize.h"
#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include "src/parser/xl6_parser_api.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ast_materialize_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
typedef struct allocator_state {xallocator original;xatomic64 live,calls;uint64 failAt;int recording;} allocator_state;
static allocator_state allocation;
static int fail(void){return allocation.recording && xrtAtomic64FetchAdd(&allocation.calls,1,XMEMORY_RELAXED)+1==allocation.failAt;}
static void* trackedAlloc(void* user,size_t size)
{(void)user;if(fail())return NULL;void* p=allocation.original.Alloc(allocation.original.Context,size);if(p)xrtAtomic64FetchAdd(&allocation.live,1,XMEMORY_RELAXED);return p;}
static void trackedFree(void* user,void* p)
{(void)user;if(p){if(!xrtAtomic64FetchSub(&allocation.live,1,XMEMORY_RELAXED))abort();allocation.original.Free(allocation.original.Context,p);}}
static void* trackedRealloc(void* user,void* p,size_t size)
{
 (void)user;if(!size){trackedFree(NULL,p);return NULL;}if(fail())return NULL;
 void* value=allocation.original.Realloc(allocation.original.Context,p,size);if(value && !p)xrtAtomic64FetchAdd(&allocation.live,1,XMEMORY_RELAXED);return value;
}
/* Backing spans are intentionally retained by the XRT global heap. Logical
 * live blocks,not allocator backing spans,prove this forest's release. */
static uint64 live(void){xmemstats stats;xrtMemStatsGet(&stats);return stats.BlockAllocCalls-stats.BlockFreeCalls;}
static int same(const char* a,const char* b) { return a && b ? !strcmp(a,b) : a==b; }
static int equal(const XL6_AstNode* a,const XL6_AstNode* b,int sources)
{
    if(!a || !b) return a==b;
    XL6_AstNode left=*a,right=*b;
    left.text=right.text=left.doc=right.doc=NULL; left.literalData=right.literalData=NULL;
    left.children=right.children=NULL; left.inferredType=right.inferredType=left.semanticDeclaredType=right.semanticDeclaredType=NULL;
    left.xlsStorage=right.xlsStorage=NULL;
    left.source=right.source=NULL; left.childCapacity=right.childCapacity=0;
    if(memcmp(&left,&right,sizeof(left)) || !same(a->text,b->text) || !same(a->doc,b->doc) || !!a->literalData!=!!b->literalData ||
       (a->literalSize && memcmp(a->literalData,b->literalData,a->literalSize))) return 0;
    if(sources && a->source) {
        if(!b->source || b->source==a->source || !same(a->source->path,b->source->path) ||
           a->source->completionQuery!=b->source->completionQuery || a->source->completionStart!=b->source->completionStart ||
           a->source->completionEnd!=b->source->completionEnd) return 0;
    } else if(b->source) return 0;
    if(!equal(a->inferredType,b->inferredType,sources) || !equal(a->semanticDeclaredType,b->semanticDeclaredType,sources)) return 0;
    for(size_t i=0;i<a->childCount;++i) if(!equal(a->children[i],b->children[i],sources)) return 0;
    return 1;
}


static size_t nodes(const XL6_AstNode* n,int pooled)
{
 if(!n)return 0;
 if(!!n->xlsStorage!=pooled)return SIZE_MAX;
 size_t count=1,a=nodes(n->inferredType,pooled),b=nodes(n->semanticDeclaredType,pooled);if(a==SIZE_MAX || b==SIZE_MAX)return SIZE_MAX;count+=a+b;
 for(size_t i=0;i<n->childCount;++i){a=nodes(n->children[i],pooled);if(a==SIZE_MAX)return a;count+=a;}return count;
}
static int batch(void)
{
 XL6_AstNode* input=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(input);
 for(unsigned i=0;i<170;++i){XL6_AstNode* n=xl6AstNewText(XL6_AST_NAME,"value",5,NULL);CHECK(n);xl6AstAddChild(input,n);}
 CHECK(xl6AstSetSourceTree(input,"/batch.xl"));uint64 baseline=live();
 xls_ast_materialize_scope outer,child,mask;xlsAstMaterializeBegin(&outer,64u*1024u*1024u);
 XL6_AstNode* a=xlsAstMaterialize(input,NULL,NULL);CHECK(a && a->xlsStorage && equal(input,a,1));size_t remaining=xlsAstMaterializeBudget();
 xlsAstMaterializeBegin(&child,SIZE_MAX);XL6_AstNode* b=xlsAstMaterialize(input,NULL,NULL);CHECK(b && b->xlsStorage!=a->xlsStorage && equal(input,b,1));xls_ast_materialize_work childWork;xlsAstMaterializeEnd(&child,&childWork);CHECK(childWork.pooledCopies==1 && childWork.chargedBytes<=remaining && xlsAstMaterializeBudget()<remaining);
 xlsAstMaterializeBegin(&mask,0);XL6_AstNode* heap=xlsAstMaterialize(input,NULL,NULL);CHECK(heap && !heap->xlsStorage);xls_ast_materialize_work maskWork;xlsAstMaterializeEnd(&mask,&maskWork);CHECK(maskWork.heapCopies==1 && !maskWork.nodes && !maskWork.chargedBytes);
 XL6_AstNode* c=xlsAstMaterialize(input,NULL,NULL);CHECK(c && c->xlsStorage==a->xlsStorage && equal(input,c,1));xls_ast_materialize_work work;xlsAstMaterializeEnd(&outer,&work);CHECK(work.calls==4 && work.pooledCopies==3 && work.heapCopies==1 && work.nodes==3*nodes(a,1) && work.chargedBytes<=64u*1024u*1024u);
 xl6AstFree(a);CHECK(equal(input,c,1));xl6AstFree(heap);xl6AstFree(b);xl6AstFree(c);CHECK(live()==baseline);
 size_t budgets[]={0,1,4096};for(unsigned i=0;i<3;++i){xlsAstMaterializeBegin(&outer,budgets[i]);a=xlsAstMaterialize(input,NULL,NULL);CHECK(a && !a->xlsStorage && equal(input,a,1));xlsAstMaterializeEnd(&outer,&work);CHECK(work.heapCopies==1 && work.chargedBytes<=budgets[i]);xl6AstFree(a);CHECK(live()==baseline);}
 xlsAstMaterializeBegin(&outer,SIZE_MAX);a=xlsAstMaterialize(input,&(xls_ast_clone_options){.bytes=1},NULL);CHECK(!a);xlsAstMaterializeEnd(&outer,&work);CHECK(work.calls==1 && !work.pooledCopies && !work.heapCopies);CHECK(live()==baseline);
 /* The backing allocator sees pool pages,not every logical small block. */
 allocation.recording=1;allocation.failAt=0;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);xlsAstMaterializeBegin(&outer,SIZE_MAX);a=xlsAstMaterialize(input,NULL,NULL);xlsAstMaterializeEnd(&outer,&work);allocation.recording=0;uint64 operations=xrtAtomic64Load(&allocation.calls,XMEMORY_RELAXED);CHECK(a && operations);xl6AstFree(a);CHECK(live()==baseline);
 for(uint64 i=1;i<=operations;++i){allocation.failAt=i;allocation.recording=1;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);xlsAstMaterializeBegin(&outer,SIZE_MAX);a=xlsAstMaterialize(input,NULL,NULL);xlsAstMaterializeEnd(&outer,&work);allocation.recording=0;CHECK(a && equal(input,a,1) && work.poolFallbacks && work.heapCopies==1);xl6AstFree(a);CHECK(live()==baseline);}
 allocation.failAt=0;printf("Materialization backing failures=%llu,heap fallback complete and balanced\n",(unsigned long long)operations);xl6AstFree(input);return 1;
}
static int syntaxCases(void)
{
 const char* source="func stable(int value) as int\n return value+1\nend\nfunc main(...) as int\n return stable(7)\nend\n";
 char* path=xrtPathAbs("materialize-syntax.xl");xls_text* text=xlsTextCreate(source,strlen(source),1024*1024,NULL);CHECK(path && text);XL6_ParseResult parsed;xl6ParseResultInit(&parsed);CHECK(xl6ParseSource(source,path,&parsed));xls_syntax* syntax=xlsSyntaxAdoptOrdinary(path,text,&parsed.root);xl6ParseResultFree(&parsed);CHECK(syntax && xlsSyntaxAccepted(syntax));size_t before=0,after=0;CHECK(xlsSyntaxCacheBytes(syntax,1024*1024,&before));
 xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,SIZE_MAX);XL6_ParseResult copy;xl6ParseResultInit(&copy);CHECK(xlsSyntaxCopyParse(syntax,&copy) && copy.root->xlsStorage && nodes(copy.root,1)!=SIZE_MAX);CHECK(xlsSyntaxCacheBytes(syntax,1024*1024,&after) && after==before);
 xls_syntax* forbidden=xlsSyntaxAdoptOrdinary(path,text,&copy.root);CHECK(!forbidden && !copy.root);xl6ParseResultFree(&copy);
 CHECK(xlsSyntaxCopyParse(syntax,&copy) && copy.root->xlsStorage);const XL6_AstNode* view=xlsSyntaxAst(syntax);CHECK(view && !view->xlsStorage && nodes(view,0)!=SIZE_MAX && equal(view,copy.root,1));xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);CHECK(work.pooledCopies>=3 && work.nodes && work.pages && !work.heapCopies);
 xlsSyntaxRelease(syntax);xlsTextRelease(text);CHECK(copy.root && copy.root->childCount==2);xl6ParseResultFree(&copy);xrtFree(path);return 1;
}
static int syntaxAllocation(void)
{
 xstrbuf source={0};for(unsigned i=0;i<128;++i)CHECK(xrtStrBufAppendFormat(&source,"func helper_%u(int value) as int\n return value+%u\nend\n",i,i));
 char* path=xrtPathAbs("materialize-allocation.xl");xls_text* text=xlsTextCreate(source.Data,source.Size,1024*1024,NULL);CHECK(path && text);XL6_ParseResult parsed;xl6ParseResultInit(&parsed);CHECK(xl6ParseSource(source.Data,path,&parsed));xls_syntax* syntax=xlsSyntaxAdoptOrdinary(path,text,&parsed.root);xl6ParseResultFree(&parsed);CHECK(syntax);uint64 baseline=live(),allocations[2];XL6_ParseResult copies[2];xls_ast_materialize_work work[2];
 for(unsigned i=0;i<2;++i){xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,i ? 64u*1024u*1024u : 0);xmemstats before,after;xrtMemStatsGet(&before);xl6ParseResultInit(&copies[i]);CHECK(xlsSyntaxCopyParse(syntax,&copies[i]));xrtMemStatsGet(&after);allocations[i]=after.BlockAllocCalls-before.BlockAllocCalls;xlsAstMaterializeEnd(&scope,&work[i]);}
 CHECK(equal(copies[0].root,copies[1].root,1) && work[1].calls==129 && work[1].pooledCopies==129 && work[1].pages<work[1].calls && allocations[1]<allocations[0]);
 printf("Private syntax128 functions: heap logical allocations=%llu,shared pool=%llu,nodes=%llu,pages=%llu,copies=%llu\n",(unsigned long long)allocations[0],(unsigned long long)allocations[1],(unsigned long long)work[1].nodes,(unsigned long long)work[1].pages,(unsigned long long)work[1].calls);
 xl6ParseResultFree(&copies[0]);xl6ParseResultFree(&copies[1]);CHECK(live()==baseline);
 XL6_AstNode* empty=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(empty);xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,SIZE_MAX);XL6_AstNode* pooled=xlsAstMaterialize(empty,NULL,NULL);xlsAstMaterializeEnd(&scope,NULL);CHECK(pooled && pooled->xlsStorage && !xlsSyntaxAdoptOrdinary(path,text,&pooled) && !pooled);xl6AstFree(empty);
 xlsSyntaxRelease(syntax);xlsTextRelease(text);xrtFree(path);xrtStrBufFree(&source);return 1;
}
typedef struct reader {const xls_syntax* syntax;const XL6_AstNode* expected;size_t budget;int okay;} reader;
static int32 readPrivate(void* user)
{
 reader* r=user;r->okay=1;for(unsigned i=0;i<12;++i){xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,r->budget);XL6_ParseResult copy;xl6ParseResultInit(&copy);int okay=xlsSyntaxCopyParse(r->syntax,&copy);xls_ast_materialize_work work;xlsAstMaterializeEnd(&scope,&work);if(!okay || !equal(r->expected,copy.root,1) || (r->budget ? !work.nodes : !!work.nodes))r->okay=0;xl6ParseResultFree(&copy);}return 0;
}
static int concurrent(void)
{
 const char* source="func main(...) as int\n int value=7\n return value\nend\n";char* path=xrtPathAbs("materialize-concurrent.xl");xls_text* text=xlsTextCreate(source,strlen(source),1024,NULL);xls_parse_work work={0};xls_syntax* syntax=xlsSyntaxBuild(path,text,NULL,&work);CHECK(path && text && syntax);const XL6_AstNode* view=xlsSyntaxAst(syntax);CHECK(view && !view->xlsStorage);reader values[4];xthread* threads[4];
 for(unsigned i=0;i<4;++i){values[i]=(reader){syntax,view,i%2?0:64u*1024u*1024u,0};threads[i]=xrtThreadCreate(readPrivate,&values[i],0);CHECK(threads[i]);}
 for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && values[i].okay);xrtThreadDestroy(threads[i]);}xlsSyntaxRelease(syntax);xlsTextRelease(text);xrtFree(path);return 1;
}
static int completion(void)
{
 char* path=xrtPathAbs("materialize-completion.xl");CHECK(path);size_t budgets[]={0,1,32768,64u*1024u*1024u};xls_semantic_completion* held=NULL;
 for(unsigned b=0;b<4;++b){xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,53,0};
  for(unsigned i=0;i<8;++i){xstrbuf body={0};for(unsigned j=0;j<24;++j)CHECK(xrtStrBufAppendFormat(&body,"func stable_%u(int value) as int\n int local=value+%u\n if local>0 then\n return local\n end\n return 0\nend\n",j,j));CHECK(xrtStrBufAppendFormat(&body,"func main(...) as int\n int completionLocal=%u\n return completionL\nend\n",i));xls_text* text=xlsTextCreate(body.Data,body.Size,1024*1024,NULL);CHECK(text);xlsTextRelease(doc.text);doc.text=text;++doc.revision;size_t offset=(size_t)(strstr(body.Data,"return completionL")-body.Data)+strlen("return completionL");
   xls_ast_materialize_scope scope;xlsAstMaterializeBegin(&scope,budgets[b]);xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);xlsAstMaterializeEnd(&scope,NULL);CHECK(result && xlsSemanticCompletionInfo(result).available);xls_ast_materialize_work copied=xlsSemanticCompletionMaterializationWork(result);CHECK(copied.calls && copied.chargedBytes<=budgets[b]);if(b==3)CHECK(copied.nodes && copied.pooledCopies==copied.calls && !copied.heapCopies);if(!b)CHECK(!copied.nodes && copied.heapCopies==copied.calls);
   if(i)CHECK(xlsSemanticCompletionSemWork(result).reused>=24);
   xls_semantic_input input={path,body.Data,body.Size};xls_semantic_completion* fresh=xlsSemanticCompletionCreate(path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(fresh && !xlsSemanticCompletionMaterializationWork(fresh).nodes);char *a=xlsSemanticCompletionJson(result,NULL),*c=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && c && !strcmp(a,c) && equal(xlsSemanticCompletionAst(fresh),xlsSemanticCompletionAst(result),1));xlsSemanticStringFree(a);xlsSemanticStringFree(c);xlsSemanticCompletionDestroy(fresh);
   if(b==3 && i==7)held=xlsSemanticCompletionRetain(result);
   xlsSemanticCompletionDestroy(result);xrtStrBufFree(&body);
  }
  xlsCompletionSessionDestroy(session);xlsTextRelease(doc.text);
 }
 CHECK(held && xlsSemanticCompletionAst(held) && xlsSemanticCompletionMaterializationWork(held).nodes);char* json=xlsSemanticCompletionJson(held,NULL);CHECK(json);xlsSemanticStringFree(json);xlsSemanticCompletionDestroy(held);xrtFree(path);return 1;
}
static int run(void){CHECK(batch());CHECK(syntaxCases());CHECK(syntaxAllocation());CHECK(concurrent());CHECK(completion());return 1;}
int main(void)
{
 xrtAtomic64Init(&allocation.live,0);xrtAtomic64Init(&allocation.calls,0);xrtGetAllocator(&allocation.original);xallocator custom={&allocation,trackedAlloc,trackedRealloc,trackedFree};if(!xrtSetAllocator(&custom))return 1;xrtMemStatsEnable(true);if(!run())return 1;printf("analysis_ast_materialize_test passed: %u checks\n",checks);return 0;
}
