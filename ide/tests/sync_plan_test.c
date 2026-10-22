#define _WIN32_WINNT 0x0601
#include "services/sync_plan.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Private queue/worker access makes allocation, backpressure and publication
   races deterministic without adding fault hooks to the shipped API. */
static int frameCalls,failFrame,failAllocation;
static char* testFrame(const xvalue* value,size_t* size,xide_model_error* error)
{ ++frameCalls;return failFrame ? NULL : xideLspFrame(value,size,error); }
static void* testCalloc(size_t count,size_t size)
{ return failAllocation ? NULL : calloc(count,size); }
#define xideLspFrame testFrame
#define calloc testCalloc
#include "../src/services/language_client.c"
#undef calloc
#undef xideLspFrame
#include "../src/services/sync_plan.c"
static int checks;
#define CHECK(e) do {++checks;if(!(e)){fprintf(stderr,"sync_plan_test:%d %s\n",__LINE__,#e);return 1;}}while(0)
static const xvalue* get(const xvalue* v,const char* key)
{ return xrtValueObjectGet(v,xrtStrView(key)); }
static int integer(const xvalue* v) {int64 n=-1;xrtValueGetInt(v,&n);return (int)n;}
static int textIs(const xvalue* v,const char* text)
{ xstrview s={0};return xrtValueGetString(v,&s) && s.Size==strlen(text) && !memcmp(s.Data,text,s.Size); }
typedef struct messages { xvalue* values[256];size_t count; } messages;
static int decoded(const xvalue* v,void* user)
{messages* m=user;if(m->count==256) return 0;m->values[m->count++]=xrtValueDeepClone(v);return m->values[m->count-1]!=NULL;}
static int drain(xide_language_client* client,messages* m)
{
 xide_lsp_stream stream={0};xide_model_error error={0};int okay=1;
 for(node* n=client->sendHead;n && okay;n=n->next)
  for(size_t at=0;at<n->length && okay;) {size_t step=n->length-at;if(step>7) step=7;okay=xideLspStreamFeed(&stream,n->bytes+at,step,decoded,m,&error);at+=step;}
 okay=okay && xideLspStreamEnd(&stream,&error);xideLspStreamClear(&stream);clear(client->sendHead);
 client->sendHead=client->sendTail=NULL;client->sendCount=client->sendBytes=0;return okay;
}
static void clearMessages(messages* m)
{for(size_t i=0;i<m->count;++i) xrtValueRelease(m->values[i]);memset(m,0,sizeof(*m));}
static int batches(void)
{
 xide_language_client client={0};InitializeCriticalSection(&client.lock);messages m={0};
 xvalue* params=xrtValueObject();CHECK(params && xrtValueObjectSetNew(params,xrtStrView("n"),xrtValueInt(1)));
 xide_language_batch* batch=xideLanguageBatchCreate();CHECK(batch && !xideLanguageBatchSend(NULL,batch));
 failFrame=1;CHECK(!xideLanguageBatchAppend(batch,"first",params) && !batch->head && !batch->count && !batch->bytes);failFrame=0;
 failAllocation=1;CHECK(!xideLanguageBatchAppend(batch,"first",params) && !batch->head);failAllocation=0;
 CHECK(xideLanguageBatchAppend(batch,"first",params));size_t bytes=batch->bytes;node* first=batch->head;
 failFrame=1;CHECK(!xideLanguageBatchAppend(batch,"failed",params) && batch->count==1 && batch->bytes==bytes && batch->tail==first && !first->next);failFrame=0;
 batch->count=QUEUE_COUNT;CHECK(!xideLanguageBatchAppend(batch,"limit",params) && batch->head==first);batch->count=1;
 batch->bytes=QUEUE_BYTES-1;CHECK(!xideLanguageBatchAppend(batch,"limit",params) && batch->tail==first);batch->bytes=bytes;
 CHECK(xrtValueObjectSetNew(params,xrtStrView("n"),xrtValueInt(2)) && xideLanguageBatchAppend(batch,"second",params));
 client.sendCount=QUEUE_COUNT-1;CHECK(!xideLanguageBatchSend(&client,batch) && !client.sendHead && batch->count==2);client.sendCount=0;
 client.sendBytes=QUEUE_BYTES-1;CHECK(!xideLanguageBatchSend(&client,batch) && !client.sendHead && batch->count==2);client.sendBytes=0;
 client.state=XIDE_LS_STOPPED;CHECK(!xideLanguageBatchSend(&client,batch) && batch->head==first);client.state=XIDE_LS_RUNNING;
 int frames=frameCalls;failFrame=failAllocation=1;CHECK(xideLanguageBatchSend(&client,batch) && frameCalls==frames && client.sendHead==first && client.sendCount==2);failFrame=failAllocation=0;
 xvalue* query=xrtJsonParse(xrtStrView("{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"after\"}"));CHECK(query && xideLanguageClientSend(&client,query,NULL));xrtValueRelease(query);
 CHECK(drain(&client,&m) && m.count==3 && textIs(get(m.values[0],"method"),"first") && integer(get(get(m.values[0],"params"),"n"))==1 && integer(get(get(m.values[1],"params"),"n"))==2 && integer(get(m.values[2],"id"))==3);
 clearMessages(&m);batch=xideLanguageBatchCreate();CHECK(batch && xideLanguageBatchSend(&client,batch) && !client.sendHead);
 batch=xideLanguageBatchCreate();CHECK(batch && xideLanguageBatchAppend(batch,"retired",params));xideLanguageBatchDestroy(batch);
 xrtValueRelease(params);DeleteCriticalSection(&client.lock);return 0;
}
typedef struct gate { HANDLE entered,release;LONG block; } gate;
static void wake(void* user)
{gate* g=user;if(InterlockedExchange(&g->block,0)){SetEvent(g->entered);WaitForSingleObject(g->release,10000);}}
static int waitGate(gate* g) {return WaitForSingleObject(g->entered,10000)==WAIT_OBJECT_0;}
static void blockNext(gate* g) {ResetEvent(g->entered);ResetEvent(g->release);InterlockedExchange(&g->block,1);}
static xide_sync_result* waitResult(xide_sync_planner* p)
{uint64_t end=GetTickCount64()+10000;xide_sync_result* r;do {if((r=xideSyncPlannerTake(p))) return r;Sleep(1);}while(GetTickCount64()<end);return NULL;}
static int plans(void)
{
 gate g={CreateEventW(NULL,TRUE,FALSE,NULL),CreateEventW(NULL,TRUE,FALSE,NULL),0};CHECK(g.entered && g.release);
 xide_sync_planner* p=xideSyncPlannerCreate(wake,&g);xls_sync* base=xlsSyncCreate(2);CHECK(p && base);
 xvalue* text=xrtValueString(xrtStrView("A😀Z\n"));xide_sync_input input={"file:///main.xl",text,1,1};
 CHECK(xideSyncPlannerSubmit(p,0,base,&input,1)==-1 && xideSyncPlannerSubmit(p,1,base,NULL,1)==-1 && xideSyncPlannerSubmit(p,1,base,&input,65)==-1);
 blockNext(&g);CHECK(xideSyncPlannerSubmit(p,1,base,&input,1)==1);xrtValueRelease(text);text=NULL;
 CHECK(waitGate(&g));xide_sync_result* first=xideSyncPlannerTake(p);CHECK(first && first->valid && first->token==1 && first->versions[0]==1 && first->count==1 && !xlsSyncVersion(base,input.uri));
 xls_text* lease=xlsSyncRetainText(first->sync,input.uri,1);CHECK(lease && !strcmp(xlsTextData(lease),"A😀Z\n"));
 xlsSyncDestroy(base);base=first->sync;first->sync=NULL;
 xide_language_client client={0};InitializeCriticalSection(&client.lock);messages m={0};
 CHECK(xideLanguageBatchSend(&client,first->batch));first->batch=NULL;CHECK(drain(&client,&m) && m.count==1 && textIs(get(m.values[0],"method"),"textDocument/didOpen") && textIs(get(get(get(m.values[0],"params"),"textDocument"),"text"),"A😀Z\n"));clearMessages(&m);
 text=xrtValueString(xrtStrView("A😁Z\n"));input.text=text;input.revision=2;
 CHECK(xideSyncPlannerSubmit(p,2,base,&input,1)==1 && xideSyncPlannerSubmit(p,3,base,&input,1)==0);
 CHECK(xideSyncPlannerRetire(p,first));xide_sync_result* extra=xrtCalloc(1,sizeof(*extra));CHECK(extra && !xideSyncPlannerRetire(p,extra));xideSyncResultRelease(extra);
 xideSyncPlannerCancel(p);xrtValueRelease(text);SetEvent(g.release);
 uint64_t until=GetTickCount64()+5000;int idle=0;do {xrtMutexLock(&p->lock);idle=!p->pending;xrtMutexUnlock(&p->lock);if(!idle) Sleep(1);}while(!idle && GetTickCount64()<until);
 CHECK(idle && !xideSyncPlannerTake(p) && xlsSyncVersion(base,input.uri)==1);
 text=xrtValueString(xrtStrView("A😁Z\n"));input.text=text;CHECK(xideSyncPlannerSubmit(p,4,base,&input,1)==1);xrtValueRelease(text);
 xide_sync_result* next=waitResult(p);CHECK(next && next->valid && next->token==4 && next->versions[0]==2 && xlsSyncVersion(base,input.uri)==1);
 char digest[65];size_t count=0;CHECK(xlsSyncInputState(next->sync,digest,&count) && count==1 && !strcmp(digest,next->digest));
 CHECK(xideLanguageBatchSend(&client,next->batch));next->batch=NULL;CHECK(drain(&client,&m) && m.count==1 && textIs(get(m.values[0],"method"),"textDocument/didChange"));
 const xvalue* delta=xrtValueArrayGet(get(get(m.values[0],"params"),"contentChanges"),0);CHECK(delta && textIs(get(delta,"text"),"😁") && integer(get(get(get(delta,"range"),"start"),"character"))==1 && integer(get(get(get(delta,"range"),"end"),"character"))==3);clearMessages(&m);
 xls_sync* old=base;base=next->sync;next->sync=old;CHECK(xideSyncPlannerRetire(p,next));
 text=xrtValueString((xstrview){"\xc0\x80",2});input.text=text;input.revision=3;CHECK(xideSyncPlannerSubmit(p,5,base,&input,1)==1);xrtValueRelease(text);next=waitResult(p);CHECK(next && !next->valid && xlsSyncVersion(base,input.uri)==2);xideSyncResultRelease(next);
 text=xrtValueString(xrtStrView("replacement"));input.text=text;input.identity=9;input.revision=1;
 CHECK(xideSyncPlannerSubmit(p,6,base,&input,1)==1);xrtValueRelease(text);next=waitResult(p);CHECK(next && next->valid && next->versions[0]==1 && next->count==1);
 CHECK(xideLanguageBatchSend(&client,next->batch));next->batch=NULL;CHECK(drain(&client,&m) && m.count==2 && textIs(get(m.values[0],"method"),"textDocument/didClose") && textIs(get(m.values[1],"method"),"textDocument/didOpen"));clearMessages(&m);
 xideSyncResultRelease(next);CHECK(xideSyncPlannerSubmit(p,7,base,NULL,0)==1);next=waitResult(p);CHECK(next && next->valid && !next->count && xlsSyncVersion(base,input.uri)==2);xideSyncResultRelease(next);
 xideSyncPlannerDestroy(p);xlsSyncDestroy(base);CHECK(!strcmp(xlsTextData(lease),"A😀Z\n"));xlsTextRelease(lease);DeleteCriticalSection(&client.lock);CloseHandle(g.entered);CloseHandle(g.release);return 0;
}
int main(void)
{CHECK(!batches());CHECK(!plans());printf("sync_plan_test passed: %d checks; atomic prepared batches, bounded gated worker slots, cancellation, exact delta/receipt/version parity, immutable ownership and retirement.\n",checks);return 0;}
