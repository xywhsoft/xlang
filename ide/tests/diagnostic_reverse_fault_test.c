#include "analysis_test_clock.h"
#ifndef XLS_COMPILER_XRT
#error Native dependency graph fault fixture requires compiler XRT
#endif
#include "analysis/dependency_graph.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static xatomic32 rejectCreate,rejectUpdate;
static xls_dependency_graph* faultGraphCreate(const xls_dependency_limits* limits,xls_dependency_compare compare,void* user,enum xls_dependency_error* error)
{if(xrtAtomic32Load(&rejectCreate,XMEMORY_ACQUIRE)){if(error)*error=XLS_DEPENDENCY_MEMORY;return NULL;}return xlsDependencyGraphCreate(limits,compare,user,error);}
static int faultGraphUpdate(xls_dependency_graph* graph,const xls_dependency_update* changes,size_t count,int (*stop)(void*),void* user,enum xls_dependency_error* error)
{if(xrtAtomic32Load(&rejectUpdate,XMEMORY_ACQUIRE)){if(error)*error=XLS_DEPENDENCY_MEMORY;return 0;}return xlsDependencyGraphUpdate(graph,changes,count,stop,user,error);}
/* Compile the exact actor under private names. Only the optional index's
 * creation/update allocation outcome is injected;all guards,publications,
 * negative candidates,threading and original fallback execute unchanged. */
#define xlsDependencyGraphCreate faultGraphCreate
#define xlsDependencyGraphUpdate faultGraphUpdate
#define xideDiagnosticPublicationDestroy faultPublicationDestroy
#define xideDiagnosticPublicationCount faultPublicationCount
#define xideDiagnosticPublicationGroup faultPublicationGroup
#define xideDiagnosticPublicationFind faultPublicationFind
#define xideDiagnosticPublicationCompare faultPublicationCompare
#define xideDiagnosticPublicationEqual faultPublicationEqual
#define xideDiagnosticPublicationDependencyCount faultPublicationDependencyCount
#define xideDiagnosticPublicationDependencyId faultPublicationDependencyId
#define xideDiagnosticPublicationGroupDependency faultPublicationGroupDependency
#define xideDiagnosticPublicationDependencyChunks faultPublicationDependencyChunks
#define xideDiagnosticPublicationFindDependency faultPublicationFindDependency
#define xideDiagnosticStoreCreateWithDependencies faultStoreCreateWithDependencies
#define xideDiagnosticStoreCreate faultStoreCreate
#define xideDiagnosticStoreDestroy faultStoreDestroy
#define xideDiagnosticStoreLoad faultStoreLoad
#define xideDiagnosticStorePutTake faultStorePutTake
#define xideDiagnosticStorePutConfigured faultStorePutConfigured
#define xideDiagnosticStoreRemove faultStoreRemove
#define xideDiagnosticStoreInvalidate faultStoreInvalidate
#define xideDiagnosticStoreSweep faultStoreSweep
#define xideDiagnosticStoreSnapshot faultStoreSnapshot
#define xideDiagnosticStoreClear faultStoreClear
#define xideDiagnosticQueryResult faultQueryResult
#define xideDiagnosticQueryTake faultQueryTake
#define xideDiagnosticQuerySetWake faultQuerySetWake
#define xideDiagnosticQueryCancel faultQueryCancel
#define xideDiagnosticQueryDestroy faultQueryDestroy
#include "services/diagnostic_store.c"

static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"diagnostic_reverse_fault_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
typedef struct fault_fixture {xide_language_input_guard* guard;xvalue* result;} fault_fixture;
static fault_fixture faultCapture(const char* root,const char* path,const char* body,int version)
{
 fault_fixture fixture={0};xide_toolchains* cache=xideToolchainsCreate();char* compiler=xrtPathJoin(root,"absent/xl.exe");xide_model_error error={0};
 xide_language_snapshot source={path,body,version};xide_language_analysis_options options={.nativeBackend=1};
 xide_language_check* check=xideLanguageCheckStartSnapshotsEx(cache,compiler,root,path,&source,1,&options,&error);uint64_t began=xlsTestClockMicros();
 while(check && xideLanguageCheckState(check,error.message,sizeof(error.message))==XIDE_CHECK_RUNNING && xlsTestClockMicros()-began<15000000)xrtSleep(1);
 fixture.result=xideLanguageCheckResult(check);if(fixture.result)fixture.guard=xideLanguageCheckCaptureInputGuard(check,&error);
 if(!fixture.guard){fprintf(stderr,"fault capture:%s\n",error.message);xrtValueRelease(fixture.result);fixture.result=NULL;}
 xideLanguageCheckDestroy(check);xideToolchainsDestroy(cache);xrtFree(compiler);return fixture;
}
static enum xide_diagnostic_state faultWait(xide_diagnostic_query* q,xide_diagnostic_snapshot* snapshot)
{
 uint64_t began=xlsTestClockMicros();enum xide_diagnostic_state state;
 while((state=faultQueryResult(q,snapshot))==XIDE_DIAGNOSTIC_PENDING && xlsTestClockMicros()-began<15000000)xrtSleep(1);
 if(state==XIDE_DIAGNOSTIC_FAILED)fprintf(stderr,"fault actor:%s\n",snapshot->error);
 return state;
}
static xide_diagnostic_query* faultInsert(xide_diagnostic_store* store,const char* label,fault_fixture f)
{xide_diagnostic_query* q=faultStorePutTake(store,label,f.guard,f.result);xideLanguageInputGuardRelease(f.guard);return q;}
static int faultRun(void)
{
 xrtAtomic32Init(&rejectCreate,1);xrtAtomic32Init(&rejectUpdate,0);
 char relative[96];snprintf(relative,sizeof(relative),"build/reverse-fault-%llu",(unsigned long long)xlsTestClockMicros());CHECK(xrtDirCreateAll(relative));
 char *root=xrtPathAbs(relative),*a=root ? xrtPathJoin(root,"a.xl") : NULL,*b=root ? xrtPathJoin(root,"b.xl") : NULL,
  *candidate=root ? xrtPathJoin(root,"missing.xl") : NULL,*other=root ? xrtPathJoin(root,"other.xl") : NULL;CHECK(root && a && b && candidate && other);
 const char *body="import missing\nfunc main(...) as int\n return missing.value()\nend\n",*independent="func main(...) as int\n return otherMissing\nend\n";
 fault_fixture f=faultCapture(root,a,body,1);CHECK(f.guard && f.result && xideLanguageInputGuardTouches(f.guard,candidate));
 xide_diagnostic_store* store=faultStoreCreate(NULL);CHECK(store);xide_diagnostic_snapshot snapshot={0};
 xide_diagnostic_query* q=faultInsert(store,"a",f);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && !snapshot.dependencyIndexed && snapshot.dependencyFallbacks);faultQueryDestroy(q);
 q=faultStoreInvalidate(store,other);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && !snapshot.dependencyIndexed && !xrtValueCount(snapshot.invalidated));faultQueryDestroy(q);
 q=faultStoreInvalidate(store,candidate);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && !snapshot.records && xrtValueCount(snapshot.invalidated)==1 && !snapshot.dependencyIndexed);faultQueryDestroy(q);
 xrtAtomic32Store(&rejectCreate,0,XMEMORY_RELEASE);f=faultCapture(root,a,body,2);CHECK(f.guard);
 q=faultInsert(store,"a",f);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && snapshot.dependencyIndexed && snapshot.dependencyGraph.roots==1);faultQueryDestroy(q);
 xrtAtomic32Store(&rejectUpdate,1,XMEMORY_RELEASE);f=faultCapture(root,b,independent,1);CHECK(f.guard);
 q=faultInsert(store,"b",f);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && !snapshot.dependencyIndexed && snapshot.dependencyFallbacks>=3);faultQueryDestroy(q);
 q=faultStoreInvalidate(store,other);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && !snapshot.dependencyIndexed);faultQueryDestroy(q);
 xrtAtomic32Store(&rejectUpdate,0,XMEMORY_RELEASE);f=faultCapture(root,a,body,3);CHECK(f.guard);
 q=faultInsert(store,"a",f);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==2 && snapshot.dependencyIndexed && snapshot.dependencyGraph.roots==2);faultQueryDestroy(q);
 q=faultStoreInvalidate(store,candidate);CHECK(q && faultWait(q,&snapshot)==XIDE_DIAGNOSTIC_READY && snapshot.records==1 && snapshot.dependencyIndexed && snapshot.dependencyGraph.roots==1 && snapshot.dependencyGraph.lookups==1);
 xide_diagnostic_publication* publication=NULL;xvalue* invalid=NULL;CHECK(faultQueryTake(q,&publication,&invalid) && publication && xrtValueCount(invalid)==1);faultQueryDestroy(q);
 faultStoreDestroy(store);CHECK(faultPublicationCount(publication)==1 && faultPublicationFind(publication,b)!=SIZE_MAX);faultPublicationDestroy(publication);xrtValueRelease(invalid);
 xrtFree(root);xrtFree(a);xrtFree(b);xrtFree(candidate);xrtFree(other);return 1;
}
int main(void)
{
 if(!faultRun())return 1;
 printf("diagnostic_reverse_fault_test passed: %u checks;real actor publication survives index create/update allocation failures,negative-candidate fallback and incremental index recovery.\n",checks);return 0;
}
