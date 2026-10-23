#include "services/sync_plan.h"
#include <string.h>
#define SYNC_DOCUMENTS 64u
#define SYNC_TEXT_LIMIT (8u*1024u*1024u)
#define SYNC_TOTAL_LIMIT (32u*1024u*1024u)
typedef struct owned_input { xvalue *uri,*text;uint64_t identity,revision; } owned_input;
typedef struct sync_job { uint64_t token;owned_input inputs[SYNC_DOCUMENTS];size_t count;xide_sync_result* result; } sync_job;
struct xide_sync_planner {
    xthread* thread;xmutex lock;xcond changed;int stop;uint64_t current;
    sync_job* pending;xide_sync_result *completed,*retired;
    void (*wake)(void*);void* user;
};
void xideSyncResultRelease(xide_sync_result* result)
{ if(result) {for(size_t i=0;i<result->operationCount;++i) xrtValueRelease(result->operations[i].uri);xlsSyncDestroy(result->sync);xideLanguageBatchDestroy(result->batch);xrtFree(result);} }
static void releaseJob(sync_job* job)
{
    if(!job) return;
    for(size_t i=0;i<job->count;++i) {xrtValueRelease(job->inputs[i].uri);xrtValueRelease(job->inputs[i].text);}
    xideSyncResultRelease(job->result);xrtFree(job);
}
static int current(xide_sync_planner* planner,uint64_t token)
{ xrtMutexLock(&planner->lock);int okay=!planner->stop && planner->current==token;xrtMutexUnlock(&planner->lock);return okay; }
static int collect(void* user,const char* method,const xvalue* params)
{
 xide_sync_result* result=user;const xvalue* document=xrtValueObjectGet(params,xrtStrView("textDocument"));
 const xvalue* uri=xrtValueObjectGet(document,xrtStrView("uri"));
 if(result->operationCount==128 || !uri || !xideLanguageBatchAppend(result->batch,method,params)) return 0;
 result->operations[result->operationCount++]=(xide_sync_operation){xrtValueRetain(uri),!strcmp(method,"textDocument/didClose")};return 1;
}
static int32 run(ptr user)
{
    xide_sync_planner* planner=user;
    for(;;) {
        sync_job* job=NULL;xide_sync_result* garbage=NULL;
        xrtMutexLock(&planner->lock);
        while(!planner->stop && !planner->pending && !planner->retired &&
              !(planner->completed && planner->completed->token!=planner->current))
            xrtCondWait(&planner->changed,&planner->lock);
        if(planner->stop) {xrtMutexUnlock(&planner->lock);break;}
        if(planner->retired) {garbage=planner->retired;planner->retired=NULL;}
        else if(planner->completed && planner->completed->token!=planner->current) {garbage=planner->completed;planner->completed=NULL;}
        else {job=planner->pending;planner->pending=NULL;}
        xrtMutexUnlock(&planner->lock);
        if(garbage) {xideSyncResultRelease(garbage);if(planner->wake) planner->wake(planner->user);continue;}
        xide_sync_result* result=job->result;job->result=NULL;
        if(current(planner,job->token)) {
            xls_sync_document documents[SYNC_DOCUMENTS];int okay=1;
            for(size_t i=0;i<job->count;++i) {
                xstrview uri={0},text={0};owned_input* input=job->inputs+i;
                if(!xrtValueGetString(input->uri,&uri) || !xrtValueGetString(input->text,&text)) {okay=0;break;}
                documents[i]=(xls_sync_document){uri.Data,text.Data ? text.Data : "",text.Size,input->identity,input->revision};
            }
            result->batch=xideLanguageBatchCreate();
            okay=okay && result->batch && xlsSyncUpdate(result->sync,documents,job->count,collect,result) &&
                xlsSyncInputState(result->sync,result->digest,&result->count);
            for(size_t i=0;okay && i<job->count;++i) {
                result->versions[i]=xlsSyncVersion(result->sync,documents[i].uri);if(!result->versions[i]) okay=0;
            }
            result->valid=okay;
        }
        uint64_t token=job->token;releaseJob(job);
        xrtMutexLock(&planner->lock);
        int publish=!planner->stop && planner->current==token;
        if(publish) {garbage=planner->completed;planner->completed=result;result=NULL;}
        xrtMutexUnlock(&planner->lock);
        xideSyncResultRelease(garbage);xideSyncResultRelease(result);
        if(planner->wake) planner->wake(planner->user);
    }
    releaseJob(planner->pending);xideSyncResultRelease(planner->completed);xideSyncResultRelease(planner->retired);return 0;
}
xide_sync_planner* xideSyncPlannerCreate(void (*wake)(void*),void* user)
{
    xide_sync_planner* planner=xrtCalloc(1,sizeof(*planner));if(!planner) return NULL;
    if(!xrtMutexInit(&planner->lock)) {xrtFree(planner);return NULL;}
    if(!xrtCondInit(&planner->changed)) {xrtMutexUnit(&planner->lock);xrtFree(planner);return NULL;}
    planner->wake=wake;planner->user=user;planner->thread=xrtThreadCreate(run,planner,0);
    if(!planner->thread) {xrtCondUnit(&planner->changed);xrtMutexUnit(&planner->lock);xrtFree(planner);return NULL;}return planner;
}
int xideSyncPlannerSubmit(xide_sync_planner* planner,uint64_t token,const xls_sync* base,const xide_sync_input* inputs,size_t count)
{
    if(!planner || !token || !base || count>SYNC_DOCUMENTS || (count && !inputs)) return -1;
    xrtMutexLock(&planner->lock);int available=!planner->stop && !planner->pending;xrtMutexUnlock(&planner->lock);if(!available) return 0;
    sync_job* job=xrtCalloc(1,sizeof(*job));if(!job) return -1;
    job->token=token;job->result=xrtCalloc(1,sizeof(*job->result));size_t total=0;
    if(!job->result || !(job->result->sync=xlsSyncClone(base))) {releaseJob(job);return -1;}
    job->result->token=token;
    for(size_t i=0;i<count;++i) {
        xstrview text={0};const xide_sync_input* input=inputs+i;
        if(!input->uri || !*input->uri || strlen(input->uri)>262144 || !xrtValueGetString(input->text,&text) ||
           text.Size>SYNC_TEXT_LIMIT || text.Size>SYNC_TOTAL_LIMIT-total) {releaseJob(job);return -1;}
        total+=text.Size;owned_input* owned=job->inputs+job->count++;
        owned->uri=xrtValueString(xrtStrView(input->uri));owned->text=xrtValueRetain(input->text);
        owned->identity=input->identity;owned->revision=input->revision;
        if(!owned->uri || !owned->text) {releaseJob(job);return -1;}
    }
    xrtMutexLock(&planner->lock);available=!planner->stop && !planner->pending;
    if(available) {planner->current=token;planner->pending=job;job=NULL;xrtCondSignal(&planner->changed);}
    xrtMutexUnlock(&planner->lock);releaseJob(job);return available;
}
void xideSyncPlannerCancel(xide_sync_planner* planner)
{ if(planner) {xrtMutexLock(&planner->lock);planner->current=0;xrtCondSignal(&planner->changed);xrtMutexUnlock(&planner->lock);} }
xide_sync_result* xideSyncPlannerTake(xide_sync_planner* planner)
{
    if(!planner) return NULL;
    xrtMutexLock(&planner->lock);xide_sync_result* result=planner->completed;planner->completed=NULL;xrtMutexUnlock(&planner->lock);return result;
}
int xideSyncPlannerRetire(xide_sync_planner* planner,xide_sync_result* result)
{
    if(!planner || !result) return 0;
    xrtMutexLock(&planner->lock);int okay=!planner->stop && !planner->retired;
    if(okay) {planner->retired=result;xrtCondSignal(&planner->changed);}xrtMutexUnlock(&planner->lock);return okay;
}
void xideSyncPlannerDestroy(xide_sync_planner* planner)
{
    if(!planner) return;
    xrtMutexLock(&planner->lock);planner->stop=1;planner->current=0;xrtCondSignal(&planner->changed);xrtMutexUnlock(&planner->lock);
    xrtThreadWait(planner->thread);xrtThreadDestroy(planner->thread);xrtCondUnit(&planner->changed);xrtMutexUnit(&planner->lock);xrtFree(planner);
}
