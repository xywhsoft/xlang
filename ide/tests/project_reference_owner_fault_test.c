#include "analysis_test_clock.h"
#include "services/project_references.h"
#include "analysis/worker_modules.h"
#include <xrt.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
static xatomic32 fenceMode,shardFault;
static xevent *fenceReached,*fenceResume;
static int fenceFired;
static void ownerFence(unsigned at)
{if(!fenceFired && xrtAtomic32Load(&fenceMode,XMEMORY_ACQUIRE)==at){fenceFired=1;xrtEventSet(fenceReached);xrtEventWait(fenceResume);}}
static xls_query* fenceDependencies(xls_worker* worker,xls_context* context,const xls_analysis_document* docs,size_t count,const char* target)
{xls_query* q=xlsWorkerDependenciesInContext(worker,context,docs,count,target);if(q)ownerFence(1);return q;}
static enum xls_query_state fenceDependencyResult(xls_query* query,xls_dependency_capture** result,xls_dependency_session_stats* stats,uint64_t* thread,char* error,size_t capacity)
{enum xls_query_state state=xlsQueryDependencyResult(query,result,stats,thread,error,capacity);if(state==XLS_QUERY_READY)ownerFence(2);return state;}
static xide_language_check* fenceSemantic(xls_context* context,xide_toolchains* cache,const char* compiler,xls_worker* actor,const xls_analysis_document* docs,size_t count,const char* trigger,const char* keyPath,int line,int character,const xide_language_analysis_options* options,xide_model_error* error)
{if(keyPath)ownerFence(4);xide_language_check* check=xideLanguageCheckStartReferenceCapture(context,cache,compiler,actor,docs,count,trigger,keyPath,line,character,options,error);if(check && keyPath)ownerFence(3);return check;}
static int fenceDependencyValidate(xls_dependency_capture* value)
{ownerFence(5);return xlsDependencyCaptureValidate(value);}
/* Called only from the private alternate snapshot TU. The original shard
 * factory and semantic checker remain real. Mode2 uses a real one-byte quota;
 * mode1 simulates only factory allocation refusal on an otherwise valid root. */
xide_reference_shard* ownerFaultShardCreate(xls_projection_store* cache,xls_semantic_check* checked,const xide_semantic_source* sources,size_t count,size_t bytes,xide_model_error* error)
{
 unsigned mode=xrtAtomic32Load(&shardFault,XMEMORY_ACQUIRE);const char* path=xlsSemanticCheckInfo(checked).path;
 if(mode && path && strstr(path,"a_library.xl")){if(mode==1){if(error){error->code=XIDE_MODEL_MEMORY;snprintf(error->message,sizeof(error->message),"controlled shard allocation refusal");}return NULL;}bytes=1;}
 return xideSemanticReferenceShardCreate(cache,checked,sources,count,bytes,error);
}
#define xlsWorkerDependenciesInContext fenceDependencies
#define xlsQueryDependencyResult fenceDependencyResult
#define xideLanguageCheckStartReferenceCapture fenceSemantic
#define xlsDependencyCaptureValidate fenceDependencyValidate
/* Fixture-only original locations() wrapper calls ownerFence(6) after its
 * unchanged body. That phase is after member guards/before owner publication. */
#ifndef XIDE_OWNER_REFERENCE_FIXTURE
#error Build this fixture through tools/build_lsp_core.py
#endif
#include XIDE_OWNER_REFERENCE_FIXTURE
#undef xlsWorkerDependenciesInContext
#undef xlsQueryDependencyResult
#undef xideLanguageCheckStartReferenceCapture
#undef xlsDependencyCaptureValidate
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_reference_owner_fault_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int faultWrite(const char* path,const char* body){return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)});}
static int waitIdle(xide_project_references* store)
{
 uint64_t until=xlsTestClockMicros()+15000000;
 do{xtaskpoolstats stats;if(!xrtTaskPoolGet(store->pool,&stats))return 0;if(!stats.Running && !stats.Queued)return 1;xlsTestSleepMicros(100);}while(xlsTestClockMicros()<until);return 0;
}
static enum xide_reference_query_state collectFault(xide_reference_query* q,const xvalue** value,xide_project_reference_stats* stats,char* error)
{uint64_t until=xlsTestClockMicros()+15000000;enum xide_reference_query_state state;do{state=xideProjectReferencesResult(q,value,stats,error,512);if(state!=XIDE_REFERENCES_PENDING)return state;xlsTestSleepMicros(100);}while(xlsTestClockMicros()<until);return XIDE_REFERENCES_PENDING;}
typedef struct fault_fixture {char *dir,*entry,*lib,*other,*project,*compiler;xls_context* context;xide_toolchains* cache;xide_project_references* store;xls_text* text;xls_analysis_document doc;const char *main,*library;} fault_fixture;
static int faultSetup(fault_fixture* f,int allocator)
{
 char name[128];snprintf(name,sizeof(name),"fault-%llu",(unsigned long long)xlsTestClockMicros());f->dir=xrtPathAbs(name);CHECK(f->dir && xrtDirCreateAll(f->dir));f->entry=xrtPathJoin(f->dir,"middle.xl");f->lib=xrtPathJoin(f->dir,"a_library.xl");f->other=xrtPathJoin(f->dir,"z_importer.xl");f->project=xrtPathJoin(f->dir,"project.xlp");f->compiler=xrtPathJoin(f->dir,"absent.exe");CHECK(f->entry && f->lib && f->other && f->project && f->compiler);
 f->main=allocator?"func main(...) as int\n return 0\nend\n":"import a_library as api\nfunc main(...) as int\n return api.main(7)\nend\n";
 f->library=allocator?"export func compute(int value) as int\n return value+1\nend\n":"export func main(int value) as int\n return value+1\nend\n";
 const char* other=allocator?"import a_library as api\nfunc main(...) as int\n return api.compute(8)\nend\n":"import a_library as api\nfunc main(...) as int\n return api.main(8)\nend\n";
 CHECK(faultWrite(f->entry,f->main) && faultWrite(f->lib,f->library) && faultWrite(f->other,other));const char* members[]={f->entry,f->lib,f->other};xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=f->project,.configuration="debug",.entry=f->entry,.directory=f->dir,.targetKind="exe",.analysisProfile="xlang-ls/compiler-xrt-v1"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,3};f->context=xlsContextCreate(&spec,1024*1024,NULL);f->cache=xideToolchainsCreate();f->store=xideProjectReferencesCreate(f->cache,f->compiler,NULL);const char* source=allocator?f->main:f->library;f->text=xlsTextCreate(source,strlen(source),1024*1024,NULL);CHECK(f->context && f->cache && f->store && f->text);f->doc=(xls_analysis_document){"query",allocator?f->entry:f->lib,f->text,1,1};return 1;
}
static void faultClear(fault_fixture* f)
{xideProjectReferencesDestroy(f->store);xideToolchainsDestroy(f->cache);xlsContextRelease(f->context);xlsTextRelease(f->text);xrtFree(f->dir);xrtFree(f->entry);xrtFree(f->lib);xrtFree(f->other);xrtFree(f->project);xrtFree(f->compiler);}
static int cancelAndMutation(void)
{
 for(unsigned mode=1;mode<=6;++mode)for(unsigned empty=0;empty<(mode==5?2u:1u);++empty){
  fault_fixture f={0};CHECK(faultSetup(&f,0));const xvalue* locations=NULL;xide_project_reference_stats stats;char error[512]={0};int line=empty?1:0,col=empty?0:13;
  /* Warm cached owner proof for mode6;the same final fence must reject a
     changed original disk owner even when the dependency API is bypassed. */
  if(mode==6){xide_reference_query* seed=xideProjectReferencesSubmit(f.store,f.context,&f.doc,1,"query",f.lib,0,13,1);CHECK(seed && collectFault(seed,&locations,&stats,error)==XIDE_REFERENCES_READY && xrtValueCount(locations)==3);xideProjectReferencesQueryDestroy(seed);CHECK(waitIdle(f.store));}
  fenceReached=xrtEventCreate(false,false);fenceResume=xrtEventCreate(false,false);CHECK(fenceReached && fenceResume);fenceFired=0;xrtAtomic32Store(&fenceMode,mode,XMEMORY_RELEASE);
  xide_reference_query* q=xideProjectReferencesSubmit(f.store,f.context,&f.doc,1,"query",f.lib,line,col,1);CHECK(q && xrtEventWaitFor(fenceReached,10000000)==XWAIT_OK);
  if(mode<=3)xideProjectReferencesCancel(q);else CHECK(faultWrite(f.entry,"func main(...) as int\n return 0\nend\n"));
  xrtEventSet(fenceResume);enum xide_reference_query_state state=collectFault(q,&locations,&stats,error);CHECK(state==(mode<=3?XIDE_REFERENCES_CANCELLED:XIDE_REFERENCES_FAILED) && !locations && !stats.roots && !xideProjectReferencesTake(q));const reference_output* internal=xrtFutureValue(q->future);if(empty)CHECK(internal && !internal->stats.memberTasks && !f.store->records.Count);xideProjectReferencesQueryDestroy(q);CHECK(waitIdle(f.store));
  for(size_t i=0;i<f.store->memberCount;++i){xls_worker_load load;CHECK(xlsWorkerLoad(f.store->members[i],&load) && !load.queued && !load.running);}xls_worker_load load;CHECK(xlsWorkerLoad(f.store->keys,&load) && !load.queued && !load.running);
  xrtAtomic32Store(&fenceMode,0,XMEMORY_RELEASE);xrtEventDestroy(fenceReached);xrtEventDestroy(fenceResume);fenceReached=fenceResume=NULL;CHECK(faultWrite(f.entry,f.main));
  q=xideProjectReferencesSubmit(f.store,f.context,&f.doc,1,"query",f.lib,0,13,1);CHECK(q && collectFault(q,&locations,&stats,error)==XIDE_REFERENCES_READY && xrtValueCount(locations)==3);xideProjectReferencesQueryDestroy(q);CHECK(waitIdle(f.store));faultClear(&f);
  printf("owner fault fence mode=%u empty=%u passed\n",mode,empty);fflush(stdout);
 }return 1;
}
static int infrastructure(void)
{
 for(unsigned mode=1;mode<=2;++mode){fault_fixture f={0};CHECK(faultSetup(&f,1));xrtAtomic32Store(&shardFault,mode,XMEMORY_RELEASE);xide_reference_query* q=xideProjectReferencesSubmit(f.store,f.context,&f.doc,1,"query",f.entry,0,6,1);const xvalue* locations=NULL;xide_project_reference_stats stats;char error[512]={0};CHECK(q && collectFault(q,&locations,&stats,error)==XIDE_REFERENCES_FAILED && !locations && !xideProjectReferencesTake(q));
  const reference_output* out=xrtFutureValue(q->future);CHECK(out && !out->stats.sourceRejections && *out->error);xideProjectReferencesQueryDestroy(q);CHECK(waitIdle(f.store) && f.store->records.Count==1);xrtAtomic32Store(&shardFault,0,XMEMORY_RELEASE);
  q=xideProjectReferencesSubmit(f.store,f.context,&f.doc,1,"query",f.entry,0,6,1);CHECK(q && collectFault(q,&locations,&stats,error)==XIDE_REFERENCES_READY && xrtValueCount(locations)==1);xideProjectReferencesQueryDestroy(q);CHECK(waitIdle(f.store));faultClear(&f);printf("owner infrastructure mode=%u never deferred passed\n",mode);
 }return 1;
}
int main(void)
{xrtAtomic32Init(&fenceMode,0);xrtAtomic32Init(&shardFault,0);if(!cancelAndMutation() || !infrastructure())return 1;printf("project_reference_owner_fault_test passed: %u checks;real metadata/semantic jobs,three cancellation and four publication mutation fences,including empty and cached owner proof,real quota/factory refusal never deferred,idle teardown and recovery\n",checks);return 0;}
