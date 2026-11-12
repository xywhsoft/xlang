#include "services/source_barrier.h"
#include "app/xide_app.h"
#include "services/file_sync.h"
#include "services/toolchain.h"
#include "ui/language_source_lease.h"
#include "ui/edit_request_view.h"
#include <stdio.h>
#include <string.h>
enum {DOCUMENTS,JOBS,TASKS,TOOLS,WATCH,LANGUAGE};
static int barrierError(xide_model_error* e,const char* text)
{if(e){e->code=XIDE_MODEL_CONFLICT;snprintf(e->message,sizeof(e->message),"%s",text);}return 0;}
static void barrierFailed(xide_source_barrier* b,const xide_model_error* e)
{
 if(!b->failed){if(e&&e->code)b->failure=*e;else barrierError(&b->failure,"A source barrier participant could not be admitted.");}
 b->failed=1;
}
static int barrierToken(const xide_source_barrier* b,uint64_t token)
{return b&&token&&b->token==token&&b->app;}
static void barrierIdentity(xide_app* a,void** p)
{p[DOCUMENTS]=&a->documents;p[JOBS]=&a->sourceJobs;p[TASKS]=a->tasks;p[TOOLS]=xideTasksToolchains(a->tasks);p[WATCH]=a->fileSync;p[LANGUAGE]=a->languageView;p[6]=a->completionView;}
uint64_t xideSourceBarrierBegin(xide_source_barrier* b,xide_app* a,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 if(!b||!a||b->token||b->serial==UINT64_MAX||!a->context||!a->tasks||!a->fileSync||!a->languageView||!a->completionView){barrierError(e,"Source barrier is active, exhausted or missing initialized participants.");return 0;}
 uint64_t serial=b->serial+1;*b=(xide_source_barrier){.app=a,.serial=serial,.token=serial};barrierIdentity(a,b->identities);
 xide_model_error error={0};
 b->leases[DOCUMENTS]=xideDocumentsSuspendSources(&a->documents,&error);
 if(!b->leases[DOCUMENTS]){b->token=0;b->app=NULL;if(e)*e=error;return 0;}b->held|=XIDE_SOURCE_DOCUMENTS;
 b->leases[JOBS]=xideSourceJobsSuspend(&a->sourceJobs,&error);if(!b->leases[JOBS])goto failed;b->held|=XIDE_SOURCE_JOBS;
 b->leases[TASKS]=xideTasksSuspend(a->tasks,&error);if(!b->leases[TASKS])goto failed;b->held|=XIDE_SOURCE_TASKS;
 b->leases[TOOLS]=xideToolchainsSuspend(xideTasksToolchains(a->tasks),&error);if(!b->leases[TOOLS])goto failed;b->held|=XIDE_SOURCE_TOOLCHAINS;
 b->leases[WATCH]=xideFileSyncSuspend(a,&error);if(!b->leases[WATCH])goto failed;b->held|=XIDE_SOURCE_WATCH;
 b->leases[LANGUAGE]=xideLanguageSourcesSuspend(a,&error);if(!b->leases[LANGUAGE])goto failed;b->held|=XIDE_SOURCE_LANGUAGE;
 return b->token;
failed:barrierFailed(b,&error);if(e)*e=b->failure;return b->token;
}
uint64_t xideSourceBarrierBeginEdit(xide_source_barrier* b,xide_app* a,xide_edit_request* request,xide_model_error* e)
{
 if(!b||b->token||!xideLanguageEditRequestCurrent(a,request)){barrierError(e,"Edit request scope is stale or the barrier is already held.");return 0;}
 xide_edit_request* retained=xideEditRequestRetain(request);if(!retained){barrierError(e,"Cannot retain edit request scope.");return 0;}
 uint64_t token=xideSourceBarrierBegin(b,a,e);if(token)b->request=retained;else xideEditRequestRelease(retained);return token;
}
int xideSourceBarrierState(xide_source_barrier* b,uint64_t token,xide_source_barrier_status* out)
{
 if(out)*out=(xide_source_barrier_status){0};
 if(!out||!barrierToken(b,token))return 0;
 xide_app* a=b->app;void* identities[7];barrierIdentity(a,identities);unsigned lost=0,pending=0;
 if(b->request&&!xideLanguageEditRequestOwned(a,b->request))b->invalidated=1;
 for(unsigned i=0;i<6;++i)if((b->held&(1u<<i))&&(identities[i]!=b->identities[i]||(i==LANGUAGE&&identities[6]!=b->identities[6])))lost|=1u<<i;
 if((b->held&XIDE_SOURCE_DOCUMENTS)&&!(lost&XIDE_SOURCE_DOCUMENTS)&&!xideDocumentsSourcesSuspended(&a->documents,b->leases[DOCUMENTS]))lost|=XIDE_SOURCE_DOCUMENTS;
 if((b->held&XIDE_SOURCE_JOBS)&&!(lost&XIDE_SOURCE_JOBS)){
  xide_source_jobs_status s={0};if(!xideSourceJobsState(&a->sourceJobs,b->leases[JOBS],&s))lost|=XIDE_SOURCE_JOBS;else{if(!s.quiet)pending|=XIDE_SOURCE_JOBS;b->invalidated|=s.invalidated;}
 }
 if((b->held&XIDE_SOURCE_TASKS)&&!(lost&XIDE_SOURCE_TASKS)){
  if(!xideTasksSourceLeaseCurrent(a->tasks,b->leases[TASKS]))lost|=XIDE_SOURCE_TASKS;else if(!xideTasksSuspended(a->tasks,b->leases[TASKS]))pending|=XIDE_SOURCE_TASKS;
 }
 if((b->held&XIDE_SOURCE_TOOLCHAINS)&&!(lost&XIDE_SOURCE_TOOLCHAINS)){
  xide_toolchains* c=xideTasksToolchains(a->tasks);if(!xideToolchainsSourceLeaseCurrent(c,b->leases[TOOLS]))lost|=XIDE_SOURCE_TOOLCHAINS;else if(!xideToolchainsSuspended(c,b->leases[TOOLS]))pending|=XIDE_SOURCE_TOOLCHAINS;
 }
 if((b->held&XIDE_SOURCE_WATCH)&&!(lost&XIDE_SOURCE_WATCH)){
  if(!xideFileSyncSourceLeaseCurrent(a,b->leases[WATCH]))lost|=XIDE_SOURCE_WATCH;else if(!xideFileSyncSuspended(a,b->leases[WATCH]))pending|=XIDE_SOURCE_WATCH;
 }
 if((b->held&XIDE_SOURCE_LANGUAGE)&&!(lost&XIDE_SOURCE_LANGUAGE)){
  xide_language_sources_status s={0};if(!xideLanguageSourcesState(a,b->leases[LANGUAGE],&s))lost|=XIDE_SOURCE_LANGUAGE;else{
   b->invalidated|=s.invalidated;if(s.failed)barrierFailed(b,NULL);
   if(!(b->resuming?s.resumed:s.quiet))pending|=XIDE_SOURCE_LANGUAGE;
  }
 }
 if(lost)barrierFailed(b,NULL);
 int quiet=!lost&&!pending&&b->held==XIDE_SOURCE_ALL&&!b->resuming;
 *out=(xide_source_barrier_status){.held=b->held,.pending=pending,.lost=lost,.quiet=quiet,.ready=quiet&&!b->failed&&!b->invalidated,.failed=b->failed,.invalidated=b->invalidated,.resuming=b->resuming};return 1;
}
int xideSourceBarrierResume(xide_source_barrier* b,uint64_t token,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 xide_source_barrier_status s={0};if(!xideSourceBarrierState(b,token,&s)||s.lost)return barrierError(e,"Source barrier token or participant graph is stale.");
 if(b->resuming)return 1;
 b->resuming=1;
 if((b->held&XIDE_SOURCE_LANGUAGE)&&!xideLanguageSourcesResume(b->app,b->leases[LANGUAGE])){barrierFailed(b,NULL);if(e)*e=b->failure;}
 return 1;
}
int xideSourceBarrierRelease(xide_source_barrier* b,uint64_t token,xide_model_error* e)
{
 if(e)*e=(xide_model_error){0};
 xide_source_barrier_status s={0};if(!xideSourceBarrierState(b,token,&s)||!b->resuming||s.lost){barrierError(e,"Source barrier cannot release a stale, unresumed or replaced participant.");return -1;}
 if(s.pending&(XIDE_SOURCE_WATCH|XIDE_SOURCE_LANGUAGE))return 0;
 xide_app* a=b->app;
 if(b->held&XIDE_SOURCE_WATCH){if(!xideFileSyncResume(a,b->leases[WATCH],e))return -1;b->held&=~XIDE_SOURCE_WATCH;}
 if(b->held&XIDE_SOURCE_LANGUAGE){if(!xideLanguageSourcesRelease(a,b->leases[LANGUAGE])){barrierError(e,"Language sources have not released their acknowledged resume.");return -1;}b->held&=~XIDE_SOURCE_LANGUAGE;}
 if(b->held&XIDE_SOURCE_TOOLCHAINS){if(!xideToolchainsResume(xideTasksToolchains(a->tasks),b->leases[TOOLS],e))return -1;b->held&=~XIDE_SOURCE_TOOLCHAINS;}
 if(b->held&XIDE_SOURCE_TASKS){if(!xideTasksResume(a->tasks,b->leases[TASKS],e))return -1;b->held&=~XIDE_SOURCE_TASKS;}
 if(b->held&XIDE_SOURCE_JOBS){if(!xideSourceJobsResume(&a->sourceJobs,b->leases[JOBS],e))return -1;b->held&=~XIDE_SOURCE_JOBS;}
 if(b->held&XIDE_SOURCE_DOCUMENTS){if(!xideDocumentsResumeSources(&a->documents,b->leases[DOCUMENTS],e))return -1;b->held&=~XIDE_SOURCE_DOCUMENTS;}
 xideEditRequestRelease(b->request);b->request=NULL;b->token=0;b->app=NULL;return 1;
}
