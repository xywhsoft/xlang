#include "services/language_client.h"
#include "platform/process.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define QUEUE_BYTES (64u*1024u*1024u)
#define QUEUE_COUNT 2048u
typedef struct node { struct node* next; char* bytes; size_t length,offset; xvalue* value; } node;
struct xide_language_batch { node *head,*tail;size_t bytes,count; };
struct xide_language_client { CRITICAL_SECTION lock; HANDLE worker; volatile LONG stop; char *program,*directory; char** arguments; size_t argumentCount; enum xide_language_client_state state; char error[768]; node *sendHead,*sendTail,*receiveHead,*receiveTail; size_t sendBytes,receiveBytes,sendCount,receiveCount; void (*wake)(void*); void* wakeUser; };
static char* copy(const char* s) { size_t n=strlen(s)+1; char* p=malloc(n); if(p) memcpy(p,s,n); return p; }
static int fail(xide_model_error* e,const char* s) { if(e) { e->code=XIDE_MODEL_INVALID; snprintf(e->message,sizeof(e->message),"%s",s); } return 0; }
static void clear(node* n) { while(n) { node* next=n->next; free(n->bytes); xrtValueRelease(n->value); free(n); n=next; } }
xide_language_batch* xideLanguageBatchCreate(void)
{ return calloc(1,sizeof(xide_language_batch)); }
void xideLanguageBatchDestroy(xide_language_batch* batch)
{ if(batch) {clear(batch->head);free(batch);} }
int xideLanguageBatchAppend(xide_language_batch* batch,const char* method,const xvalue* params)
{
 if(!batch || !method || !*method || !params || batch->count>=QUEUE_COUNT) return 0;
 xvalue* message=xrtValueObject();node* n=calloc(1,sizeof(*n));xide_model_error error={0};
 int okay=message && n && xrtValueObjectSetNew(message,xrtStrView("jsonrpc"),xrtValueString(xrtStrView("2.0"))) &&
  xrtValueObjectSetNew(message,xrtStrView("method"),xrtValueString(xrtStrView(method))) &&
  xrtValueObjectSet(message,xrtStrView("params"),params);
 if(okay) n->bytes=xideLspFrame(message,&n->length,&error);
 okay=okay && n->bytes && n->length<=QUEUE_BYTES-batch->bytes;xrtValueRelease(message);
 if(!okay) {clear(n);return 0;}
 if(batch->tail) batch->tail->next=n;else batch->head=n;
 batch->tail=n;batch->bytes+=n->length;++batch->count;return 1;
}
int xideLanguageBatchSend(xide_language_client* c,xide_language_batch* batch)
{
 if(!c || !batch) return 0;
 EnterCriticalSection(&c->lock);
 int okay=c->state<=XIDE_LS_RUNNING && batch->count<=QUEUE_COUNT-c->sendCount && batch->bytes<=QUEUE_BYTES-c->sendBytes;
 if(okay && batch->head) {
  if(c->sendTail) c->sendTail->next=batch->head;else c->sendHead=batch->head;
  c->sendTail=batch->tail;c->sendCount+=batch->count;c->sendBytes+=batch->bytes;
 }
 LeaveCriticalSection(&c->lock);if(okay) free(batch);return okay;
}
static void wakeOwner(xide_language_client* c)
{ EnterCriticalSection(&c->lock); void (*wake)(void*)=c->wake; void* user=c->wakeUser; LeaveCriticalSection(&c->lock); if(wake) wake(user); }
void xideLanguageClientSetWake(xide_language_client* c,void (*wake)(void*),void* user)
{ if(!c) return; EnterCriticalSection(&c->lock); c->wake=wake; c->wakeUser=user; int pending=c->receiveHead!=NULL || c->state>=XIDE_LS_STOPPED; LeaveCriticalSection(&c->lock); if(pending) wakeOwner(c); }
static int receive(xvalue* value,size_t length,void* user)
{
 xide_language_client* c=user;node* n=calloc(1,sizeof(*n));if(!n) return 0;
 n->value=value;n->length=length;
 EnterCriticalSection(&c->lock); int okay=c->receiveCount<QUEUE_COUNT && length<=QUEUE_BYTES-c->receiveBytes;
 if(okay) { if(c->receiveTail) c->receiveTail->next=n; else c->receiveHead=n; c->receiveTail=n; ++c->receiveCount; c->receiveBytes+=length; }
 LeaveCriticalSection(&c->lock);
 /* Only success takes the root. The stream releases rejected frames after
    this callback returns, outside the queue lock. Count original wire bytes. */
 if(!okay) {n->value=NULL;clear(n);}else wakeOwner(c);return okay;
}
static DWORD WINAPI worker(void* user)
{
 xide_language_client* c=user; xide_lsp_stream stream={0}; xide_model_error e={0}; xide_process_info info={0}; char stderrTail[256]={0};
 xide_process_spec spec={.program=c->program,.directory=c->directory,.arguments=(const char* const*)c->arguments,.argumentCount=c->argumentCount,.interactive=1,.separateStderr=1,.stdoutCapacity=8u*1024u*1024u+16384u}; xide_process* p=xideProcessStart(&spec,e.message,sizeof(e.message)); int okay=p!=NULL;
 while(okay && !InterlockedCompareExchange(&c->stop,0,0)) {
  xideProcessGetInfo(p,&info); if(info.started) { EnterCriticalSection(&c->lock); c->state=XIDE_LS_RUNNING; LeaveCriticalSection(&c->lock); }
  node* sent=NULL;EnterCriticalSection(&c->lock); node* n=c->sendHead;
  if(n) { size_t k=n->length-n->offset; if(k>16384) k=16384; if(xideProcessWrite(p,n->bytes+n->offset,k)) n->offset+=k;
   if(n->offset==n->length) { c->sendHead=n->next; if(!c->sendHead) c->sendTail=NULL; c->sendBytes-=n->length; --c->sendCount; n->next=NULL; sent=n; }
  }
  LeaveCriticalSection(&c->lock);
  clear(sent); /* Large encoded bodies are retired outside the owner lock. */
  char bytes[16384]; size_t k=xideProcessRead(p,bytes,sizeof(bytes)); if(k && !xideLspStreamFeedOwned(&stream,bytes,k,receive,c,&e)) { okay=0; break; }
  size_t warning=xideProcessReadError(p,bytes,sizeof(bytes)); if(warning) { size_t keep=warning<sizeof(stderrTail)-1 ? warning : sizeof(stderrTail)-1; memcpy(stderrTail,bytes+warning-keep,keep); stderrTail[keep]=0; }
  xideProcessGetInfo(p,&info);
  if(info.ioError || info.droppedBytes) { fail(&e,*info.error ? info.error : "语言服务协议输出被丢弃。"); okay=0; break; }
  if(info.state==XIDE_PROCESS_FINISHED && !k && !warning) { okay=xideLspStreamEnd(&stream,&e) && info.started && info.exit.Kind==XPROCESS_EXIT_CODE && info.exit.Code==0; if(*stderrTail && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,stderrTail,-1,NULL,0)) strcpy(stderrTail,"stderr contains binary or split UTF-8"); if(!okay && !*e.message) snprintf(e.message,sizeof(e.message),"语言服务异常退出（%d）；%.180s",info.exit.Code,stderrTail); break; }
  if(!k && !warning) Sleep(5);
 }
 if(p) { xideProcessDestroy(p); } xideLspStreamClear(&stream);
 EnterCriticalSection(&c->lock); c->state=okay || InterlockedCompareExchange(&c->stop,0,0) ? XIDE_LS_STOPPED : XIDE_LS_FAILED; snprintf(c->error,sizeof(c->error),"%s",e.message); LeaveCriticalSection(&c->lock); wakeOwner(c); return 0;
}
static int validArgument(const char* text,size_t* bytes,int required)
{
 if(!text || (required && !*text)) return 0;
 size_t n=strlen(text); if(n>131072 || n>512u*1024u-*bytes || (n && !MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text,(int)n,NULL,0))) return 0;
 *bytes+=n; return 1;
}
xide_language_client* xideLanguageClientStartProcess(const xide_language_client_spec* spec,xide_model_error* e)
{
 size_t bytes=0;
 if(!spec || spec->argumentCount>64 || (spec->argumentCount && !spec->arguments) || !validArgument(spec->program,&bytes,1) || !validArgument(spec->directory,&bytes,1)) { fail(e,"Invalid stdio server specification."); return NULL; }
 for(size_t i=0;i<spec->argumentCount;++i) if(!validArgument(spec->arguments[i],&bytes,0)) { fail(e,"Invalid stdio server argument."); return NULL; }
 xide_language_client* c=calloc(1,sizeof(*c)); if(!c) { fail(e,"Cannot allocate stdio client."); return NULL; }
 InitializeCriticalSection(&c->lock); c->program=copy(spec->program); c->directory=copy(spec->directory); c->argumentCount=spec->argumentCount;
 c->arguments=calloc(spec->argumentCount ? spec->argumentCount : 1,sizeof(*c->arguments)); int okay=c->program && c->directory && c->arguments;
 for(size_t i=0;okay && i<spec->argumentCount;++i) { c->arguments[i]=copy(spec->arguments[i]); okay=c->arguments[i]!=NULL; }
 if(okay) c->worker=CreateThread(NULL,0,worker,c,0,NULL);
 if(!c->worker) { xideLanguageClientStop(c); fail(e,"Cannot start background stdio transport."); return NULL; } return c;
}
xide_language_client* xideLanguageClientStart(const char* program,const char* compiler,const char* directory,xide_model_error* e)
{
 if(!compiler) { fail(e,"Language compiler must not be null."); return NULL; }
 const char* arguments[]={"--compiler",compiler}; xide_language_client_spec spec={program,directory,arguments,2}; return xideLanguageClientStartProcess(&spec,e);
}
int xideLanguageClientSend(xide_language_client* c,const xvalue* message,xide_model_error* e)
{
 if(!c) { return fail(e,"语言服务未启动。"); } node* n=calloc(1,sizeof(*n)); if(!n) return fail(e,"无法分配语言服务发送队列。"); n->bytes=xideLspFrame(message,&n->length,e); if(!n->bytes) { clear(n); return 0; }
 EnterCriticalSection(&c->lock); int okay=c->state<=XIDE_LS_RUNNING && c->sendCount<QUEUE_COUNT && n->length<=QUEUE_BYTES-c->sendBytes;
 if(okay) { if(c->sendTail) c->sendTail->next=n; else c->sendHead=n; c->sendTail=n; ++c->sendCount; c->sendBytes+=n->length; } LeaveCriticalSection(&c->lock);
 if(!okay) { clear(n); return fail(e,"语言服务已结束或发送队列达到上限。"); } return 1;
}
xvalue* xideLanguageClientNext(xide_language_client* c)
{
 if(!c) { return NULL; } EnterCriticalSection(&c->lock); node* n=c->receiveHead; xvalue* value=NULL;
 if(n) { c->receiveHead=n->next; if(!c->receiveHead) c->receiveTail=NULL; --c->receiveCount; c->receiveBytes-=n->length; value=n->value; n->value=NULL; n->next=NULL; clear(n); }
 LeaveCriticalSection(&c->lock); return value;
}
int xideLanguageClientHasPending(xide_language_client* c)
{ if(!c) return 0; EnterCriticalSection(&c->lock); int pending=c->receiveHead!=NULL; LeaveCriticalSection(&c->lock); return pending; }
enum xide_language_client_state xideLanguageClientState(xide_language_client* c,char* message,size_t capacity)
{ if(!c) return XIDE_LS_STOPPED; EnterCriticalSection(&c->lock); enum xide_language_client_state state=c->state; if(message && capacity) snprintf(message,capacity,"%s",c->error); LeaveCriticalSection(&c->lock); return state; }
void xideLanguageClientStop(xide_language_client* c)
{ if(c) { InterlockedExchange(&c->stop,1); if(c->worker) { WaitForSingleObject(c->worker,INFINITE); CloseHandle(c->worker); } clear(c->sendHead); clear(c->receiveHead); free(c->program); free(c->directory); if(c->arguments) for(size_t i=0;i<c->argumentCount;++i) free(c->arguments[i]); free(c->arguments); DeleteCriticalSection(&c->lock); free(c); } }
