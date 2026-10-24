#include "analysis/dependency_graph.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>

/* The separately linked production API is tested below. A private exact copy
 * adds deterministic allocation faults;no fault behavior is shipped. */
static size_t fixtureCalls,fixtureFail,fixtureLive;
static void* fixtureMalloc(size_t bytes){if(++fixtureCalls==fixtureFail)return NULL;void* p=xrtMalloc(bytes);if(p)++fixtureLive;return p;}
static void* fixtureCalloc(size_t count,size_t bytes){if(++fixtureCalls==fixtureFail)return NULL;void* p=xrtCalloc(count,bytes);if(p)++fixtureLive;return p;}
static void fixtureFree(void* p){if(p)--fixtureLive;xrtFree(p);}
#define xrtMalloc fixtureMalloc
#define xrtCalloc fixtureCalloc
#define xrtFree fixtureFree
#define xlsDependencyGraphCreate privateGraphCreate
#define xlsDependencyGraphDestroy privateGraphDestroy
#define xlsDependencyGraphUpdate privateGraphUpdate
#define xlsDependencyGraphFind privateGraphFind
#define xlsDependencyGraphStats privateGraphStats
#include "analysis/dependency_graph.c"
#undef xlsDependencyGraphCreate
#undef xlsDependencyGraphDestroy
#undef xlsDependencyGraphUpdate
#undef xlsDependencyGraphFind
#undef xlsDependencyGraphStats
#undef xrtMalloc
#undef xrtCalloc
#undef xrtFree

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"analysis_dependency_graph_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static int binaryCompare(xls_dependency_key a,xls_dependency_key b,void* user)
{(void)user;size_t n=a.size<b.size ? a.size : b.size;int order=memcmp(a.data,b.data,n);return order ? order : (a.size>b.size)-(a.size<b.size);}
static xls_dependency_key stringKey(const char* s){return (xls_dependency_key){s,strlen(s)+1};}
static int sameLogical(xls_dependency_stats a,xls_dependency_stats b)
{return a.generation==b.generation && a.updates==b.updates && a.roots==b.roots && a.paths==b.paths && a.edges==b.edges && a.bytes==b.bytes;}
typedef struct stop_state {size_t calls,at;} stop_state;
static int stopAt(void* user){stop_state* s=user;return ++s->calls==s->at;}
static int basics(void)
{
 enum xls_dependency_error error=XLS_DEPENDENCY_OK;uint64_t mask=99;xls_dependency_stats stats;
 CHECK(!xlsDependencyGraphCreate(NULL,NULL,NULL,&error) && error==XLS_DEPENDENCY_ARGUMENT);
 xls_dependency_limits bad={65,32,32,64,16384};CHECK(!xlsDependencyGraphCreate(&bad,binaryCompare,NULL,&error));
 xls_dependency_graph* g=xlsDependencyGraphCreate(NULL,binaryCompare,NULL,&error);CHECK(g && error==XLS_DEPENDENCY_OK);
 CHECK(!xlsDependencyGraphFind(NULL,stringKey("a"),&mask) && !mask);
 CHECK(!xlsDependencyGraphFind(g,(xls_dependency_key){NULL,0},&mask));CHECK(!xlsDependencyGraphFind(g,stringKey("a"),NULL));
 CHECK(xlsDependencyGraphFind(g,stringKey("missing"),&mask) && !mask);
 unsigned char bytes[]={0,42,0,255};xls_dependency_key a[]={stringKey("a"),stringKey("shared"),stringKey("a"),{bytes,sizeof(bytes)}};
 xls_dependency_key b[]={stringKey("b"),stringKey("shared")};xls_dependency_update u[]={{0,a,4},{63,b,2}};
 CHECK(xlsDependencyGraphUpdate(g,u,2,NULL,NULL,&error) && error==XLS_DEPENDENCY_OK);
 CHECK(xlsDependencyGraphFind(g,stringKey("shared"),&mask) && mask==(UINT64_C(1)<<63)+1);
 CHECK(xlsDependencyGraphFind(g,a[3],&mask) && mask==1);bytes[1]=9;
 CHECK(xlsDependencyGraphFind(g,a[3],&mask) && !mask);bytes[1]=42;
 CHECK(xlsDependencyGraphFind(g,a[3],&mask) && mask==1);
 xlsDependencyGraphStats(g,&stats);CHECK(stats.paths==4 && stats.edges==5 && stats.roots==2 && stats.generation==1);
 xls_dependency_stats before=stats;
 xls_dependency_update invalid[]={{0,NULL,0},{0,NULL,0}};CHECK(!xlsDependencyGraphUpdate(g,invalid,2,NULL,NULL,&error) && error==XLS_DEPENDENCY_ARGUMENT);
 invalid[1].root=64;CHECK(!xlsDependencyGraphUpdate(g,invalid,2,NULL,NULL,&error));
 xlsDependencyGraphStats(g,&stats);CHECK(sameLogical(before,stats));
 /* Shared nodes are removed only once,after all updated owner arrays have
  * released their references;the new owner can retain the same key. */
 xls_dependency_update replace[]={{0,NULL,0},{63,NULL,0},{7,b,2}};
 CHECK(xlsDependencyGraphUpdate(g,replace,3,NULL,NULL,&error));
 CHECK(xlsDependencyGraphFind(g,stringKey("shared"),&mask) && mask==(UINT64_C(1)<<7));
 xlsDependencyGraphStats(g,&stats);CHECK(stats.paths==2 && stats.edges==2 && stats.roots==1 && stats.generation==2);
 replace[2].keys=NULL;replace[2].count=0;CHECK(xlsDependencyGraphUpdate(g,replace,3,NULL,NULL,&error));
 xlsDependencyGraphStats(g,&stats);CHECK(!stats.paths && !stats.edges && !stats.roots && stats.bytes==sizeof(*g));
 CHECK(xlsDependencyGraphUpdate(g,NULL,0,NULL,NULL,&error));xlsDependencyGraphDestroy(g);
 xlsDependencyGraphStats(NULL,&stats);CHECK(!stats.bytes && !stats.generation);xlsDependencyGraphDestroy(NULL);
 return 1;
}
static int differential(void)
{
 xls_dependency_graph* g=xlsDependencyGraphCreate(NULL,binaryCompare,NULL,NULL);CHECK(g);
 uint64_t expected[97]={0};uint32_t values[97];for(size_t i=0;i<97;++i)values[i]=(uint32_t)i;
 uint32_t state=123456789;size_t generation=0;
 for(size_t step=0;step<240;++step){
  xls_dependency_update updates[4];xls_dependency_key keys[4][24];size_t n=1+step%4;
  for(size_t i=0;i<n;++i){
   size_t root=(step*7+i*11)%64;uint64_t bit=UINT64_C(1)<<root;
   for(size_t j=0;j<97;++j)expected[j]&=~bit;
   size_t count=(step+i*3)%24;
   for(size_t j=0;j<count;++j){state=state*1664525u+1013904223u;size_t k=state%97;keys[i][j]=(xls_dependency_key){&values[k],sizeof(values[k])};expected[k]|=bit;}
   updates[i]=(xls_dependency_update){root,keys[i],count};
  }
  CHECK(xlsDependencyGraphUpdate(g,updates,n,NULL,NULL,NULL));++generation;
  size_t paths=0,edges=0;uint64_t roots=0;
  for(size_t j=0;j<97;++j){uint64_t mask=99;CHECK(xlsDependencyGraphFind(g,(xls_dependency_key){&values[j],sizeof(values[j])},&mask) && mask==expected[j]);
   paths+=mask!=0;roots|=mask;while(mask){++edges;mask&=mask-1;}
  }
  size_t owners=0;while(roots){++owners;roots&=roots-1;}
  xls_dependency_stats stats;xlsDependencyGraphStats(g,&stats);CHECK(stats.paths==paths && stats.edges==edges && stats.roots==owners && stats.generation==generation);
 }
 xlsDependencyGraphDestroy(g);return 1;
}
static xls_dependency_graph* seedPrivate(void)
{
 fixtureFail=0;xls_dependency_graph* g=privateGraphCreate(NULL,binaryCompare,NULL,NULL);
 xls_dependency_key keys[]={stringKey("old"),stringKey("shared"),stringKey("later")};
 xls_dependency_update changes[]={{0,keys,3},{63,keys+1,2}};
 if(!g || !privateGraphUpdate(g,changes,2,NULL,NULL,NULL)){privateGraphDestroy(g);return NULL;}return g;
}
static int verifyPrivate(xls_dependency_graph* g,xls_dependency_stats old,size_t live)
{
 xls_dependency_stats stats;privateGraphStats(g,&stats);CHECK(sameLogical(old,stats) && fixtureLive==live);
 uint64_t mask=0;CHECK(privateGraphFind(g,stringKey("old"),&mask) && mask==1);
 CHECK(privateGraphFind(g,stringKey("shared"),&mask) && mask==1+(UINT64_C(1)<<63));
 CHECK(privateGraphFind(g,stringKey("new"),&mask) && !mask);return 1;
}
static int faultsAndCancel(void)
{
 fixtureCalls=0;fixtureFail=1;enum xls_dependency_error createError=XLS_DEPENDENCY_OK;
 CHECK(!privateGraphCreate(NULL,binaryCompare,NULL,&createError) && createError==XLS_DEPENDENCY_MEMORY && !fixtureLive);fixtureFail=0;
 xls_dependency_key keys[]={stringKey("new"),stringKey("shared"),stringKey("newer"),stringKey("new")};
 xls_dependency_update changes[]={{0,keys,4},{63,keys+1,3},{8,keys,4}};
 xls_dependency_graph* g=seedPrivate();CHECK(g);fixtureCalls=0;stop_state count={0};
 CHECK(privateGraphUpdate(g,changes,3,stopAt,&count,NULL));size_t allocations=fixtureCalls,stops=count.calls;
 privateGraphDestroy(g);CHECK(!fixtureLive && allocations>=6 && stops>=10);
 for(size_t i=1;i<=allocations;++i){
  g=seedPrivate();CHECK(g);xls_dependency_stats old;privateGraphStats(g,&old);size_t live=fixtureLive;
  fixtureCalls=0;fixtureFail=i;enum xls_dependency_error error=XLS_DEPENDENCY_OK;
  CHECK(!privateGraphUpdate(g,changes,3,NULL,NULL,&error) && error==XLS_DEPENDENCY_MEMORY);
  CHECK(verifyPrivate(g,old,live));fixtureFail=0;privateGraphDestroy(g);CHECK(!fixtureLive);
 }
 for(size_t i=1;i<=stops;++i){
  g=seedPrivate();CHECK(g);xls_dependency_stats old;privateGraphStats(g,&old);size_t live=fixtureLive;stop_state stop={0,i};enum xls_dependency_error error=XLS_DEPENDENCY_OK;
  CHECK(!privateGraphUpdate(g,changes,3,stopAt,&stop,&error) && error==XLS_DEPENDENCY_CANCELLED);
  CHECK(verifyPrivate(g,old,live));CHECK(privateGraphUpdate(g,changes,3,NULL,NULL,NULL));privateGraphDestroy(g);CHECK(!fixtureLive);
 }
 printf("Dependency transaction fault allocations=%zu,cancellation gates=%zu;all preserve original graph\n",allocations,stops);return 1;
}
static int limits(void)
{
 xls_dependency_limits chosen={2,2,4,16,16384};xls_dependency_graph* g=xlsDependencyGraphCreate(&chosen,binaryCompare,NULL,NULL);CHECK(g);
 xls_dependency_key keys[]={stringKey("a"),stringKey("b"),stringKey("c")};xls_dependency_update change={0,keys,2};CHECK(xlsDependencyGraphUpdate(g,&change,1,NULL,NULL,NULL));
 xls_dependency_stats before,after;xlsDependencyGraphStats(g,&before);change.root=1;change.count=3;enum xls_dependency_error error;
 CHECK(!xlsDependencyGraphUpdate(g,&change,1,NULL,NULL,&error) && error==XLS_DEPENDENCY_LIMIT);
 xlsDependencyGraphStats(g,&after);CHECK(sameLogical(before,after));
 change.count=1;change.keys=(xls_dependency_key[]){{"a very long key beyond limit",28}};
 CHECK(!xlsDependencyGraphUpdate(g,&change,1,NULL,NULL,&error) && error==XLS_DEPENDENCY_ARGUMENT);
 xlsDependencyGraphStats(g,&after);CHECK(sameLogical(before,after));xlsDependencyGraphDestroy(g);
 chosen=(xls_dependency_limits){2,20,40,128,sizeof(*g)+64};g=xlsDependencyGraphCreate(&chosen,binaryCompare,NULL,NULL);CHECK(g);
 change=(xls_dependency_update){0,keys,2};CHECK(!xlsDependencyGraphUpdate(g,&change,1,NULL,NULL,&error) && error==XLS_DEPENDENCY_LIMIT);
 xlsDependencyGraphStats(g,&after);CHECK(!after.paths && !after.edges && after.bytes==sizeof(*g));xlsDependencyGraphDestroy(g);return 1;
}
int main(void)
{
 if(!basics() || !differential() || !faultsAndCancel() || !limits())return 1;
 printf("analysis_dependency_graph_test passed: %u checks;actual XRT AVL,transactional reverse owners,binary keys,bit63,240 differential batches,all allocation/cancellation failures and bounded staging.\n",checks);return 0;
}
