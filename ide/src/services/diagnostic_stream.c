#include "services/diagnostic_stream.h"
#include <string.h>
#include <stdio.h>
#define SLOTS 2u
struct xide_diagnostic_batch {
 size_t count,bytes;xvalue* messages[XIDE_DIAGNOSTIC_BATCH_MESSAGES];
};
typedef struct diagnostic_job {
 uint64_t session,serial,epoch;xide_diagnostic_batch* batch;xls_sync* sync;
 xide_diagnostic_result* result;
} diagnostic_job;
typedef struct retirement {void* value;void (*release)(void*);} retirement;
struct xide_diagnostic_stream {
 xmutex lock;xcond changed;xthread* worker;int stop;
 uint64_t session,serial;
 diagnostic_job* jobs[SLOTS];size_t jobHead,jobCount;
 xide_diagnostic_result* ready[SLOTS];size_t readyHead,readyCount;
 xide_diagnostic_stream_stats stats;retirement retired;
 /* Only the worker accesses the mutable protocol registry. */
 xide_diagnostic_dependencies* registry;uint64_t registrySession;int poisoned;
 void (*wake)(void*);void* user;
};
xide_diagnostic_batch* xideDiagnosticBatchCreate(void) {return xrtCalloc(1,sizeof(xide_diagnostic_batch));}
int xideDiagnosticBatchAppend(xide_diagnostic_batch* b,xvalue* message,size_t bytes)
{
 if(!b || !message || !bytes || b->count>=XIDE_DIAGNOSTIC_BATCH_MESSAGES || bytes>XIDE_DIAGNOSTIC_BATCH_BYTES-b->bytes) return 0;
 b->messages[b->count++]=message;b->bytes+=bytes;return 1;
}
size_t xideDiagnosticBatchCount(const xide_diagnostic_batch* b) {return b ? b->count : 0;}
size_t xideDiagnosticBatchBytes(const xide_diagnostic_batch* b) {return b ? b->bytes : 0;}
void xideDiagnosticBatchRelease(xide_diagnostic_batch* b)
{if(b) {for(size_t i=0;i<b->count;++i) xrtValueRelease(b->messages[i]);xrtFree(b);}}
void xideDiagnosticResultRelease(xide_diagnostic_result* result)
{if(result) {for(size_t i=0;i<result->count;++i) xideDiagnosticNoticeRelease(result->notices[i]);xrtFree(result);}}
xide_diagnostic_notice* xideDiagnosticResultTakeNotice(xide_diagnostic_result* result,size_t index)
{
 if(!result || !result->valid || index>=result->count) return NULL;
 xide_diagnostic_notice* n=result->notices[index];result->notices[index]=NULL;return n;
}
static void releaseJob(diagnostic_job* job)
{if(job) {xideDiagnosticBatchRelease(job->batch);xlsSyncDestroy(job->sync);xideDiagnosticResultRelease(job->result);xrtFree(job);}}
static const xvalue* field(const xvalue* value,const char* name)
{return value && xrtValueType(value)==XVALUE_OBJECT ? xrtValueObjectGet(value,xrtStrView(name)) : NULL;}
static int textIs(const xvalue* value,const char* expected)
{xstrview text={0};return xrtValueGetString(value,&text) && text.Size==strlen(expected) && !memcmp(text.Data,expected,text.Size);}
typedef struct cancel_state {xide_diagnostic_stream* stream;uint64_t session;} cancel_state;
static int cancelled(void* user)
{
 cancel_state* c=user;xide_diagnostic_stream* s=c->stream;
 xrtMutexLock(&s->lock);int invalid=s->stop || s->session!=c->session;xrtMutexUnlock(&s->lock);return invalid;
}
static void notify(xide_diagnostic_stream* s) {if(s->wake) s->wake(s->user);}
static void process(xide_diagnostic_stream* s,diagnostic_job* job)
{
 xide_diagnostic_result* result=job->result;
 result->session=job->session;result->serial=job->serial;result->sourceEpoch=job->epoch;
 result->workerThreadId=xrtThreadCurrentId();result->messages=job->batch->count;result->wireBytes=job->batch->bytes;
 cancel_state c={s,job->session};
 if(cancelled(&c)) return;
 if(s->poisoned) {snprintf(result->error,sizeof(result->error),"Diagnostic stream requires a new connection session after invalid wire data.");return;}
 if(!s->registry) s->registry=xideDiagnosticDependenciesCreate(NULL);
 if(!s->registry) {snprintf(result->error,sizeof(result->error),"Cannot allocate diagnostic registry.");s->poisoned=1;return;}
 if(job->sync) {
  result->hasSync=xlsSyncInputState(job->sync,result->syncDigest,&result->syncDocuments);
  if(!result->hasSync) {snprintf(result->error,sizeof(result->error),"Invalid diagnostic source snapshot.");s->poisoned=1;return;}
 }
 for(size_t i=0;i<job->batch->count;++i) {
  if(cancelled(&c)) return;
  const xvalue* message=job->batch->messages[i],*params=field(message,"params");
  if(!textIs(field(message,"jsonrpc"),"2.0") || field(message,"id")) {snprintf(result->error,sizeof(result->error),"Expected an owned diagnostic notification.");s->poisoned=1;return;}
  if(textIs(field(message,"method"),"xlang/diagnosticDependencies")) {
   enum xide_dependency_message state=xideDiagnosticDependenciesMessage(s->registry,params,cancelled,&c);
   if(state==XIDE_DEPENDENCY_CANCELLED) return;
   if(state==XIDE_DEPENDENCY_ERROR) {snprintf(result->error,sizeof(result->error),"%s",xideDiagnosticDependenciesError(s->registry));s->poisoned=1;return;}
  } else if(textIs(field(message,"method"),"textDocument/publishDiagnostics")) {
   xide_diagnostic_notice* n=xideDiagnosticNoticeDecode(s->registry,params,cancelled,&c,result->error,sizeof(result->error));
   if(!n) {if(!cancelled(&c)) s->poisoned=1;return;}
   result->notices[result->count++]=n;
   if(!n->cleared && n->overlay) {
    xls_text* text=job->sync ? xlsSyncRetainText(job->sync,n->uri,n->version) : NULL;
    int mapped=text && xideDiagnosticNoticeMap(n,text,cancelled,&c);xlsTextRelease(text);
    if(!mapped) ++result->unmappedOverlays;
   }
  } else {snprintf(result->error,sizeof(result->error),"Unexpected method in diagnostic batch.");s->poisoned=1;return;}
 }
 if(cancelled(&c)) return;
 xideDiagnosticDependenciesStats(s->registry,&result->registry);result->valid=1;
}
static diagnostic_job* popJob(xide_diagnostic_stream* s)
{diagnostic_job* job=s->jobs[s->jobHead];s->jobs[s->jobHead]=NULL;s->jobHead=(s->jobHead+1)%SLOTS;--s->jobCount;return job;}
static xide_diagnostic_result* popReady(xide_diagnostic_stream* s)
{xide_diagnostic_result* r=s->ready[s->readyHead];s->ready[s->readyHead]=NULL;s->readyHead=(s->readyHead+1)%SLOTS;--s->readyCount;return r;}
static int32 run(ptr user)
{
 xide_diagnostic_stream* s=user;
 for(;;) {
  xrtMutexLock(&s->lock);
  while(!s->stop && !s->jobCount && !s->retired.value && s->registrySession==s->session &&
        !(s->readyCount && s->ready[s->readyHead]->session!=s->session)) xrtCondWait(&s->changed,&s->lock);
  if(s->stop) {xrtMutexUnlock(&s->lock);break;}
  if(s->registrySession!=s->session) {
   uint64_t session=s->session;xrtMutexUnlock(&s->lock);
   xideDiagnosticDependenciesDestroy(s->registry);s->registry=NULL;s->registrySession=session;s->poisoned=0;notify(s);continue;
  }
  if(s->readyCount && s->ready[s->readyHead]->session!=s->session) {
   xide_diagnostic_result* old=popReady(s);--s->stats.outstanding;++s->stats.discarded;xrtMutexUnlock(&s->lock);xideDiagnosticResultRelease(old);notify(s);continue;
  }
  if(s->jobCount && s->jobs[s->jobHead]->session!=s->session) {
   diagnostic_job* old=popJob(s);--s->stats.outstanding;++s->stats.cancelled;++s->stats.finished;xrtMutexUnlock(&s->lock);releaseJob(old);notify(s);continue;
  }
  if(s->retired.value) {
   retirement old=s->retired;s->retired=(retirement){0};xrtMutexUnlock(&s->lock);old.release(old.value);notify(s);continue;
  }
  diagnostic_job* job=popJob(s);s->stats.running=1;xrtMutexUnlock(&s->lock);
  process(s,job);uint64_t session=job->session;xide_diagnostic_result* result=job->result;job->result=NULL;releaseJob(job);
  xrtMutexLock(&s->lock);s->stats.running=0;++s->stats.finished;
  if(!s->stop && s->session==session) {s->ready[(s->readyHead+s->readyCount)%SLOTS]=result;++s->readyCount;result=NULL;}
  else {--s->stats.outstanding;++s->stats.cancelled;}
  xrtMutexUnlock(&s->lock);xideDiagnosticResultRelease(result);notify(s);
 }
 for(size_t i=0;i<s->jobCount;++i) releaseJob(s->jobs[(s->jobHead+i)%SLOTS]);
 for(size_t i=0;i<s->readyCount;++i) xideDiagnosticResultRelease(s->ready[(s->readyHead+i)%SLOTS]);
 if(s->retired.value) s->retired.release(s->retired.value);
 xideDiagnosticDependenciesDestroy(s->registry);s->registry=NULL;return 0;
}
xide_diagnostic_stream* xideDiagnosticStreamCreate(void (*wake)(void*),void* user)
{
 xide_diagnostic_stream* s=xrtCalloc(1,sizeof(*s));if(!s) return NULL;
 if(!xrtMutexInit(&s->lock)) {xrtFree(s);return NULL;}
 if(!xrtCondInit(&s->changed)) {xrtMutexUnit(&s->lock);xrtFree(s);return NULL;}
 s->wake=wake;s->user=user;s->worker=xrtThreadCreate(run,s,0);
 if(!s->worker) {xrtCondUnit(&s->changed);xrtMutexUnit(&s->lock);xrtFree(s);return NULL;}return s;
}
int xideDiagnosticStreamReset(xide_diagnostic_stream* s,uint64_t session)
{
 if(!s || !session) return 0;
 xrtMutexLock(&s->lock);int okay=!s->stop && session>=s->session;
 if(okay && session!=s->session) {s->session=session;xrtCondSignal(&s->changed);}
 xrtMutexUnlock(&s->lock);return okay;
}
int xideDiagnosticStreamSubmit(xide_diagnostic_stream* s,uint64_t session,uint64_t epoch,xide_diagnostic_batch* batch,const xls_sync* sync,uint64_t* serial)
{
 if(serial) *serial=0;
 if(!s || !session || !batch || !batch->count) return -1;
 xrtMutexLock(&s->lock);int valid=!s->stop && session==s->session && s->serial!=UINT64_MAX;
 int available=valid && s->stats.outstanding<SLOTS;
 if(!available) ++s->stats.rejected;
 xrtMutexUnlock(&s->lock);
 if(!available) return valid ? 0 : -1;
 diagnostic_job* job=xrtCalloc(1,sizeof(*job));if(!job) return -1;
 job->result=xrtCalloc(1,sizeof(*job->result));job->session=session;job->epoch=epoch;job->sync=sync ? xlsSyncClone(sync) : NULL;
 if(!job->result || (sync && !job->sync)) {releaseJob(job);return -1;}
 xrtMutexLock(&s->lock);valid=!s->stop && session==s->session && s->serial!=UINT64_MAX;available=valid && s->stats.outstanding<SLOTS;
 if(available) {
  job->serial=++s->serial;job->batch=batch;s->jobs[(s->jobHead+s->jobCount)%SLOTS]=job;++s->jobCount;++s->stats.outstanding;++s->stats.submitted;
  if(serial) *serial=job->serial;
  job=NULL;xrtCondSignal(&s->changed);
 } else ++s->stats.rejected;
 xrtMutexUnlock(&s->lock);releaseJob(job);return available ? 1 : valid ? 0 : -1;
}
xide_diagnostic_result* xideDiagnosticStreamTake(xide_diagnostic_stream* s)
{
 if(!s) return NULL;
 xrtMutexLock(&s->lock);xide_diagnostic_result* result=NULL;
 if(s->readyCount && s->ready[s->readyHead]->session==s->session) {result=popReady(s);--s->stats.outstanding;}
 xrtMutexUnlock(&s->lock);return result;
}
int xideDiagnosticStreamRetire(xide_diagnostic_stream* s,void* value,void (*release)(void*))
{
 if(!s || !value || !release) return 0;
 xrtMutexLock(&s->lock);int okay=!s->stop && !s->retired.value;
 if(okay) {s->retired=(retirement){value,release};xrtCondSignal(&s->changed);}
 xrtMutexUnlock(&s->lock);return okay;
}
int xideDiagnosticStreamLoad(xide_diagnostic_stream* s,xide_diagnostic_stream_stats* stats)
{
 if(!stats) return 0;
 *stats=(xide_diagnostic_stream_stats){0};if(!s) return 0;
 xrtMutexLock(&s->lock);*stats=s->stats;stats->queued=s->jobCount;stats->ready=s->readyCount;stats->retired=s->retired.value!=NULL;xrtMutexUnlock(&s->lock);return 1;
}
void xideDiagnosticStreamDestroy(xide_diagnostic_stream* s)
{
 if(!s) return;
 xrtMutexLock(&s->lock);s->stop=1;xrtCondSignal(&s->changed);xrtMutexUnlock(&s->lock);
 xrtThreadWait(s->worker);xrtThreadDestroy(s->worker);xrtCondUnit(&s->changed);xrtMutexUnit(&s->lock);xrtFree(s);
}
