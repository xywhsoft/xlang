#include "editor/native_edit_async.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
enum {PREPARE=1,STAGE,APPLY,FINISH,RETIRE,DROP};
typedef struct native_async_job {
 xide_edit_candidate* candidate;xide_native_edit* work;uint64_t token;
 const char* directory;const xide_edit_alias_exclusion* excluded;size_t count,budget;
 xatomic32 cancel,apply,restore;enum xide_native_async_state state;
 xide_edit_transaction_status status;xide_model_error error;
} native_async_job;
struct xide_native_edit_async {
 xthread* thread;xmutex lock;xcond changed;int active,pending,ownerBusy,stop;
 uint64_t owner,serial,workerThread,retiredThread;
 native_async_job* job;
 /* Owner only. The execute function and its job never borrow this ticket. */
 xide_native_edit_ticket* ticket;void (*wake)(void*);void* user;
};
static int owner(xide_native_edit_async* c){return c&&c->owner==xrtThreadCurrentId();}
static size_t pathBytes(const char* text)
{if(!text)return 0;size_t n=0;while(n<65536&&text[n])++n;return n&&n<65536?n+1:0;}
static int enter(xide_native_edit_async* c,uint64_t token)
{
 if(!owner(c))return 0;
 xrtMutexLock(&c->lock);
 if(!token||c->stop||c->active||c->pending||c->ownerBusy||!c->job||c->job->token!=token){xrtMutexUnlock(&c->lock);return 0;}
 return 1;
}
static int stopped(void* raw)
{native_async_job* j=raw;return xrtAtomic32Load(&j->cancel,XMEMORY_ACQUIRE)||!xrtAtomic32Load(&j->apply,XMEMORY_ACQUIRE)||!xideEditCandidateCurrent(j->candidate);}
static int environment(native_async_job* j)
{
 const xvalue* catalog=xideEditRequestCatalog(xideEditCallRequest(xideEditCandidateCall(j->candidate)));
 const xvalue* value=xrtValueObjectGet(catalog,xrtStrView("packagePath"));xstrview expected={0};
 int known=xrtValueIs(value,XVALUE_NULL)||xrtValueGetString(value,&expected);char* actual=NULL;xide_model_error ignored={0};
 int okay=known&&xideLanguageCheckPackagePath(&actual,&ignored);
 if(okay)okay=actual?expected.Data&&strlen(actual)==expected.Size&&!memcmp(actual,expected.Data,expected.Size):xrtValueIs(value,XVALUE_NULL);
 free(actual);return okay;
}
static int fence(void* raw,size_t document,int action)
{
 native_async_job* j=raw;
 if(action==XIDE_EDIT_FILE_RESTORE)return xrtAtomic32Load(&j->restore,XMEMORY_ACQUIRE)!=0;
 return action==XIDE_EDIT_FILE_APPLY&&!stopped(j)&&xrtAtomic32Load(&j->restore,XMEMORY_ACQUIRE)&&(document!=SIZE_MAX||environment(j));
}
static void releaseJob(native_async_job* j)
{if(j){xideEditCandidateRelease(j->candidate);xrtFree(j);}}
static void observe(native_async_job* j)
{if(j->work)(void)xideNativeEditStatus(j->work,&j->status);}
static int32 execute(ptr raw)
{
 xide_native_edit_async* c=raw;
 for(;;){
  xrtMutexLock(&c->lock);while(!c->pending&&!c->stop)xrtCondWait(&c->changed,&c->lock);
  if(c->stop){xrtMutexUnlock(&c->lock);break;}
  int command=c->pending;c->pending=0;c->active=1;c->workerThread=xrtThreadCurrentId();native_async_job* j=c->job;
  if(command==DROP){c->job=NULL;xrtMutexUnlock(&c->lock);releaseJob(j);xrtMutexLock(&c->lock);c->retiredThread=xrtThreadCurrentId();}
  else {
   xrtMutexUnlock(&c->lock);xide_model_error error={0};int okay=0;
   if(command==PREPARE){if(!stopped(j)&&environment(j))j->work=xideNativeEditPrepare(j->candidate,j->directory,j->excluded,j->count,j->budget,stopped,j,&error);okay=j->work!=NULL;}
   if(command==STAGE)okay=xideNativeEditStage(j->work,stopped,j,&error);
   if(command==APPLY)okay=xideNativeEditApply(j->work,fence,j,stopped,j,&error);
   if(command==FINISH)okay=xideNativeEditFinish(j->work,fence,j,&error);
   if(command==RETIRE){okay=xideNativeEditDestroy(j->work)==XUI_OK;if(okay)j->work=NULL;}
   xrtMutexLock(&c->lock);observe(j);
   if(error.code&&!j->error.code)j->error=error;
   if(!okay&&!j->error.code){j->error.code=XIDE_MODEL_CONFLICT;snprintf(j->error.message,sizeof(j->error.message),"Native edit worker step %d refused, failed or cancelled.",command);}
   if(command==PREPARE)j->state=okay?XIDE_NATIVE_PREPARED:XIDE_NATIVE_FAILED;
   if(command==STAGE)j->state=okay?XIDE_NATIVE_STAGED:XIDE_NATIVE_FAILED;
   if(command==APPLY)j->state=okay?XIDE_NATIVE_READY:j->status.rolledBack||j->status.needsRecovery?XIDE_NATIVE_FINISHED:XIDE_NATIVE_FAILED;
   if(command==FINISH)j->state=XIDE_NATIVE_FINISHED;
   if(command==RETIRE)j->state=okay?XIDE_NATIVE_RETIRED:XIDE_NATIVE_FAILED;
  }
  c->active=0;xrtCondBroadcast(&c->changed);xrtMutexUnlock(&c->lock);if(c->wake)c->wake(c->user);
 }
 return 0;
}
xide_native_edit_async* xideNativeAsyncCreate(void (*wake)(void*),void* user)
{
 xide_native_edit_async* c=xrtCalloc(1,sizeof(*c));if(!c)return NULL;
 if(!xrtMutexInit(&c->lock)){xrtFree(c);return NULL;}
 if(!xrtCondInit(&c->changed)){xrtMutexUnit(&c->lock);xrtFree(c);return NULL;}
 c->owner=xrtThreadCurrentId();c->wake=wake;c->user=user;c->thread=xrtThreadCreate(execute,c,0);
 if(!c->thread){xrtCondUnit(&c->changed);xrtMutexUnit(&c->lock);xrtFree(c);return NULL;}return c;
}
uint64_t xideNativeAsyncSubmit(xide_native_edit_async* c,xide_edit_candidate* candidate,const char* directory,const xide_edit_alias_exclusion* excluded,size_t count,size_t budget)
{
 if(!owner(c)||!candidate||!xideEditCandidateCurrent(candidate)||!directory||count>255||(count&&!excluded)||budget>512u*1024u*1024u)return 0;
 size_t lengths[256]={pathBytes(directory)},bytes=lengths[0];if(!bytes)return 0;
 for(size_t i=0;i<count;++i){size_t n=pathBytes(excluded[i].path);if(!n||n>1024u*1024u-bytes||(excluded[i].directory!=0&&excluded[i].directory!=1))return 0;lengths[i+1]=n;bytes+=n;}
 size_t metadata=sizeof(native_async_job)+count*sizeof(xide_edit_alias_exclusion);native_async_job* j=xrtCalloc(1,metadata+bytes);if(!j)return 0;
 j->candidate=xideEditCandidateRetain(candidate);if(!j->candidate){xrtFree(j);return 0;}
 j->count=count;j->budget=budget;j->state=XIDE_NATIVE_NEW;xrtAtomic32Init(&j->cancel,0);xrtAtomic32Init(&j->apply,0);xrtAtomic32Init(&j->restore,0);
 xide_edit_alias_exclusion* paths=(void*)(j+1);j->excluded=paths;char* text=(char*)j+metadata;size_t length=lengths[0];memcpy(text,directory,length);j->directory=text;text+=length;
 for(size_t i=0;i<count;++i){length=lengths[i+1];memcpy(text,excluded[i].path,length);paths[i]=(xide_edit_alias_exclusion){text,excluded[i].directory};text+=length;}
 xrtMutexLock(&c->lock);int okay=!c->stop&&!c->active&&!c->pending&&!c->ownerBusy&&!c->job&&c->serial<UINT64_MAX;
 if(okay){j->token=++c->serial;c->job=j;}xrtMutexUnlock(&c->lock);
 if(!okay){/* Caller still retains the source candidate, so this cannot be its
   * expensive last release. Failed submission never enters worker ownership. */releaseJob(j);return 0;}return j->token;
}
int xideNativeAsyncAuthorize(xide_native_edit_async* c,uint64_t token,int apply,int restore)
{
 if(!owner(c))return 0;
 xrtMutexLock(&c->lock);native_async_job* j=c->job;int okay=j&&j->token==token&&!c->stop;
 if(okay){xrtAtomic32Store(&j->apply,apply&&!xrtAtomic32Load(&j->cancel,XMEMORY_ACQUIRE),XMEMORY_RELEASE);xrtAtomic32Store(&j->restore,restore!=0,XMEMORY_RELEASE);}xrtMutexUnlock(&c->lock);return okay;
}
static int queue(xide_native_edit_async* c,uint64_t token,int command)
{
 if(!enter(c,token))return 0;
 native_async_job* j=c->job;int state=j->state,okay=0;enum xide_native_async_state next=XIDE_NATIVE_FAILED;
 if(command==PREPARE){okay=state==XIDE_NATIVE_NEW;next=XIDE_NATIVE_PREPARING;}
 if(command==STAGE){okay=state==XIDE_NATIVE_CAPTURED;next=XIDE_NATIVE_STAGING;}
 if(command==APPLY){okay=state==XIDE_NATIVE_ARMED;next=XIDE_NATIVE_APPLYING;}
 if(command==FINISH){okay=state==XIDE_NATIVE_PUBLISHED;next=XIDE_NATIVE_FINISHING;}
 if(command==RETIRE){okay=!c->ticket&&(state==XIDE_NATIVE_NEW||state==XIDE_NATIVE_PREPARED||state==XIDE_NATIVE_CAPTURED||state==XIDE_NATIVE_STAGED||state==XIDE_NATIVE_ARMED||state==XIDE_NATIVE_FAILED||state==XIDE_NATIVE_FINISHED);next=XIDE_NATIVE_RETIRING;}
 if(command==DROP){okay=state==XIDE_NATIVE_RETIRED&&!c->ticket;next=XIDE_NATIVE_DROPPING;}
 if(command<=APPLY)okay=okay&&!stopped(j)&&xrtAtomic32Load(&j->restore,XMEMORY_ACQUIRE);
 if(okay){j->state=next;c->pending=command;xrtCondSignal(&c->changed);}xrtMutexUnlock(&c->lock);return okay;
}
int xideNativeAsyncPrepare(xide_native_edit_async* c,uint64_t t){return queue(c,t,PREPARE);}
int xideNativeAsyncStage(xide_native_edit_async* c,uint64_t t){return queue(c,t,STAGE);}
int xideNativeAsyncApply(xide_native_edit_async* c,uint64_t t){return queue(c,t,APPLY);}
int xideNativeAsyncFinish(xide_native_edit_async* c,uint64_t t){return queue(c,t,FINISH);}
int xideNativeAsyncDrop(xide_native_edit_async* c,uint64_t t){return queue(c,t,DROP);}
int xideNativeAsyncRetire(xide_native_edit_async* c,uint64_t t,int finalShutdown)
{if(!enter(c,t))return 0;int okay=finalShutdown||!c->job->status.needsRecovery;xrtMutexUnlock(&c->lock);return okay&&queue(c,t,RETIRE);}
static void error(native_async_job* j,int status)
{if(status!=XUI_OK&&!j->error.code){j->error.code=XIDE_MODEL_CONFLICT;snprintf(j->error.message,sizeof(j->error.message),"Native edit owner step refused (%d).",status);}}
int xideNativeAsyncCapture(xide_native_edit_async* c,uint64_t t,xide_document_store* store)
{
 if(!enter(c,t))return XUI_ERROR_BUSY;
 native_async_job* j=c->job;if(j->state!=XIDE_NATIVE_PREPARED||c->ticket||stopped(j)){xrtMutexUnlock(&c->lock);return XUI_ERROR_INVALID_STATE;}
 c->ownerBusy=1;xrtMutexUnlock(&c->lock);xide_native_edit_ticket* ticket=NULL;int status=xideNativeEditCapture(store,j->work,&ticket);
 xrtMutexLock(&c->lock);c->ticket=ticket;j->state=status==XUI_OK?XIDE_NATIVE_CAPTURED:XIDE_NATIVE_FAILED;observe(j);error(j,status);c->ownerBusy=0;xrtMutexUnlock(&c->lock);return status;
}
int xideNativeAsyncArm(xide_native_edit_async* c,uint64_t t,int(*guard)(void*),void* user)
{
 if(!guard||!enter(c,t))return XUI_ERROR_BUSY;
 native_async_job* j=c->job;if(j->state!=XIDE_NATIVE_STAGED||!c->ticket||stopped(j)){xrtMutexUnlock(&c->lock);return XUI_ERROR_INVALID_STATE;}
 c->ownerBusy=1;xrtMutexUnlock(&c->lock);int status=xideNativeEditArm(c->ticket,guard,user);
 xrtMutexLock(&c->lock);if(status==XUI_OK)j->state=XIDE_NATIVE_ARMED;observe(j);error(j,status);c->ownerBusy=0;xrtMutexUnlock(&c->lock);return status;
}
int xideNativeAsyncPublish(xide_native_edit_async* c,uint64_t t,int(*guard)(void*),void* user,xide_open_edit_outcome* out)
{
 if(out)*out=(xide_open_edit_outcome){0};
 if(!guard||!out||!enter(c,t))return XUI_ERROR_BUSY;
 native_async_job* j=c->job;if(j->state!=XIDE_NATIVE_READY||!c->ticket){xrtMutexUnlock(&c->lock);return XUI_ERROR_INVALID_STATE;}
 c->ownerBusy=1;xrtMutexUnlock(&c->lock);int status=xideNativeEditPublish(c->ticket,guard,user,out);
 xrtMutexLock(&c->lock);observe(j);if(j->status.phase==XIDE_MIX_UI_COMMITTED||j->status.phase==XIDE_MIX_UI_REJECTED)j->state=XIDE_NATIVE_PUBLISHED;error(j,status);c->ownerBusy=0;xrtMutexUnlock(&c->lock);return status;
}
int xideNativeAsyncDetach(xide_native_edit_async* c,uint64_t t)
{
 if(!enter(c,t))return XUI_ERROR_BUSY;
 native_async_job* j=c->job;c->ownerBusy=1;xrtMutexUnlock(&c->lock);int status=xideNativeEditTicketDestroy(c->ticket);
 xrtMutexLock(&c->lock);if(status==XUI_OK)c->ticket=NULL;observe(j);if(j->status.phase==XIDE_MIX_UI_REJECTED||j->status.phase==XIDE_MIX_UI_COMMITTED)j->state=XIDE_NATIVE_PUBLISHED;error(j,status);c->ownerBusy=0;xrtMutexUnlock(&c->lock);return status;
}
void xideNativeAsyncCancel(xide_native_edit_async* c,uint64_t t)
{
 if(!owner(c))return;
 xrtMutexLock(&c->lock);native_async_job* j=c->job;
 if(j&&j->token==t){xrtAtomic32Store(&j->cancel,1,XMEMORY_RELEASE);xrtAtomic32Store(&j->apply,0,XMEMORY_RELEASE);xideEditRequestInvalidate((xide_edit_request*)xideEditCallRequest(xideEditCandidateCall(j->candidate)),XIDE_EDIT_REQUEST_CANCELLED);}
 xrtMutexUnlock(&c->lock);
}
int xideNativeAsyncStatus(xide_native_edit_async* c,xide_native_async_status* out)
{
 if(out)*out=(xide_native_async_status){0};
 if(!c||!out)return 0;
 xrtMutexLock(&c->lock);native_async_job* j=c->job;out->state=j?j->state:XIDE_NATIVE_IDLE;out->active=c->active||c->pending||c->ownerBusy;out->workerThread=c->workerThread;out->retiredThread=c->retiredThread;
 if(j){out->token=j->token;out->cancelled=xrtAtomic32Load(&j->cancel,XMEMORY_ACQUIRE);out->applyAllowed=xrtAtomic32Load(&j->apply,XMEMORY_ACQUIRE);out->restoreAllowed=xrtAtomic32Load(&j->restore,XMEMORY_ACQUIRE);out->transaction=j->status;out->error=j->error;out->candidate=j->candidate;out->directory=j->directory;}
 xrtMutexUnlock(&c->lock);return 1;
}
static void drain(xide_native_edit_async* c)
{xrtMutexLock(&c->lock);while(c->active||c->pending)xrtCondWait(&c->changed,&c->lock);xrtMutexUnlock(&c->lock);}
int xideNativeAsyncDestroy(xide_native_edit_async* c)
{
 if(!c)return XUI_OK;
 if(!owner(c))return XUI_ERROR_INVALID_ARGUMENT;
 xrtMutexLock(&c->lock);int busy=c->ownerBusy;uint64_t t=c->job?c->job->token:0;xrtMutexUnlock(&c->lock);if(busy)return XUI_ERROR_BUSY;
 if(t){xideNativeAsyncCancel(c,t);drain(c);if(xideNativeAsyncDetach(c,t)!=XUI_OK)return XUI_ERROR_BUSY;xide_native_async_status s={0};xideNativeAsyncStatus(c,&s);
  if(s.state==XIDE_NATIVE_PUBLISHED){if(!xideNativeAsyncFinish(c,t))return XUI_ERROR_BUSY;drain(c);}
  xideNativeAsyncStatus(c,&s);if(s.state!=XIDE_NATIVE_RETIRED){if(!xideNativeAsyncRetire(c,t,1))return XUI_ERROR_BUSY;drain(c);}
  if(!xideNativeAsyncDrop(c,t))return XUI_ERROR_BUSY;
  drain(c);
 }
 xrtMutexLock(&c->lock);c->stop=1;xrtCondSignal(&c->changed);xrtMutexUnlock(&c->lock);xrtThreadWait(c->thread);xrtThreadDestroy(c->thread);xrtCondUnit(&c->changed);xrtMutexUnit(&c->lock);xrtFree(c);return XUI_OK;
}
