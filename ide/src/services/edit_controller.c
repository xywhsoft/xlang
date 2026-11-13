#include "services/edit_controller.h"
#include <string.h>
#include <stdio.h>
typedef struct edit_job {
 xide_edit_request* request;uint64_t connection;int line,character,cancel,drop;
 enum xide_edit_controller_state state;xide_edit_call* call;
 xide_edit_candidate* candidate;xvalue* message;size_t bytes;
 const char* uri;xstrview name;int remoteCode;char error[256];
} edit_job;
struct xide_edit_controller {
 xthread* worker;xmutex lock;xcond changed;int stop,paused,active;
 edit_job* job;uint64_t workerThread,retiredThread;void (*wake)(void*);void* user;
};
static const xvalue* get(const xvalue* v,const char* key)
{return xrtValueIs(v,XVALUE_OBJECT)?xrtValueObjectGet(v,xrtStrView(key)):NULL;}
static void signal(xide_edit_controller* c){xrtCondSignal(&c->changed);}
static void cancelLocked(xide_edit_controller* c)
{
 edit_job* j=c->job;if(!j)return;j->cancel=1;xideEditRequestInvalidate(j->request,XIDE_EDIT_REQUEST_CANCELLED);xideEditCallCancel(j->call);
 if(!c->active)j->state=XIDE_EDIT_CANCELLED;
 signal(c);
}
typedef struct edit_stop {xide_edit_controller* c;edit_job* job;} edit_stop;
static int stopped(void* data)
{edit_stop* s=data;xrtMutexLock(&s->c->lock);int stop=s->c->stop||s->c->paused||s->job->cancel||s->job->drop||xideEditRequestInvalidated(s->job->request);xrtMutexUnlock(&s->c->lock);return stop;}
static void freeJob(edit_job* j)
{if(j){xideEditCandidateRelease(j->candidate);xideEditCallRelease(j->call);xrtValueRelease(j->message);xideEditRequestRelease(j->request);xrtFree(j);}}
static int work(const xide_edit_controller* c)
{return c->job&&(c->job->drop||c->job->message||(!c->paused&&c->job->state==XIDE_EDIT_PREPARING));}
static int32 execute(ptr data)
{
 xide_edit_controller* c=data;
 for(;;){
  xrtMutexLock(&c->lock);while(!c->stop&&!work(c))xrtCondWait(&c->changed,&c->lock);
  if(c->stop){edit_job* j=c->job;c->job=NULL;c->active=1;xrtMutexUnlock(&c->lock);freeJob(j);break;}
  edit_job* j=c->job;c->active=1;c->workerThread=xrtThreadCurrentId();
  if(j->drop){c->job=NULL;xrtMutexUnlock(&c->lock);freeJob(j);xrtMutexLock(&c->lock);c->retiredThread=xrtThreadCurrentId();goto settled;}
  edit_stop stopping={c,j};
  if(j->state==XIDE_EDIT_PREPARING){
   xrtMutexUnlock(&c->lock);enum xide_edit_call_error why=XIDE_EDIT_CALL_OK;
   xide_edit_call* call=xideEditCallCreate(j->request,j->connection,j->uri,j->line,j->character,j->name,stopped,&stopping,&why);
   xrtMutexLock(&c->lock);j->call=call;
   if(j->cancel||j->drop||c->paused||xideEditRequestInvalidated(j->request)){xideEditCallCancel(call);j->state=XIDE_EDIT_CANCELLED;}
   else {j->state=call?XIDE_EDIT_PREPARED:XIDE_EDIT_REJECTED;if(!call)snprintf(j->error,sizeof(j->error),"Cannot prepare native rename call (%d).",why);}
  }else{
   xvalue* root=j->message;size_t bytes=j->bytes;j->message=NULL;j->bytes=0;j->state=XIDE_EDIT_PROCESSING;
   xrtMutexUnlock(&c->lock);xide_edit_reply* reply=NULL;xide_edit_candidate* candidate=NULL;enum xide_edit_call_error why=XIDE_EDIT_CALL_OK;xide_model_error error={0};int remote=0,code=0;char message[256]={0};
   if(!stopped(&stopping)){reply=xideEditCallReply(j->call,j->connection,root,bytes,&why);if(reply)root=NULL;}
   if(reply){xstrview reason={0};remote=xideEditReplyError(reply,&code,&reason);if(remote)snprintf(message,sizeof(message),"%.*s",(int)(reason.Size<sizeof(message)-1?reason.Size:sizeof(message)-1),reason.Data?reason.Data:"");else candidate=xideEditCandidatePrepare(j->call,reply,0,stopped,&stopping,&error);}
   xideEditReplyRelease(reply);xrtValueRelease(root);
   if(stopped(&stopping)){xideEditCandidateRelease(candidate);candidate=NULL;}
   xrtMutexLock(&c->lock);j->candidate=candidate;j->remoteCode=code;
   if(j->cancel||j->drop||c->paused||xideEditRequestInvalidated(j->request))j->state=XIDE_EDIT_CANCELLED;
   else if(candidate)j->state=XIDE_EDIT_READY;
   else {j->state=remote?XIDE_EDIT_REMOTE_ERROR:XIDE_EDIT_REJECTED;snprintf(j->error,sizeof(j->error),"%.255s",remote?message:*error.message?error.message:"Native candidate routing or data admission failed.");}
  }
settled:c->active=0;xrtMutexUnlock(&c->lock);if(c->wake)c->wake(c->user);
 }
 return 0;
}
xide_edit_controller* xideEditControllerCreate(void (*wake)(void*),void* user)
{
 xide_edit_controller* c=xrtCalloc(1,sizeof(*c));if(!c)return NULL;
 if(!xrtMutexInit(&c->lock)){xrtFree(c);return NULL;}if(!xrtCondInit(&c->changed)){xrtMutexUnit(&c->lock);xrtFree(c);return NULL;}
 c->wake=wake;c->user=user;c->worker=xrtThreadCreate(execute,c,0);if(!c->worker){xrtCondUnit(&c->changed);xrtMutexUnit(&c->lock);xrtFree(c);return NULL;}return c;
}
int xideEditControllerSubmit(xide_edit_controller* c,xide_edit_request* request,uint64_t connection,const char* uri,int line,int character,xstrview name)
{
 if(!c||!request||!connection||!uri||!name.Data||!name.Size||name.Size>4096||line<0||character<0||xideEditRequestInvalidated(request))return 0;
 size_t n=strlen(uri);if(!n||n>262144)return 0;
 edit_job* j=xrtCalloc(1,sizeof(*j)+n+1+name.Size);if(!j)return 0;j->request=xideEditRequestRetain(request);if(!j->request){xrtFree(j);return 0;}
 j->connection=connection;j->line=line;j->character=character;j->state=XIDE_EDIT_PREPARING;char* text=(char*)(j+1);memcpy(text,uri,n+1);j->uri=text;text+=n+1;memcpy(text,name.Data,name.Size);j->name=(xstrview){text,name.Size};
 xrtMutexLock(&c->lock);int okay=!c->stop&&!c->paused&&!c->active&&!c->job;if(okay){c->job=j;signal(c);}xrtMutexUnlock(&c->lock);
 /* A failed submission never became a worker job; request caller still owns
  * its original reference, so this shared-reference drop cannot retire it. */
 if(!okay){xideEditRequestRelease(j->request);xrtFree(j);}return okay;
}
int xideEditControllerStatus(xide_edit_controller* c,xide_edit_controller_status* out)
{
 if(out)*out=(xide_edit_controller_status){0};
 if(!c||!out)return 0;
 xrtMutexLock(&c->lock);edit_job* j=c->job;out->state=j?j->drop?XIDE_EDIT_RETIRING:j->state:XIDE_EDIT_IDLE;out->active=c->active;out->paused=c->paused;out->workerThread=c->workerThread;out->retiredThread=c->retiredThread;
 if(j){out->request=j->request;out->call=j->call;out->candidate=j->state==XIDE_EDIT_READY&&!j->cancel?j->candidate:NULL;out->remoteCode=j->remoteCode;memcpy(out->error,j->error,sizeof(out->error));}xrtMutexUnlock(&c->lock);return 1;
}
int xideEditControllerSend(xide_edit_controller* c,xide_language_client* client)
{
 if(!c)return 0;
 xrtMutexLock(&c->lock);edit_job* j=c->job;int okay=!c->stop&&!c->paused&&!c->active&&j&&!j->cancel&&!j->drop&&j->state==XIDE_EDIT_PREPARED&&xideEditCallSend(j->call,client);
 if(okay)j->state=XIDE_EDIT_WAITING;
 xrtMutexUnlock(&c->lock);return okay;
}
int xideEditControllerReceive(xide_edit_controller* c,uint64_t peer,xvalue* message,size_t bytes)
{
 int64 id=0;if(!c||!message||!bytes||bytes>XIDE_LSP_BODY_BYTES||!xrtValueIs(get(message,"id"),XVALUE_INT)||!xrtValueGetInt(get(message,"id"),&id))return 0;
 xrtMutexLock(&c->lock);edit_job* j=c->job;int take=!c->stop&&!c->paused&&j&&!j->cancel&&!j->drop&&j->state==XIDE_EDIT_WAITING&&!j->message&&peer==j->connection&&id==xideEditRequestId(j->request);
 if(take){j->message=message;j->bytes=bytes;signal(c);}xrtMutexUnlock(&c->lock);return take;
}
void xideEditControllerCancel(xide_edit_controller* c){if(c){xrtMutexLock(&c->lock);cancelLocked(c);xrtMutexUnlock(&c->lock);}}
int xideEditControllerCancelSend(xide_edit_controller* c,xide_language_client* client)
{if(!c)return 0;xrtMutexLock(&c->lock);cancelLocked(c);edit_job* j=c->job;int okay=!j||!j->call||xideEditCallCancelSend(j->call,client);xrtMutexUnlock(&c->lock);return okay;}
void xideEditControllerDrop(xide_edit_controller* c){if(c){xrtMutexLock(&c->lock);cancelLocked(c);if(c->job)c->job->drop=1;signal(c);xrtMutexUnlock(&c->lock);}}
void xideEditControllerSuspend(xide_edit_controller* c)
{if(c){xrtMutexLock(&c->lock);c->paused=1;if(c->job&&c->job->state!=XIDE_EDIT_READY)cancelLocked(c);signal(c);xrtMutexUnlock(&c->lock);}}
int xideEditControllerQuiet(void* data)
{xide_edit_controller* c=data;if(!c)return 1;xrtMutexLock(&c->lock);int quiet=c->paused&&!c->active&&!work(c);xrtMutexUnlock(&c->lock);return quiet;}
int xideEditControllerResume(xide_edit_controller* c)
{if(!c)return 1;xrtMutexLock(&c->lock);int okay=!c->stop&&c->paused&&!c->active&&!work(c);if(okay)c->paused=0;xrtMutexUnlock(&c->lock);return okay;}
void xideEditControllerDestroy(xide_edit_controller* c)
{if(c){xrtMutexLock(&c->lock);c->stop=1;cancelLocked(c);signal(c);xrtMutexUnlock(&c->lock);xrtThreadWait(c->worker);xrtThreadDestroy(c->worker);xrtCondUnit(&c->changed);xrtMutexUnit(&c->lock);xrtFree(c);}}
