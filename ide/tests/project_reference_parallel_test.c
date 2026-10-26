#include "services/project_references.h"
#include "analysis/worker_modules.h"
#include <stdio.h>
#include <string.h>
#ifndef XIDE_REFERENCE_MEMBER_WORKERS
#define XIDE_REFERENCE_MEMBER_WORKERS 4u
#endif
static size_t workerCalls,failWorker,memberStarts;
static const char* refusedPath;
static int noWatch,holdAdmissions;
static xevent *admitted,*resumeAdmission;
static xls_worker* createWorker(const xls_session_limits* limits,size_t queue,
 const xls_worker_cache_limits* cache,xls_module_store* modules)
{if(++workerCalls==failWorker)return NULL;return xlsWorkerCreateWithModules(limits,queue,cache,modules);}
static xcancelwatch* registerCancel(xcancel* cancel,xcancelproc proc,ptr data)
{return noWatch ? NULL : xrtCancelWatch(cancel,proc,data);}
static xide_language_check* startCapture(xls_context* context,xide_toolchains* cache,
 const char* compiler,xls_worker* actor,const xls_analysis_document* docs,size_t count,
 const char* trigger,const char* keyPath,int line,int character,
 const xide_language_analysis_options* options,xide_model_error* error)
{
 if(refusedPath && strstr(xlsContextSpec(context)->entry,refusedPath))return NULL;
 xide_language_check* check=xideLanguageCheckStartReferenceCapture(context,cache,compiler,actor,docs,count,trigger,keyPath,line,character,options,error);
 if(check && !keyPath && ++memberStarts==XIDE_REFERENCE_MEMBER_WORKERS && holdAdmissions){xrtEventSet(admitted);xrtEventWait(resumeAdmission);}
 return check;
}
/* Interpose only this compiled host module,not the compiler or XRT. The
 * admission fence makes cancellation deterministic with real child checks. */
#define xlsWorkerCreateWithModules createWorker
#define xideLanguageCheckStartReferenceCapture startCapture
#define xrtCancelWatch registerCancel
#include "services/project_references.c"
#undef xlsWorkerCreateWithModules
#undef xideLanguageCheckStartReferenceCapture
#undef xrtCancelWatch
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"project_reference_parallel_test:%d:%s\n",__LINE__,#v);return 0;}}while(0)
static int writeText(const char* path,const char* text)
{return xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)text,strlen(text)});}
static int idle(xide_project_references* store)
{
 for(unsigned i=0;i<10000;++i){xtaskpoolstats stats;if(!xrtTaskPoolGet(store->pool,&stats))return 0;if(!stats.Queued && !stats.Running)return 1;xrtSleep(1);}return 0;
}
static xide_reference_query* submit(xide_project_references* store,xls_context* scope,const xls_analysis_document* doc)
{return xideProjectReferencesSubmit(store,scope,doc,1,doc->key,doc->path,2,16,1);}
static int collect(xide_reference_query* query,int okay,xvalue** value,xide_project_reference_stats* stats,char* error)
{
 *value=NULL;enum xide_reference_query_state state;
 for(unsigned i=0;;++i){state=xideProjectReferencesResult(query,NULL,stats,error,512);if(state!=XIDE_REFERENCES_PENDING)break;CHECK(i<10000);xrtSleep(1);}
 CHECK(state==(okay ? XIDE_REFERENCES_READY : XIDE_REFERENCES_FAILED));
 *value=xideProjectReferencesTake(query);CHECK(okay ? *value!=NULL : *value==NULL);xideProjectReferencesQueryDestroy(query);return 1;
}
static bool jsonBytes(xbytesview bytes,void* user)
{return xrtStrBufAppend(user,(xstrview){(const char*)bytes.Data,bytes.Size});}
static int equivalent(const xvalue* a,const xvalue* b)
{
 xjsonwriteconfig config;xrtJsonWriteConfigInit(&config);xstrbuf x={0},y={0};
 CHECK(xrtJsonWrite(a,&config,jsonBytes,&x) && xrtJsonWrite(b,&config,jsonBytes,&y));CHECK(x.Size==y.Size && !memcmp(x.Data,y.Data,x.Size));xrtStrBufFree(&x);xrtStrBufFree(&y);return 1;
}
static int run(void)
{
 char* directory=xrtPathAbs("build/reference-parallel");CHECK(directory && xrtDirCreateAll(directory));
 char *entry=xrtPathJoin(directory,"main.xl"),*shared=xrtPathJoin(directory,"shared.xl"),*project=xrtPathJoin(directory,"test.xlp"),*compiler=xrtPathJoin(directory,"absent-xl.exe");CHECK(entry && shared && project && compiler);
 const char* main="import shared\nfunc main(...) as int\n return shared.compute(7)\nend\n";
 const char* member="import shared\nfunc other() as int\n return shared.compute(8)\nend\n";
 CHECK(writeText(entry,main) && writeText(shared,"export func compute(int value) as int\n return value+1\nend\n"));
 char* paths[32];const char* members[34]={entry,shared};for(unsigned i=0;i<32;++i){char name[40];snprintf(name,sizeof(name),"member-%02u.xl",i);paths[i]=xrtPathJoin(directory,name);members[i+2]=paths[i];CHECK(paths[i] && writeText(paths[i],member));}
 xls_context_spec spec={.kind=XLS_CONTEXT_PROJECT,.project=project,.configuration="debug",.entry=entry,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){members,34};xls_context* scope=xlsContextCreate(&spec,16u*1024u*1024u,NULL);
 xls_module_store* modules=xlsModuleStoreCreate(NULL);xide_toolchains* cache=xideToolchainsCreate();xls_text* text=xlsTextCreate(main,strlen(main),strlen(main),NULL);xls_analysis_document doc={"entry",entry,text,1,1};CHECK(scope && modules && cache && text);
 workerCalls=0;xide_project_references* parallel=xideProjectReferencesCreate(cache,compiler,modules);CHECK(parallel && parallel->memberCount==XIDE_REFERENCE_MEMBER_WORKERS);xvalue *cold=NULL,*warm=NULL,*serial=NULL;char error[512]={0};xide_project_reference_stats stats;
 xide_reference_query* query=submit(parallel,scope,&doc);CHECK(query && collect(query,1,&cold,&stats,error));CHECK(xrtValueCount(cold)==34 && stats.captured==34 && !stats.reused && stats.memberTasks==33);
 CHECK(stats.memberWorkers==XIDE_REFERENCE_MEMBER_WORKERS && stats.memberPeak==XIDE_REFERENCE_MEMBER_WORKERS && stats.memberThreads==XIDE_REFERENCE_MEMBER_WORKERS);
 noWatch=1;query=submit(parallel,scope,&doc);CHECK(query && collect(query,1,&warm,&stats,error));noWatch=0;CHECK(stats.captured==1 && stats.reused==33 && !stats.memberTasks && !stats.memberPeak && !stats.memberThreads && equivalent(cold,warm));xrtValueRelease(warm);
 /* Optional extra-worker refusal keeps real serial analysis available. */
 workerCalls=0;failWorker=2;xide_project_references* fallback=xideProjectReferencesCreate(cache,compiler,modules);failWorker=0;CHECK(fallback && fallback->memberCount==1);
 noWatch=1;query=submit(fallback,scope,&doc);CHECK(query && collect(query,1,&serial,&stats,error) && stats.memberWorkers==1 && stats.memberPeak==1 && stats.memberThreads==1 && equivalent(cold,serial));noWatch=0;xrtValueRelease(serial);xrtValueRelease(cold);xideProjectReferencesDestroy(fallback);
 /* An owned warm pin must survive owner-cache removal. */
 CHECK(idle(parallel) && parallel->records.Count);reference_record* pin=retainRecord(records(parallel)[0]);size_t recordCount=parallel->records.Count;removeRecord(parallel,0);CHECK(parallel->records.Count+1==recordCount && xrtAtomic32Load(&pin->references,XMEMORY_ACQUIRE)==1 && pin->guard && pin->shard && overlayCurrent(pin,&(reference_input){.docs={doc},.count=1},NULL));releaseRecord(pin);xideProjectReferencesDestroy(parallel);
 /* Earlier source failure wins over a later admission failure;the successful
    prefix remains cached and no partial successful Location[] is published. */
 char *good=xrtPathJoin(directory,"01-good.xl"),*bad=xrtPathJoin(directory,"02-bad.xl"),*cannot=xrtPathJoin(directory,"03-cannot.xl");CHECK(good && bad && cannot && writeText(good,member) && writeText(bad,"func incomplete(\n") && writeText(cannot,member));
 const char* brokenMembers[]={entry,good,bad,cannot,shared};spec.lists[XLS_CONTEXT_MEMBERS]=(xls_context_strings){brokenMembers,5};xls_context* broken=xlsContextCreate(&spec,16u*1024u*1024u,NULL);workerCalls=0;parallel=xideProjectReferencesCreate(cache,compiler,modules);CHECK(broken && parallel);
 refusedPath="03-cannot.xl";query=submit(parallel,broken,&doc);CHECK(query && collect(query,0,&warm,&stats,error));refusedPath=NULL;CHECK(strstr(error,"complete verified reference facts") && idle(parallel) && parallel->records.Count==2);
 CHECK(writeText(bad,member));query=submit(parallel,broken,&doc);CHECK(query && collect(query,1,&warm,&stats,error) && xrtValueCount(warm)==5 && stats.reused==1 && stats.captured==4);xrtValueRelease(warm);xideProjectReferencesDestroy(parallel);xlsContextRelease(broken);
 /* Cancel only after four real member checks have been admitted,then prove
    coordinator and all children are idle before destroying their owners. */
 workerCalls=0;parallel=xideProjectReferencesCreate(cache,compiler,modules);admitted=xrtEventCreate(false,false);resumeAdmission=xrtEventCreate(false,false);CHECK(parallel && admitted && resumeAdmission);memberStarts=0;holdAdmissions=1;query=submit(parallel,scope,&doc);CHECK(query && xrtEventWaitFor(admitted,10000000)==XWAIT_OK);xideProjectReferencesCancel(query);xrtEventSet(resumeAdmission);CHECK(idle(parallel));holdAdmissions=0;CHECK(xideProjectReferencesResult(query,NULL,NULL,NULL,0)==XIDE_REFERENCES_CANCELLED && !xideProjectReferencesTake(query) && parallel->records.Count==1);
 for(size_t i=0;i<parallel->memberCount;++i){xls_worker_load load;CHECK(xlsWorkerLoad(parallel->members[i],&load) && !load.running && !load.queued);}
 xideProjectReferencesQueryDestroy(query);xideProjectReferencesDestroy(parallel);xrtEventDestroy(admitted);xrtEventDestroy(resumeAdmission);admitted=resumeAdmission=NULL;
 workerCalls=0;failWorker=1;CHECK(!xideProjectReferencesCreate(cache,compiler,modules));failWorker=0;
 xideToolchainsDestroy(cache);xlsModuleStoreRelease(modules);xlsContextRelease(scope);xlsTextRelease(text);for(unsigned i=0;i<32;++i)xrtFree(paths[i]);xrtFree(good);xrtFree(bad);xrtFree(cannot);xrtFree(entry);xrtFree(shared);xrtFree(project);xrtFree(compiler);xrtFree(directory);return 1;
}
int main(void){if(!run())return 1;printf("project_reference_parallel_test passed: %u checks;real full-check workers and shared modules,ordered complete Location parity,warm reuse,optional worker refusal,warm pin ownership,earlier error/prefix retention,deterministic admitted-batch cancellation and idle teardown\n",checks);return 0;}
