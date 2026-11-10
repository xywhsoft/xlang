#include "analysis/semantic_tokens.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static size_t calls,failAt,live;static void* held[32];static int badFree;
static int permitted(void){return ++calls!=failAt;}
static void track(void* p){if(!p)return;for(size_t i=0;i<32;++i)if(!held[i]){held[i]=p;++live;return;}badFree=1;}
static void untrack(void* p){if(!p)return;for(size_t i=0;i<32;++i)if(held[i]==p){held[i]=NULL;--live;return;}badFree=1;}
static void* allocate(size_t n,size_t z){if(!permitted())return NULL;void* p=xrtCalloc(n,z);track(p);return p;}
static void release(void* p){untrack(p);xrtFree(p);}
static bool init(xarray* a,size_t z){return permitted() && xrtArrayInit(a,z);}
static bool reserve(xarray* a,size_t n){if(!permitted())return false;untrack(a->Allocation);bool ok=xrtArrayReserve(a,n);track(a->Allocation);return ok;}
static bool pushRow(xarray* a,const void* row){if(!permitted())return false;untrack(a->Allocation);bool ok=xrtArrayPush(a,row);track(a->Allocation);return ok;}
static bool sortRows(xarray* a,xarraycompare order){return permitted() && xrtArraySort(a,order);}
static void unit(xarray* a){untrack(a->Allocation);xrtArrayUnit(a);}
#define xrtCalloc allocate
#define xrtFree release
#define xrtArrayInit init
#define xrtArrayReserve reserve
#define xrtArrayPush pushRow
#define xrtArraySort sortRows
#define xrtArrayUnit unit
#include "analysis/semantic_tokens.c"
#undef xrtCalloc
#undef xrtFree
#undef xrtArrayInit
#undef xrtArrayReserve
#undef xrtArrayPush
#undef xrtArraySort
#undef xrtArrayUnit
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_semantic_tokens_fault_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
typedef struct cancellation {size_t calls,at;} cancellation;
static int stopAt(void* user){cancellation* stop=user;return ++stop->calls==stop->at;}
static int run(void)
{
 const char* code="class Box<T>\n T value\n func identity(T arg) as T\n return arg\n end\nend\nfunc main(...) as int\n int local=7\n return local\nend\n";
 char* path=xrtPathAbs("token-fault.xl");CHECK(path);xls_semantic_input input={path,code,strlen(code)};xls_semantic_check* c=xlsSemanticCheckCreate(path,&input,1,1,NULL,NULL,NULL,0);CHECK(c && xlsSemanticCheckInfo(c).accepted);
 size_t points=0;for(size_t failure=0;failure<=points;++failure){calls=0;failAt=failure;enum xls_semantic_tokens_error e;xls_semantic_tokens* t=xlsSemanticTokensCreate(c,0,0,NULL,NULL,&e);
  if(!failure){points=calls;CHECK(t && points>100 && e==XLS_TOKENS_OK);}
  else CHECK(!t && e==XLS_TOKENS_MEMORY);
  xlsSemanticTokensRelease(t);CHECK(!live && !badFree && xlsSemanticCheckInfo(c).accepted);
 }
 failAt=calls=0;size_t successes=0;for(size_t budget=sizeof(xls_semantic_tokens);budget<=16384;budget=budget<1024?budget+1:budget+127){enum xls_semantic_tokens_error e;xls_semantic_tokens* t=xlsSemanticTokensCreate(c,0,budget,NULL,NULL,&e);if(t){CHECK(!e && xlsSemanticTokensWork(t).peakBytes<=budget);++successes;}else CHECK(e==XLS_TOKENS_LIMIT);xlsSemanticTokensRelease(t);CHECK(!live && !badFree);}
 CHECK(successes);
 cancellation stop={0};enum xls_semantic_tokens_error error;
 xls_semantic_tokens* t=xlsSemanticTokensCreate(c,0,0,stopAt,&stop,&error);CHECK(t && !error);
 size_t polls=stop.calls;xlsSemanticTokensRelease(t);CHECK(!live && !badFree && polls>4);
 for(size_t i=1;i<=polls;++i){stop=(cancellation){0,i};t=xlsSemanticTokensCreate(c,0,0,stopAt,&stop,&error);CHECK(!t && error==XLS_TOKENS_CANCELLED && stop.calls==i && !live && !badFree);}
 xlsSemanticCheckDestroy(c);xrtFree(path);printf("analysis_semantic_tokens_fault_test passed: %u checks; %zu actual allocator/array API failure positions and %zu cancellation positions; all owned allocations released\n",checks,points,polls);return 1;
}
int main(void){return run()?0:1;}
