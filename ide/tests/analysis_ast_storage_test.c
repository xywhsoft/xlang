
#include "analysis/ast_clone.h"
#include "analysis/completion_session.h"
#include "analysis/frontend_internal.h"
#include "src/parser/xl6_parser_api.h"
#include "src/stdlib/xl6_stdlib.h"
#include "src/sem/xl6_sem.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_ast_storage_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
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

static size_t pooledNodes(const XL6_AstNode* node,const xls_ast_storage* storage)
{
 if(!node)return 0;
 if(node->xlsStorage!=storage)return SIZE_MAX;
 size_t count=1,a=pooledNodes(node->inferredType,storage),b=pooledNodes(node->semanticDeclaredType,storage);
 if(a==SIZE_MAX || b==SIZE_MAX)return SIZE_MAX;
 count+=a+b;
 for(size_t i=0;i<node->childCount;++i){a=pooledNodes(node->children[i],storage);if(a==SIZE_MAX)return a;count+=a;}return count;
}
static int reject(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{(void)in;(void)out;unsigned* remaining=user;return --*remaining!=0;}
static int copyOwner(const XL6_AstNode* in,XL6_AstNode* out,void* user)
{(void)user;out->xlsStorage=in->xlsStorage;return 1;}
static int cloneCases(const XL6_AstNode* input)
{
 uint64 baseline=live();xls_ast_pool_work work={0};size_t bytes=0;
 xmemstats start,pooled,regularStart,regularEnd;xrtMemStatsGet(&start);
 XL6_AstNode* copy=xlsAstClonePooled(input,&(xls_ast_clone_options){.transform=copyOwner},NULL,NULL,64u*1024u*1024u,&work,&bytes);xrtMemStatsGet(&pooled);
 CHECK(copy && copy->xlsStorage && equal(input,copy,1) && work.nodes==pooledNodes(copy,copy->xlsStorage));
 CHECK(work.pages && work.pages==(work.nodes+63)/64 && work.nodes>work.pages*8 && work.bytes<=bytes);
 xrtMemStatsGet(&regularStart);XL6_AstNode* regular=xlsAstClone(copy,&(xls_ast_clone_options){.transform=copyOwner},NULL);xrtMemStatsGet(&regularEnd);
 CHECK(regular && !regular->xlsStorage && equal(copy,regular,1) && pooledNodes(regular,NULL)==work.nodes);
 CHECK(pooled.BlockAllocCalls-start.BlockAllocCalls<regularEnd.BlockAllocCalls-regularStart.BlockAllocCalls);
 printf("AST nodes=%llu pages=%llu pool logical allocations=%llu heap=%llu charged=%llu bytes\n",(unsigned long long)work.nodes,(unsigned long long)work.pages,(unsigned long long)(pooled.BlockAllocCalls-start.BlockAllocCalls),(unsigned long long)(regularEnd.BlockAllocCalls-regularStart.BlockAllocCalls),(unsigned long long)work.bytes);xl6AstFree(regular);
 XL6_AstNode* child=xl6AstNew(XL6_AST_NAME,NULL);CHECK(child);xl6AstAddChild(copy,child);CHECK(copy->children[copy->childCount-1]==child && !child->xlsStorage);
 XL6_AstNode* holder=xl6AstNew(XL6_AST_MODULE,NULL);CHECK(holder);xl6AstMoveChildren(holder,copy);CHECK(!copy->childCount && holder->childCount==input->childCount+1);
 xl6AstFree(copy);CHECK(holder->children[0]->xlsStorage && holder->children[holder->childCount-1]==child);xl6AstFree(holder);
 if(live()!=baseline)fprintf(stderr,"mixed forest live allocations before=%llu after=%llu nodes=%llu pages=%llu\n",(unsigned long long)baseline,(unsigned long long)live(),(unsigned long long)work.nodes,(unsigned long long)work.pages);
 CHECK(live()==baseline);
 copy=xlsAstClonePooled(input,NULL,NULL,NULL,0,&work,&bytes);CHECK(copy && !copy->xlsStorage && !work.nodes && equal(input,copy,1));xl6AstFree(copy);CHECK(live()==baseline);
 size_t budgets[]={1,128,4096};for(unsigned i=0;i<3;++i){copy=xlsAstClonePooled(input,NULL,NULL,NULL,budgets[i],&work,&bytes);CHECK(!copy && !bytes && !work.nodes && live()==baseline);}
 copy=xlsAstClonePooled(input,NULL,NULL,NULL,SIZE_MAX,&work,&bytes);CHECK(copy && work.bytes<=64u*1024u*1024u && equal(input,copy,1));size_t exact=bytes;xl6AstFree(copy);CHECK(live()==baseline);
 copy=xlsAstClonePooled(input,&(xls_ast_clone_options){.bytes=exact},NULL,NULL,SIZE_MAX,&work,&bytes);CHECK(copy && bytes==exact);xl6AstFree(copy);
 copy=xlsAstClonePooled(input,&(xls_ast_clone_options){.bytes=exact-1},NULL,NULL,SIZE_MAX,&work,&bytes);CHECK(!copy && !bytes && !work.nodes && live()==baseline);
 for(unsigned i=1;i<30;++i){unsigned remaining=i;copy=xlsAstClonePooled(input,&(xls_ast_clone_options){.transform=reject,.user=&remaining},NULL,NULL,SIZE_MAX,&work,&bytes);CHECK(!copy && live()==baseline);}
 /* Warm the library's error state before measuring rollback allocation balance. */
 CHECK(!xrtPoolInitLayout(NULL,0,0,0));baseline=live();allocation.failAt=0;allocation.recording=1;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);
 copy=xlsAstClonePooled(input,NULL,NULL,NULL,SIZE_MAX,&work,&bytes);allocation.recording=0;uint64 operations=xrtAtomic64Load(&allocation.calls,XMEMORY_RELAXED);CHECK(copy && operations>0);xl6AstFree(copy);CHECK(live()==baseline);
 for(uint64 i=1;i<=operations;++i){allocation.failAt=i;allocation.recording=1;xrtAtomic64Store(&allocation.calls,0,XMEMORY_RELAXED);copy=xlsAstClonePooled(input,NULL,NULL,NULL,SIZE_MAX,&work,&bytes);allocation.recording=0;CHECK(!copy && !bytes && !work.nodes && live()==baseline);}
 allocation.failAt=0;printf("AST pool optional allocation faults=%llu,all rolled back\n",(unsigned long long)operations);return 1;
}
typedef struct reader {const XL6_AstNode* input;int okay;} reader;
static int32 readCopies(void* user)
{
 reader* r=user;r->okay=1;for(unsigned i=0;i<20;++i){xls_ast_pool_work work={0};XL6_AstNode* copy=xlsAstClonePooled(r->input,NULL,NULL,NULL,SIZE_MAX,&work,NULL);if(!copy || !equal(r->input,copy,1) || !work.nodes)r->okay=0;xl6AstFree(copy);}return 0;
}
static int rollbackPositions(void)
{
 char* path=xrtPathAbs("ast-storage-position-rollback.xl");CHECK(path);
 xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,71,0};
 for(unsigned i=0;i<4;++i){
  xstrbuf body={0};if(i)CHECK(xrtStrBufAppend(&body,xrtStrView("\xef\xbb\xbf")));
  for(unsigned j=0;j<i;++j)CHECK(xrtStrBufAppend(&body,xrtStrView("// move 中文😀\n")));
  CHECK(xrtStrBufAppend(&body,xrtStrView("func large<T>(T value) as T\n")));
  for(unsigned j=0;j<180;++j)CHECK(xrtStrBufAppendFormat(&body," T local_%u=value\n",j));
  CHECK(xrtStrBufAppend(&body,xrtStrView(" return value\nend\nfunc main(...) as int\n int completionLocal=1\n return completionL\nend\n")));
  xls_text* text=xlsTextCreate(body.Data,body.Size,1024u*1024u,NULL);CHECK(text);xlsTextRelease(doc.text);doc.text=text;++doc.revision;
  size_t offset=(size_t)(strstr(body.Data,"return completionL")-body.Data)+strlen("return completionL")-(i?3:0);
  xls_sem_storage_control control;xlsFrontendSemStorageBegin(&control,32u*1024u);
  xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);xlsFrontendSemStorageEnd(&control,NULL);
  CHECK(result && xlsSemanticCompletionInfo(result).available);
  if(i){xls_sem_storage_work work=xlsSemanticCompletionStorageWork(result);CHECK(work.heapFallbacks && !work.nodes && xlsSemanticCompletionSemWork(result).reused>=1);}
  xls_semantic_input input={path,body.Data,body.Size};xls_semantic_completion* fresh=xlsSemanticCompletionCreate(path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(fresh && equal(xlsSemanticCompletionAst(fresh),xlsSemanticCompletionAst(result),1));
  char *a=xlsSemanticCompletionJson(result,NULL),*b=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && b && !strcmp(a,b));xlsSemanticStringFree(a);xlsSemanticStringFree(b);xlsSemanticCompletionDestroy(fresh);xlsSemanticCompletionDestroy(result);xrtStrBufFree(&body);
 }
 xlsCompletionSessionDestroy(session);xlsTextRelease(doc.text);xrtFree(path);return 1;
}
static int semanticCases(void)
{
 char* path=xrtPathAbs("ast-storage-completion.xl");CHECK(path);xls_semantic_completion* held=NULL;
 size_t budgets[]={0,1,4096,64u*1024u*1024u};
 for(unsigned b=0;b<4;++b){xls_completion_session* session=xlsCompletionSessionCreate(NULL);CHECK(session);xls_analysis_document doc={"entry",path,NULL,11,0};xls_sem_storage_control outer;xlsFrontendSemStorageBegin(&outer,budgets[b]);
  for(unsigned i=0;i<4;++i){xstrbuf body={0};for(unsigned j=0;j<24;++j)CHECK(xrtStrBufAppendFormat(&body,"func stable_%u(int value) as int\n int local=value+%u\n if local>0 then\n return local\n end\n return 0\nend\n",j,j));CHECK(xrtStrBufAppendFormat(&body,"func main(...) as int\n int completionLocal=%u\n return completionL\nend\n",i));
   xls_text* text=xlsTextCreate(body.Data,body.Size,1024u*1024u,NULL);CHECK(text);xlsTextRelease(doc.text);doc.text=text;++doc.revision;size_t offset=(size_t)(strstr(body.Data,"return completionL")-body.Data)+strlen("return completionL");
   xls_semantic_completion* result=xlsCompletionSessionQuery(session,&doc,1,"entry","entry",offset,NULL,NULL,NULL,0);CHECK(result && xlsSemanticCompletionInfo(result).available);xls_sem_storage_work work=xlsSemanticCompletionStorageWork(result);
   if(i){CHECK(xlsSemanticCompletionSemWork(result).reused>=24 && work.chargedBytes<=budgets[b]);if(b==3)CHECK(work.nodes && work.pages && !work.heapFallbacks);else CHECK(!work.nodes && work.heapFallbacks>=24);}
   xls_semantic_input input={path,body.Data,body.Size};xls_semantic_completion* fresh=xlsSemanticCompletionCreate(path,path,offset,&input,1,NULL,NULL,NULL,0);CHECK(fresh);
   char *a=xlsSemanticCompletionJson(result,NULL),*c=xlsSemanticCompletionJson(fresh,NULL);CHECK(a && c && !strcmp(a,c));CHECK(equal(xlsSemanticCompletionAst(fresh),xlsSemanticCompletionAst(result),1));
   xlsSemanticStringFree(a);xlsSemanticStringFree(c);xlsSemanticCompletionDestroy(fresh);if(b==3 && i==3)held=xlsSemanticCompletionRetain(result);xlsSemanticCompletionDestroy(result);xrtStrBufFree(&body);
  }
  xls_sem_storage_work total;xlsFrontendSemStorageEnd(&outer,&total);CHECK(total.chargedBytes<=budgets[b]);if(b==3)CHECK(total.nodes && total.pages);xlsCompletionSessionDestroy(session);xlsTextRelease(doc.text);
 }
 CHECK(held && xlsSemanticCompletionStorageWork(held).nodes && xlsSemanticCompletionAst(held));char* json=xlsSemanticCompletionJson(held,NULL);CHECK(json);xlsSemanticStringFree(json);xlsSemanticCompletionDestroy(held);xrtFree(path);return 1;
}
static int run(void)
{
 const char* source="func helper<T>(T value) as T\n return value\nend\nclass Box\n string value\nend\nfunc main(...) as int\n string local = \"a\\0😀\"\n auto box = Box()\n int result=helper::<int>(7)\n return result\nend\n";
 XL6_ParseResult parsed;xl6ParseResultInit(&parsed);CHECK(xl6ParseSource(source,"/memory/pool中文.xl",&parsed) && xl6StdlibExpandModule(parsed.root,&parsed.diags) && xl6SemCheckModuleWithIndex(parsed.root,&parsed.diags,"pool",1));
 CHECK(cloneCases(parsed.root));xls_ast_pool_work work;XL6_AstNode* pooled=xlsAstClonePooled(parsed.root,NULL,NULL,NULL,SIZE_MAX,&work,NULL);CHECK(pooled);
 reader values[4];xthread* threads[4];for(unsigned i=0;i<4;++i){values[i]=(reader){pooled,0};threads[i]=xrtThreadCreate(readCopies,&values[i],0);CHECK(threads[i]);}for(unsigned i=0;i<4;++i){CHECK(xrtThreadWait(threads[i])==XWAIT_OK && values[i].okay);xrtThreadDestroy(threads[i]);}xl6AstFree(pooled);xl6ParseResultFree(&parsed);
 CHECK(semanticCases());CHECK(rollbackPositions());return 1;
}
int main(void)
{
 xrtAtomic64Init(&allocation.live,0);xrtAtomic64Init(&allocation.calls,0);xrtGetAllocator(&allocation.original);xallocator custom={&allocation,trackedAlloc,trackedRealloc,trackedFree};
 if(!xrtSetAllocator(&custom))return 1;
 xrtMemStatsEnable(true);if(!run())return 1;
 printf("analysis_ast_storage_test passed: %u checks\n",checks);return 0;
}
