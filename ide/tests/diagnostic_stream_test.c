#include "services/diagnostic_stream.h"
#include "protocol/lsp_wire.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Private gates make ownership, capacity and reset races reproducible with
 * both the standalone XRT and the matched GUI SDK; no shipped test hooks. */
typedef struct test_gate {xmutex lock;xcond changed;int open,entered;} test_gate;
static test_gate startupGate,mapGate,retireGate;
static void gateInit(test_gate* g,int open)
{memset(g,0,sizeof(*g));if(!xrtMutexInit(&g->lock) || !xrtCondInit(&g->changed)) abort();g->open=open;}
static void gateSet(test_gate* g,int open)
{xrtMutexLock(&g->lock);g->open=open;g->entered=0;xrtCondBroadcast(&g->changed);xrtMutexUnlock(&g->lock);}
static void gatePass(test_gate* g)
{xrtMutexLock(&g->lock);g->entered=1;xrtCondBroadcast(&g->changed);while(!g->open) xrtCondWait(&g->changed,&g->lock);xrtMutexUnlock(&g->lock);}
static int gateEntered(test_gate* g)
{xrtMutexLock(&g->lock);if(!g->entered) xrtCondWaitFor(&g->changed,&g->lock,10000);int entered=g->entered;xrtMutexUnlock(&g->lock);return entered;}
static void gateUnit(test_gate* g) {xrtCondUnit(&g->changed);xrtMutexUnit(&g->lock);}
typedef struct test_start {xthreadproc proc;ptr data;} test_start;
static int32 gatedStart(ptr value)
{test_start copy=*(test_start*)value;xrtFree(value);gatePass(&startupGate);return copy.proc(copy.data);}
static xthread* gatedCreate(xthreadproc proc,ptr data,size_t stack)
{test_start* s=xrtMalloc(sizeof(*s));if(!s) return NULL;*s=(test_start){proc,data};xthread* t=xrtThreadCreate(gatedStart,s,stack);if(!t) xrtFree(s);return t;}
static int gatedMap(xide_diagnostic_notice* n,const xls_text* t,int (*stop)(void*),void* user)
{gatePass(&mapGate);return xideDiagnosticNoticeMap(n,t,stop,user);}
static xatomic32 failAt;
static void* faultCalloc(size_t n,size_t size)
{uint32 at=xrtAtomic32Load(&failAt,XMEMORY_RELAXED);if(at && xrtAtomic32FetchSub(&failAt,1,XMEMORY_RELAXED)==1) return NULL;return xrtCalloc(n,size);}
#define xrtThreadCreate gatedCreate
#define xideDiagnosticNoticeMap gatedMap
#define xrtCalloc faultCalloc
#include "../src/services/diagnostic_stream.c"
#undef xrtCalloc
#undef xideDiagnosticNoticeMap
#undef xrtThreadCreate
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"diagnostic_stream_test:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static const char* bodyA="a😀z\nnext\n",*bodyB="a😀x\nnext\n";
static const char* hashA="53862da9c31f6ca0ce1ef2c1e002b2a36b78a0c096a440e968d5c23dbd5554d3";
static const char* hashB="8a296a93f704bb899e7d19401a4c4bfc0b8b606a761aa0ead0191422ee9ba900";
static const char* identity="1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee";
static xvalue* parse(const char* json) {xjsonreadconfig c;xrtJsonReadConfigInit(&c);xvalue* v=xrtJsonRead(xrtStrView(json),&c);CHECK(v);return v;}
static int put(xvalue* v,const char* name,xvalue* owned) {return owned && xrtValueObjectSetNew(v,xrtStrView(name),owned);}
static xvalue* get(const xvalue* v,const char* name) {return (xvalue*)xrtValueObjectGet(v,xrtStrView(name));}
static xvalue* envelope(const char* method,xvalue* params)
{xvalue* v=xrtValueObject();CHECK(v && put(v,"jsonrpc",xrtValueString(xrtStrView("2.0"))) && put(v,"method",xrtValueString(xrtStrView(method))) && put(v,"params",params));return v;}
static xvalue* declaration(void)
{return envelope("xlang/diagnosticDependencies",parse("{\"schema\":1,\"id\":\"1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee\",\"removed\":false,\"part\":0,\"parts\":1,\"paths\":[\"D:/dep.xl\",\"D:/工程😀/main.xl\"]}"));}
static xvalue* publication(int version,const char* hash,const char* message)
{
 char json[1800];snprintf(json,sizeof(json),"{\"uri\":\"file:///D:/dep.xl\",\"version\":%d,\"xlang\":{\"schema\":1,\"generation\":\"12\",\"cleared\":false,\"dependencies\":\"%s\",\"document\":{\"path\":\"D:/dep.xl\",\"origin\":\"overlay\",\"version\":%d,\"normalizedSize\":12,\"normalizedSha256\":\"%s\"}},\"diagnostics\":[{\"file\":\"D:/dep.xl\",\"range\":{\"start\":{\"line\":0,\"character\":1},\"end\":{\"line\":0,\"character\":3}},\"severity\":1,\"message\":\"%s\"}]}",version,identity,version,hash,message);
 return envelope("textDocument/publishDiagnostics",parse(json));
}
static xvalue* withdrawal(void)
{return envelope("textDocument/publishDiagnostics",parse("{\"uri\":\"file:///D:/dep.xl\",\"xlang\":{\"schema\":1,\"generation\":\"13\",\"cleared\":true},\"diagnostics\":[]}"));}
static xvalue* removal(void)
{return envelope("xlang/diagnosticDependencies",parse("{\"schema\":1,\"id\":\"1f11180b87f977e5024139f595d1819766f9433987d1c1b985ed4c591f114aee\",\"removed\":true}"));}
static int appendOwned(xvalue* v,size_t bytes,void* user) {return xideDiagnosticBatchAppend(user,v,bytes);}
static void append(xide_diagnostic_batch* b,xvalue* owned)
{
 size_t size=0;xide_model_error error={0};char* frame=xideLspFrame(owned,&size,&error);CHECK(frame);xrtValueRelease(owned);
 xide_lsp_stream parser={0};for(size_t at=0;at<size;) {size_t n=size-at;if(n>17) n=17;CHECK(xideLspStreamFeedOwned(&parser,frame+at,n,appendOwned,b,&error));at+=n;}
 CHECK(xideLspStreamEnd(&parser,&error));xideLspStreamClear(&parser);free(frame);
}
static xide_diagnostic_batch* makeBatch(int declare,int version,const char* hash)
{xide_diagnostic_batch* b=xideDiagnosticBatchCreate();CHECK(b);if(declare) append(b,declaration());if(version) append(b,publication(version,hash,version==1 ? "one" : "two"));return b;}
static int discardEmit(void* user,const char* method,const xvalue* params) {(void)user;(void)method;(void)params;return 1;}
static void update(xls_sync* s,const char* body,uint64_t revision)
{xls_sync_document d={"file:///D:/dep.xl",body,strlen(body),1,revision};CHECK(xlsSyncUpdate(s,&d,1,discardEmit,NULL));}
static xls_sync* makeSync(const char* body) {xls_sync* s=xlsSyncCreate(2);CHECK(s);update(s,body,1);return s;}
static xide_diagnostic_result* waitResult(xide_diagnostic_stream* s)
{for(unsigned i=0;i<10000;++i) {xide_diagnostic_result* r=xideDiagnosticStreamTake(s);if(r) return r;xrtSleep(1);}CHECK(0);return NULL;}
typedef struct lifetime {xatomic32 dropped;uint64_t thread;} lifetime;
static void dropLife(ptr value,ptr user)
{(void)user;lifetime* l=value;l->thread=xrtThreadCurrentId();xrtAtomic32Store(&l->dropped,1,XMEMORY_RELEASE);}
static void watch(xvalue* v,lifetime* l)
{memset(l,0,sizeof(*l));xrtAtomic32Init(&l->dropped,0);ptr owned=l;static const xvaluehandleops ops={NULL,dropLife,NULL,NULL};CHECK(put(v,"testLifetime",xrtValueHandleTake(&owned,&ops,NULL)) && !owned);}
static void waitDrop(lifetime* l)
{for(unsigned i=0;i<10000 && !xrtAtomic32Load(&l->dropped,XMEMORY_ACQUIRE);++i) xrtSleep(1);CHECK(xrtAtomic32Load(&l->dropped,XMEMORY_ACQUIRE));}
static void releaseResult(void* value) {xideDiagnosticResultRelease(value);}
static void releaseBatch(void* value) {xideDiagnosticBatchRelease(value);}
static void fifoAndSnapshots(void)
{
 gateSet(&startupGate,0);gateSet(&mapGate,1);xide_diagnostic_stream* s=xideDiagnosticStreamCreate(NULL,NULL);CHECK(s && xideDiagnosticStreamReset(s,1));
 xls_sync* sync=makeSync(bodyA);char digestA[65],digestB[65];size_t documents=0;CHECK(xlsSyncInputState(sync,digestA,&documents) && documents==1);
 xide_diagnostic_batch* b1=makeBatch(1,1,hashA);lifetime rootLife,paramLife;watch(b1->messages[1],&rootLife);watch(get(b1->messages[1],"params"),&paramLife);
 uint64_t one=0,two=0,three=999;CHECK(xideDiagnosticStreamSubmit(s,1,11,b1,sync,&one)==1 && one==1);
 xls_text* original=xlsSyncRetainText(sync,"file:///D:/dep.xl",1);xls_text* cloned=xlsSyncRetainText(s->jobs[s->jobHead]->sync,"file:///D:/dep.xl",1);CHECK(original && cloned==original);xlsTextRelease(original);xlsTextRelease(cloned);
 update(sync,bodyB,2);CHECK(xlsSyncInputState(sync,digestB,&documents) && strcmp(digestA,digestB));xide_diagnostic_batch* b2=makeBatch(0,2,hashB);CHECK(xideDiagnosticStreamSubmit(s,1,12,b2,sync,&two)==1 && two==2);
 xide_diagnostic_batch* b3=xideDiagnosticBatchCreate();CHECK(b3);append(b3,withdrawal());append(b3,removal());CHECK(xideDiagnosticStreamSubmit(s,1,13,b3,NULL,&three)==0 && !three && b3->count==2);
 xide_diagnostic_stream_stats stats;CHECK(xideDiagnosticStreamLoad(s,&stats) && stats.queued==2 && !stats.running && stats.outstanding==2 && stats.submitted==2 && stats.rejected==1);
 xlsSyncDestroy(sync);gateSet(&startupGate,1);xide_diagnostic_result* r1=waitResult(s);CHECK(r1->valid && r1->serial==1 && r1->sourceEpoch==11 && r1->hasSync && !strcmp(r1->syncDigest,digestA) && r1->count==1 && !r1->unmappedOverlays && r1->workerThreadId!=xrtThreadCurrentId());
 waitDrop(&rootLife);CHECK(rootLife.thread==r1->workerThreadId && !xrtAtomic32Load(&paramLife.dropped,XMEMORY_ACQUIRE));
 CHECK(xideDiagnosticStreamSubmit(s,1,13,b3,NULL,&three)==1 && three==3);xide_diagnostic_notice* detached=xideDiagnosticResultTakeNotice(r1,0);CHECK(detached && detached->mapped && detached->items[0].start==1 && detached->items[0].end==5 && !strcmp(detached->items[0].message,"one") && !xideDiagnosticResultTakeNotice(r1,0));xideDiagnosticResultRelease(r1);
 xide_diagnostic_result* r2=waitResult(s);CHECK(r2->valid && r2->serial==2 && r2->sourceEpoch==12 && !strcmp(r2->syncDigest,digestB) && r2->notices[0]->mapped && !strcmp(r2->notices[0]->items[0].message,"two"));xideDiagnosticResultRelease(r2);
 xide_diagnostic_result* r3=waitResult(s);CHECK(r3->valid && r3->serial==3 && !r3->hasSync && r3->count==1 && r3->notices[0]->cleared && !r3->registry.dictionaries && !r3->registry.bindings);xideDiagnosticResultRelease(r3);
 CHECK(xideDiagnosticStreamLoad(s,&stats) && !stats.outstanding && stats.finished==3);xideDiagnosticStreamDestroy(s);
 CHECK(xideDiagnosticDependencyCount(detached->dependency)==2 && !strcmp(detached->items[0].message,"one"));xideDiagnosticNoticeRelease(detached);waitDrop(&paramLife);CHECK(paramLife.thread==xrtThreadCurrentId());
}
typedef struct reset_request {xide_diagnostic_stream* stream;xatomic32 done;int result;} reset_request;
static int32 doReset(ptr value) {reset_request* r=value;r->result=xideDiagnosticStreamReset(r->stream,2);xrtAtomic32Store(&r->done,1,XMEMORY_RELEASE);return 0;}
static void resetDuringMapping(void)
{
 gateSet(&startupGate,1);gateSet(&mapGate,0);xide_diagnostic_stream* s=xideDiagnosticStreamCreate(NULL,NULL);CHECK(s && xideDiagnosticStreamReset(s,1));xls_sync* sync=makeSync(bodyA);
 xide_diagnostic_batch* b=makeBatch(1,1,hashA);lifetime old;watch(b->messages[1],&old);CHECK(xideDiagnosticStreamSubmit(s,1,21,b,sync,NULL)==1 && gateEntered(&mapGate));
 /* Temporarily transfer the single owner to this helper. No owner calls run
  * concurrently; a joined Reset would fail while the worker gate stays shut. */
 reset_request request={0};request.stream=s;xrtAtomic32Init(&request.done,0);xthread* owner=xrtThreadCreate(doReset,&request,0);CHECK(owner);
 for(unsigned i=0;i<500 && !xrtAtomic32Load(&request.done,XMEMORY_ACQUIRE);++i) xrtSleep(1);
 CHECK(xrtAtomic32Load(&request.done,XMEMORY_ACQUIRE) && request.result && !xrtAtomic32Load(&old.dropped,XMEMORY_ACQUIRE));xrtThreadWait(owner);xrtThreadDestroy(owner);
 b=makeBatch(1,1,hashA);CHECK(xideDiagnosticStreamSubmit(s,2,22,b,sync,NULL)==1);gateSet(&mapGate,1);xide_diagnostic_result* r=waitResult(s);CHECK(r->session==2 && r->sourceEpoch==22 && r->valid && r->notices[0]->mapped);waitDrop(&old);CHECK(old.thread==r->workerThreadId);xideDiagnosticResultRelease(r);
 xide_diagnostic_stream_stats stats;CHECK(xideDiagnosticStreamLoad(s,&stats) && !stats.outstanding && stats.cancelled==1 && stats.finished==2 && !xideDiagnosticStreamTake(s));xlsSyncDestroy(sync);xideDiagnosticStreamDestroy(s);
}
static void poisonAndUnmatchedSources(void)
{
 xide_diagnostic_stream* s=xideDiagnosticStreamCreate(NULL,NULL);CHECK(s && xideDiagnosticStreamReset(s,1));xide_diagnostic_batch* b=makeBatch(1,0,NULL);CHECK(xideDiagnosticStreamSubmit(s,1,1,b,NULL,NULL)==1);xide_diagnostic_result* r=waitResult(s);CHECK(r->valid && !r->count);xideDiagnosticResultRelease(r);
 b=makeBatch(0,1,hashA);append(b,removal());CHECK(xideDiagnosticStreamSubmit(s,1,1,b,NULL,NULL)==1);r=waitResult(s);CHECK(!r->valid && r->count==1 && *r->error && !xideDiagnosticResultTakeNotice(r,0));xideDiagnosticResultRelease(r);
 b=makeBatch(0,1,hashA);CHECK(xideDiagnosticStreamSubmit(s,1,1,b,NULL,NULL)==1);r=waitResult(s);CHECK(!r->valid && !r->count && strstr(r->error,"new connection"));xideDiagnosticResultRelease(r);
 CHECK(xideDiagnosticStreamReset(s,2));b=makeBatch(1,1,hashA);CHECK(xideDiagnosticStreamSubmit(s,2,2,b,NULL,NULL)==1);r=waitResult(s);CHECK(r->valid && !r->hasSync && r->unmappedOverlays==1 && !r->notices[0]->mapped);xideDiagnosticResultRelease(r);
 xls_sync* wrong=makeSync(bodyB);b=makeBatch(0,1,hashA);CHECK(xideDiagnosticStreamSubmit(s,2,3,b,wrong,NULL)==1);r=waitResult(s);CHECK(r->valid && r->hasSync && r->unmappedOverlays==1 && !r->notices[0]->mapped && r->notices[0]->items[0].start==-1);xideDiagnosticResultRelease(r);xlsSyncDestroy(wrong);
 /* JSON-RPC requests must never be mistaken for notifications. */
 b=makeBatch(0,1,hashA);CHECK(put(b->messages[0],"id",xrtValueNull()) && xideDiagnosticStreamSubmit(s,2,4,b,NULL,NULL)==1);r=waitResult(s);CHECK(!r->valid && strstr(r->error,"notification"));xideDiagnosticResultRelease(r);xideDiagnosticStreamDestroy(s);
}
typedef struct retired_value {lifetime life;int block;} retired_value;
static void disposeRetired(void* value) {retired_value* v=value;if(v->block) gatePass(&retireGate);dropLife(&v->life,NULL);}
static void retirementAndBounds(void)
{
 gateSet(&retireGate,0);xide_diagnostic_stream* s=xideDiagnosticStreamCreate(NULL,NULL);CHECK(s && xideDiagnosticStreamReset(s,1));retired_value values[3]={0};values[0].block=1;for(size_t i=0;i<3;++i) xrtAtomic32Init(&values[i].life.dropped,0);
 CHECK(xideDiagnosticStreamRetire(s,&values[0],disposeRetired) && gateEntered(&retireGate));CHECK(xrtMutexTryLock(&s->lock));xrtMutexUnlock(&s->lock);
 CHECK(xideDiagnosticStreamRetire(s,&values[1],disposeRetired) && !xideDiagnosticStreamRetire(s,&values[2],disposeRetired) && xideDiagnosticStreamReset(s,2));
 xide_diagnostic_batch* b=makeBatch(1,0,NULL);CHECK(xideDiagnosticStreamSubmit(s,2,1,b,NULL,NULL)==1);b=makeBatch(0,1,hashA);CHECK(xideDiagnosticStreamSubmit(s,2,2,b,NULL,NULL)==1);gateSet(&retireGate,1);
 xide_diagnostic_result* a=waitResult(s),*r=waitResult(s);CHECK(a->valid && r->valid && a->serial<r->serial);uint64_t worker=r->workerThreadId;xideDiagnosticResultRelease(a);CHECK(xideDiagnosticStreamRetire(s,r,releaseResult));
 waitDrop(&values[0].life);waitDrop(&values[1].life);CHECK(values[0].life.thread==worker && values[1].life.thread==worker);
 /* Retirement may still contain r until the worker takes it. Retry retains
  * ownership and does not allocate or silently destroy the rejected value. */
 int accepted=0;for(unsigned i=0;i<10000 && !accepted;++i) {accepted=xideDiagnosticStreamRetire(s,&values[2],disposeRetired);if(!accepted) xrtSleep(1);}CHECK(accepted);waitDrop(&values[2].life);CHECK(values[2].life.thread==worker);xideDiagnosticStreamDestroy(s);
 b=xideDiagnosticBatchCreate();xvalue* v=xrtValueObject();CHECK(b && v && !xideDiagnosticBatchAppend(b,v,0) && !xideDiagnosticBatchAppend(b,v,(size_t)XIDE_DIAGNOSTIC_BATCH_BYTES+1));
 for(size_t i=0;i<4;++i) CHECK(xideDiagnosticBatchAppend(b,xrtValueObject(),8u*1024u*1024u));
 CHECK(xideDiagnosticBatchBytes(b)==XIDE_DIAGNOSTIC_BATCH_BYTES && !xideDiagnosticBatchAppend(b,v,1));xrtValueRelease(v);xideDiagnosticBatchRelease(b);
 b=xideDiagnosticBatchCreate();for(size_t i=0;i<XIDE_DIAGNOSTIC_BATCH_MESSAGES;++i) CHECK(xideDiagnosticBatchAppend(b,xrtValueObject(),1));v=xrtValueObject();CHECK(xideDiagnosticBatchCount(b)==128 && !xideDiagnosticBatchAppend(b,v,1));xrtValueRelease(v);xideDiagnosticBatchRelease(b);
}
static void allocationAndShutdown(void)
{
 gateSet(&startupGate,0);xide_diagnostic_stream* s=xideDiagnosticStreamCreate(NULL,NULL);CHECK(s && !xideDiagnosticStreamReset(s,0) && xideDiagnosticStreamReset(s,2) && xideDiagnosticStreamReset(s,2) && !xideDiagnosticStreamReset(s,1));
 xide_diagnostic_batch* b=xideDiagnosticBatchCreate();uint64_t serial=999;CHECK(b && xideDiagnosticStreamSubmit(s,2,1,b,NULL,&serial)==-1 && !serial);append(b,declaration());
 CHECK(xideDiagnosticStreamSubmit(s,1,1,b,NULL,&serial)==-1 && !serial && b->count==1);
 xls_sync* sync=makeSync(bodyA);for(unsigned i=1;i<=2;++i) {xrtAtomic32Store(&failAt,i,XMEMORY_RELAXED);CHECK(xideDiagnosticStreamSubmit(s,2,1,b,sync,&serial)==-1 && !serial && b->count==1 && xlsSyncVersion(sync,"file:///D:/dep.xl")==1);}xrtAtomic32Store(&failAt,0,XMEMORY_RELAXED);
 lifetime life;watch(b->messages[0],&life);CHECK(xideDiagnosticStreamSubmit(s,2,1,b,sync,&serial)==1 && serial==1);xlsSyncDestroy(sync);gateSet(&startupGate,1);xideDiagnosticStreamDestroy(s);waitDrop(&life);CHECK(life.thread!=xrtThreadCurrentId());
 gateSet(&startupGate,1);s=xideDiagnosticStreamCreate(NULL,NULL);CHECK(s && xideDiagnosticStreamReset(s,1));b=makeBatch(1,1,hashA);CHECK(xideDiagnosticStreamSubmit(s,1,1,b,NULL,NULL)==1);xide_diagnostic_result* r=waitResult(s);CHECK(r->valid);xideDiagnosticStreamDestroy(s);CHECK(r->count==1 && !strcmp(r->notices[0]->items[0].message,"one"));xideDiagnosticResultRelease(r);
 xideDiagnosticBatchRelease(NULL);xideDiagnosticResultRelease(NULL);xideDiagnosticStreamDestroy(NULL);CHECK(!xideDiagnosticStreamRetire(NULL,&life,releaseBatch) && !xideDiagnosticStreamTake(NULL) && !xideDiagnosticStreamLoad(NULL,NULL));
}
typedef struct replay_source {char* uri;xls_text* text;uint64_t identity,revision;} replay_source;
typedef struct replay_state {xide_diagnostic_stream* stream;xls_sync* sync;uint64_t epoch,serial;size_t batches,notices,mapped,unmatched,pending;} replay_state;
static const char* valueText(const xvalue* value)
{xstrview s={0};return xrtValueGetString(value,&s) && s.Data && !memchr(s.Data,0,s.Size) ? s.Data : NULL;}
static void replayResult(replay_state* state)
{
 xide_diagnostic_result* r=waitResult(state->stream);if(!r->valid) fprintf(stderr,"replay: %s\n",r->error);
 CHECK(r->valid && r->session==1 && r->serial==state->serial+1 && r->hasSync && r->workerThreadId!=xrtThreadCurrentId());state->serial=r->serial;state->pending=r->registry.pending;++state->batches;
 for(size_t i=0;i<r->count;++i) {xide_diagnostic_notice* n=r->notices[i];++state->notices;if(n->overlay && !n->cleared) {if(n->mapped) ++state->mapped;else ++state->unmatched;}}
 while(!xideDiagnosticStreamRetire(state->stream,r,releaseResult)) xrtSleep(1);
}
static void replayFlush(replay_state* state,xide_diagnostic_batch** batch)
{
 if(!*batch || !xideDiagnosticBatchCount(*batch)) return;
 int accepted=0;while(!(accepted=xideDiagnosticStreamSubmit(state->stream,1,state->epoch,*batch,state->sync,NULL))) replayResult(state);
 CHECK(accepted==1);*batch=NULL;
}
static void replaySent(replay_state* state,replay_source* sources,const xvalue* message)
{
 const char* method=valueText(get(message,"method"));const xvalue* params=get(message,"params"),*document=get(params,"textDocument");
 if(!method || (strcmp(method,"textDocument/didOpen") && strcmp(method,"textDocument/didChange") && strcmp(method,"textDocument/didClose"))) return;
 const char* uri=valueText(get(document,"uri"));CHECK(uri);size_t slot=0;while(slot<64 && (!sources[slot].uri || strcmp(sources[slot].uri,uri))) ++slot;
 if(!strcmp(method,"textDocument/didClose")) {if(slot<64) {xrtFree(sources[slot].uri);xlsTextRelease(sources[slot].text);sources[slot]=(replay_source){0};}}
 else {
  if(!strcmp(method,"textDocument/didOpen")) {
   CHECK(slot==64);for(slot=0;slot<64 && sources[slot].uri;++slot) {}CHECK(slot<64);xstrview source={0};CHECK(xrtValueGetString(get(document,"text"),&source));sources[slot].uri=xrtStrDup(uri);sources[slot].identity=++state->epoch;sources[slot].revision=1;
   sources[slot].text=xlsTextCreate(source.Data ? source.Data : "",source.Size,8u*1024u*1024u,NULL);CHECK(sources[slot].uri && sources[slot].text);
  } else {
   CHECK(slot<64);const xvalue* changes=get(params,"contentChanges");CHECK(changes && xrtValueType(changes)==XVALUE_ARRAY);
   for(size_t i=0;i<xrtValueCount(changes);++i) {
    const xvalue* change=xrtValueArrayGet(changes,i),*range=get(change,"range");xstrview source={0};CHECK(xrtValueGetString(get(change,"text"),&source));xls_text_edit edit={0};edit.text=source.Data ? source.Data : "";edit.length=source.Size;
    if(range) {
     int64 line=0,column=0,endLine=0,endColumn=0;CHECK(xrtValueGetInt(get(get(range,"start"),"line"),&line) && xrtValueGetInt(get(get(range,"start"),"character"),&column) && xrtValueGetInt(get(get(range,"end"),"line"),&endLine) && xrtValueGetInt(get(get(range,"end"),"character"),&endColumn) && line>=0 && column>=0 && endLine>=0 && endColumn>=0);
     edit.hasRange=1;edit.start=(xls_position){(size_t)line,(size_t)column};edit.end=(xls_position){(size_t)endLine,(size_t)endColumn};
    }
    xls_text* updated=xlsTextApply(sources[slot].text,&edit,1,8u*1024u*1024u,NULL);CHECK(updated);xlsTextRelease(sources[slot].text);sources[slot].text=updated;
   }
   ++sources[slot].revision;
  }
 }
 xls_sync_document documents[64];size_t count=0;for(size_t i=0;i<64;++i) if(sources[i].uri) documents[count++]=(xls_sync_document){sources[i].uri,xlsTextData(sources[i].text),xlsTextSize(sources[i].text),sources[i].identity,sources[i].revision};
 CHECK(xlsSyncUpdate(state->sync,documents,count,discardEmit,NULL));++state->epoch;
 if(slot<64 && sources[slot].uri) {int64 version=0;CHECK(xrtValueGetInt(get(document,"version"),&version) && xlsSyncVersion(state->sync,uri)==version);}
}
static void replayTranscript(const char* path)
{
 size_t bytes=0;unsigned char* raw=xrtFileReadAllLimit(path,64u*1024u*1024u,&bytes);CHECK(raw);xjsonreadconfig config;xrtJsonReadConfigInit(&config);config.MaxInputBytes=64u*1024u*1024u;config.MaxStringBytes=8u*1024u*1024u;config.MaxValues=2097152;config.MaxContainerItems=262144;config.MaxDepth=32;
 xvalue* events=xrtJsonRead((xstrview){(const char*)raw,bytes},&config);xrtFree(raw);CHECK(events && xrtValueType(events)==XVALUE_ARRAY);replay_state state={0};state.stream=xideDiagnosticStreamCreate(NULL,NULL);state.sync=xlsSyncCreate(2);CHECK(state.stream && state.sync && xideDiagnosticStreamReset(state.stream,1));replay_source sources[64]={0};xide_diagnostic_batch* batch=NULL;
 for(size_t i=0;i<xrtValueCount(events);++i) {
  const xvalue* event=xrtValueArrayGet(events,i),*sent=get(event,"send");if(sent) {replayFlush(&state,&batch);replaySent(&state,sources,sent);}
  const xvalue* message=get(event,"receive");const char* method=valueText(get(message,"method"));if(!method) continue;
  if(strcmp(method,"xlang/diagnosticDependencies") && strcmp(method,"textDocument/publishDiagnostics")) continue;
  if(!strcmp(method,"textDocument/publishDiagnostics")) {const xvalue* proof=get(get(message,"params"),"xlang");bool cleared=false;if(!xrtValueGetBool(get(proof,"cleared"),&cleared) || (!cleared && !get(proof,"dependencies"))) continue;}
  /* Serialization is test-only: the parsed production transport transfers the
   * original owned root/raw Content-Length through NextSized without this. */
  size_t length=0;char* encoded=xrtJsonStringify(message,false,&length);CHECK(encoded && length);xrtFree(encoded);
  if(batch && (batch->count==XIDE_DIAGNOSTIC_BATCH_MESSAGES || length>XIDE_DIAGNOSTIC_BATCH_BYTES-batch->bytes)) replayFlush(&state,&batch);
  if(!batch) batch=xideDiagnosticBatchCreate();
  CHECK(batch);append(batch,xrtValueDeepClone(message));
 }
 replayFlush(&state,&batch);xideDiagnosticBatchRelease(batch);xide_diagnostic_stream_stats stats;while(xideDiagnosticStreamLoad(state.stream,&stats) && stats.outstanding) replayResult(&state);CHECK(!state.pending);xideDiagnosticStreamDestroy(state.stream);xlsSyncDestroy(state.sync);xrtValueRelease(events);
 for(size_t i=0;i<64;++i) {xrtFree(sources[i].uri);xlsTextRelease(sources[i].text);}
 printf("real diagnostic stream accepted: batches=%zu notices=%zu mapped-exact-snapshot=%zu unmatched-overlay=%zu; bounded background FIFO and retirement.\n",state.batches,state.notices,state.mapped,state.unmatched);
}
static void dependencyOverlayVersions(void)
{
 gateSet(&startupGate,1);gateSet(&mapGate,1);xide_diagnostic_stream* stream=xideDiagnosticStreamCreate(NULL,NULL);xls_sync* sync=xlsSyncCreateMonotonic(2);char* second=xideLspPathToUri("D:/工程😀/main.xl",NULL);CHECK(stream && sync && second && xideDiagnosticStreamReset(stream,1));
 xls_sync_document docs[]={{"file:///D:/dep.xl",bodyA,strlen(bodyA),11,1},{second,bodyA,strlen(bodyA),22,1}};CHECK(xlsSyncUpdate(sync,docs,2,discardEmit,NULL) && xlsSyncVersion(sync,second)==2);
 xvalue* saved=NULL;
 for(size_t i=0;i<4;++i) {
  xide_diagnostic_batch* batch=makeBatch(i==0,0,NULL);xvalue* root=publication(1,hashA,"one"),*proof=get(get(root,"params"),"xlang"),*overlays=xrtValueArray();CHECK(root && overlays && put(proof,"serial",xrtValueString(xrtStrView("2"))));
  for(size_t j=0;j<3;++j) {
   if(i==1 && j==1) continue;
   xvalue* binding=xrtValueObject();CHECK(binding && put(binding,"uri",xrtValueString(xrtStrView(j==0 ? docs[0].uri : j==1 ? second : "file:///D:/unrelated.xl"))) && put(binding,"version",xrtValueInt(j==0 ? 1 : j==1 ? i==2 ? 9 : 2 : 77)) && xrtValueArrayAppend(overlays,binding));xrtValueRelease(binding);
  }
  CHECK(put(proof,"overlays",overlays));append(batch,root);CHECK(xideDiagnosticStreamSubmitCausal(stream,1,101+i,2,batch,sync,NULL)==1);xide_diagnostic_result* result=waitResult(stream);CHECK(result->valid && result->count==1 && !result->unmappedOverlays && result->notices[0]->mapped && result->notices[0]->hasOverlays && result->notices[0]->hasSerial && result->notices[0]->inputSerial==2 && result->notices[0]->wireBytes>0);
  xide_diagnostic_notice* notice=result->notices[0];CHECK(notice->overlaysMatched==(i==0 || i==3) && notice->bindingCount==(i==1 ? 1u : 2u));
  if(notice->overlaysMatched) CHECK(notice->bindings[0].identity==11 && notice->bindings[1].identity==22 && notice->bindings[1].revision==1);
  if(i==3) {saved=xrtValueDeepClone(notice->params);CHECK(saved);}
  xideDiagnosticResultRelease(result);
 }
 xide_diagnostic_batch* batch=makeBatch(0,0,NULL);append(batch,envelope("textDocument/publishDiagnostics",xrtValueDeepClone(saved)));append(batch,publication(1,hashA,"missing negotiated proof"));
 CHECK(xideDiagnosticStreamSubmitCausal(stream,1,200,2,batch,sync,NULL)==1);xide_diagnostic_result* result=waitResult(stream);CHECK(!result->valid && result->count==2 && !xideDiagnosticResultTakeNotice(result,0) && *result->error);xideDiagnosticResultRelease(result);
 CHECK(xideDiagnosticStreamReset(stream,2));batch=makeBatch(1,0,NULL);xvalue* future=xrtValueDeepClone(saved);CHECK(future && put(get(future,"xlang"),"serial",xrtValueString(xrtStrView("3"))));append(batch,envelope("textDocument/publishDiagnostics",future));
 CHECK(xideDiagnosticStreamSubmitCausal(stream,2,201,2,batch,sync,NULL)==1);result=waitResult(stream);CHECK(!result->valid && !xideDiagnosticResultTakeNotice(result,0) && strstr(result->error,"future"));xideDiagnosticResultRelease(result);
 CHECK(xideDiagnosticStreamReset(stream,3));batch=makeBatch(1,0,NULL);
 append(batch,envelope("textDocument/publishDiagnostics",xrtValueDeepClone(saved)));
 xvalue* invalid=xrtValueDeepClone(saved);CHECK(invalid && put(get(get(xrtValueArrayGet(get(invalid,"diagnostics"),0),"range"),"end"),"character",xrtValueInt(99999)));
 append(batch,envelope("textDocument/publishDiagnostics",invalid));
 CHECK(xideDiagnosticStreamSubmitCausal(stream,3,202,2,batch,sync,NULL)==1);result=waitResult(stream);
 CHECK(!result->valid && result->count==2 && result->notices[0]->mapped && result->notices[1]->sourceVerified && !result->notices[1]->mapped && !xideDiagnosticResultTakeNotice(result,0) && *result->error);
 xideDiagnosticResultRelease(result);xrtValueRelease(saved);
 xlsSyncDestroy(sync);xideDiagnosticStreamDestroy(stream);free(second);
}
int main(int argc,char** argv)
{
 gateInit(&startupGate,1);gateInit(&mapGate,1);gateInit(&retireGate,1);xrtAtomic32Init(&failAt,0);
 if(argc==3 && !strcmp(argv[1],"--transcript")) {replayTranscript(argv[2]);gateUnit(&startupGate);gateUnit(&mapGate);gateUnit(&retireGate);return 0;}
 fifoAndSnapshots();resetDuringMapping();poisonAndUnmatchedSources();retirementAndBounds();allocationAndShutdown();dependencyOverlayVersions();gateUnit(&startupGate);gateUnit(&mapGate);gateUnit(&retireGate);
 printf("diagnostic_stream_test passed: %u checks; bounded FIFO, immutable exact-source snapshots, off-owner decoding/mapping/retirement, reset during blocked mapping, no partial invalid batch delivery, poisoned-session recovery, mismatched source handling, allocation rollback and detached lifetime.\n",checks);return 0;
}
