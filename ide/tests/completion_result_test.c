#include "services/completion_result.h"
#include "protocol/document_sync.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
/* Fault injection applies only to this real module, not the SDK allocator.
   Private compilation is registered explicitly in build.ps1. */
static xatomic32 failAllocation;
static void* testCalloc(size_t count,size_t size)
{int n=xrtAtomic32Load(&failAllocation,XMEMORY_RELAXED);if(n>0 && xrtAtomic32FetchSub(&failAllocation,1,XMEMORY_RELAXED)==1) return NULL;return xrtCalloc(count,size);}
#define xrtCalloc testCalloc
#include "../src/services/completion_result.c"
#undef xrtCalloc
static int checks;
#define CHECK(e) do {++checks;if(!(e)){fprintf(stderr,"completion_result_test:%d %s\n",__LINE__,#e);return 0;}}while(0)
static xvalue* crGet(const xvalue* v,const char* key) {return (xvalue*)field(v,key);}
static int crSet(xvalue* v,const char* key,xvalue* data) {return data && xrtValueObjectSetNew(v,xrtStrView(key),data);}
static xvalue* crParse(const char* text) {return xrtJsonParse(xrtStrView(text));}
static const char* crSource="int 结果Suffix\r\nbox.\n";
static xide_completion_expected crExpected={7,3,7};
static xvalue* crMake(size_t count)
{
 xvalue* message=crParse("{\"jsonrpc\":\"2.0\",\"id\":7,\"result\":{\"version\":3,\"queryId\":7,\"scope\":\"cursor-query\",\"guardScope\":\"background-poll\",\"checkedCompilation\":false,\"isIncomplete\":false,\"available\":true,\"ok\":true,\"prefix\":\"结\",\"replacementRange\":{\"start\":{\"line\":0,\"character\":4},\"end\":{\"line\":0,\"character\":12}},\"items\":[]}}");
 if(!message) return NULL;
 xvalue* body=crGet(message,"result"),*list=crGet(body,"items");
 for(size_t i=0;i<count;++i) {
  char buffer[512];snprintf(buffer,sizeof(buffer),"{\"label\":\"Item%04zu\",\"filterText\":\"Item%04zu\",\"sortText\":\"%08zu\",\"detail\":\"int\",\"kind\":6,\"insertTextFormat\":1,\"data\":{\"symbolId\":%zu},\"textEdit\":{\"newText\":\"Item%04zu\"}}",count-i-1,count-i-1,i,i,count-i-1);
  xvalue* item=crParse(buffer);if(!item || !crSet(crGet(item,"textEdit"),"range",xrtValueDeepClone(crGet(body,"replacementRange"))) || !xrtValueArrayAppend(list,item)) {xrtValueRelease(item);xrtValueRelease(message);return NULL;}xrtValueRelease(item);
 }
 return message;
}
static int crInvalid(xls_text* text,xvalue* message,xide_completion_expected expected)
{
 xide_completion_result* r=xideCompletionDecode(text,message,expected,NULL,NULL);
 CHECK(r && !r->valid && !r->available && !r->count && !r->items && !r->names && !r->message);xideCompletionResultRelease(r);return 1;
}
static int crCancel(void* user) {return --*(int*)user<=0;}
static int crParsing(void)
{
 xls_text* text=xlsTextCreate(crSource,strlen(crSource),4096,NULL);xvalue* message=crMake(2);CHECK(text && message);
 xide_completion_result* r=xideCompletionDecode(text,message,crExpected,NULL,NULL);
 CHECK(r && r->valid && r->available && r->count==2 && r->start==4 && r->end==16 && r->message==message);
 CHECK(!strcmp(r->items[0].label,"Item0001") && !strcmp(r->items[1].label,"Item0000") && !strcmp(r->names[0],"Item0000") && !strcmp(r->items[1].detail,"int"));
 xrtValueRelease(message);CHECK(!strcmp(r->items[0].label,"Item0001"));xideCompletionResultRelease(r);
 for(int mode=0;mode<32;++mode) {
  message=crMake(2);CHECK(message);xvalue* body=crGet(message,"result"),*range=crGet(body,"replacementRange"),*item=xrtValueArrayGet(crGet(body,"items"),1),*edit=crGet(item,"textEdit");
  if(mode==0) crSet(message,"id",xrtValueInt(8));
  if(mode==1) crSet(message,"jsonrpc",xrtValueString(xrtStrView("wrong")));
  if(mode==2) crSet(message,"error",xrtValueNull());
  if(mode==3) crSet(body,"version",xrtValueInt(4));
  if(mode==4) crSet(body,"queryId",xrtValueInt(9));
  if(mode==5) crSet(body,"scope",xrtValueString(xrtStrView("project")));
  if(mode==6) crSet(body,"guardScope",xrtValueString(xrtStrView("unchecked")));
  if(mode==7) crSet(body,"checkedCompilation",xrtValueBool(true));
  if(mode==8) crSet(body,"isIncomplete",xrtValueBool(true));
  if(mode==9) crSet(body,"ok",xrtValueBool(false));
  if(mode==10) crSet(body,"available",xrtValueBool(false));
  if(mode==11) crSet(crGet(range,"end"),"character",xrtValueInt(999999));
  if(mode==12) crSet(crGet(range,"end"),"line",xrtValueInt(99));
  if(mode==13) crSet(crGet(range,"start"),"character",xrtValueInt(5));
  if(mode==14) crSet(body,"prefix",xrtValueString(xrtStrView("错")));
  if(mode==15) crSet(edit,"newText",xrtValueString(xrtStrView("other")));
  if(mode==16) crSet(item,"filterText",xrtValueString(xrtStrView("other")));
  if(mode==17) crSet(item,"insertTextFormat",xrtValueInt(2));
  if(mode==18) crSet(item,"kind",xrtValueInt(25));
  if(mode==19) crSet(crGet(item,"data"),"symbolId",xrtValueInt(-1));
  if(mode==20) crSet(crGet(item,"data"),"symbolId",xrtValueInt(INT64_C(4294967296)));
  if(mode==21) crSet(crGet(crGet(edit,"range"),"end"),"character",xrtValueInt(11));
  if(mode==22) crSet(item,"label",xrtValueString((xstrview){"\xc0\x80",2}));
  if(mode==23) crSet(item,"detail",xrtValueString((xstrview){"a\0b",3}));
  if(mode==24) crSet(item,"sortText",xrtValueString(xrtStrView("012345678901234567890123456789012")));
  if(mode==25) crSet(body,"items",xrtValueObject());
  if(mode==26) crSet(item,"label",xrtValueString(xrtStrView("1bad")));
  if(mode==27) crSet(item,"label",xrtValueString(xrtStrView("bad.name")));
  if(mode==28) crSet(crGet(range,"start"),"character",xrtValueFloat(.5));
  if(mode==29) crSet(body,"available",xrtValueInt(1));
  if(mode==30) crSet(item,"detail",xrtValueInt(1));
  if(mode==31) crSet(crGet(item,"data"),"symbolId",xrtValueFloat(.5));
  CHECK(crInvalid(text,message,crExpected));xrtValueRelease(message);
 }
 message=crMake(1);CHECK(message);
 for(int mode=0;mode<4;++mode) {xide_completion_expected expected=crExpected;if(mode==0)expected.request=1;if(mode==1)expected.version=0;if(mode==2)expected.offset=-1;if(mode==3)expected.offset=999;CHECK(crInvalid(text,message,expected));}
 for(int fail=1;fail<=3;++fail) {xrtAtomic32Store(&failAllocation,fail,XMEMORY_RELAXED);r=xideCompletionDecode(text,message,crExpected,NULL,NULL);xrtAtomic32Store(&failAllocation,0,XMEMORY_RELAXED);CHECK(fail==1 ? r==NULL : r && !r->valid && !r->items && !r->names && !r->message);xideCompletionResultRelease(r);}
 int cancelAfter=1;r=xideCompletionDecode(text,message,crExpected,crCancel,&cancelAfter);CHECK(r && !r->valid && !r->message);xideCompletionResultRelease(r);xrtValueRelease(message);
 message=crMake(0);CHECK(message);xvalue* body=crGet(message,"result");CHECK(crSet(body,"available",xrtValueBool(false)) && crSet(body,"ok",xrtValueBool(false)));
 r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid && !r->available && !r->message && !r->count);xideCompletionResultRelease(r);xrtValueRelease(message);
 message=crMake(1);CHECK(message);body=crGet(message,"result");xvalue* dot=crParse("{\"start\":{\"line\":1,\"character\":4},\"end\":{\"line\":1,\"character\":4}}");CHECK(dot && crSet(body,"replacementRange",dot) && crSet(body,"prefix",xrtValueString(xrtStrView(""))) && crSet(crGet(xrtValueArrayGet(crGet(body,"items"),0),"textEdit"),"range",xrtValueDeepClone(crGet(body,"replacementRange"))));
 r=xideCompletionDecode(text,message,(xide_completion_expected){7,3,22},NULL,NULL);CHECK(r && r->valid && r->start==22 && r->end==22);xideCompletionResultRelease(r);xrtValueRelease(message);
 message=crMake(4096);CHECK(message);r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid && r->count==4096);
 for(size_t i=0;i<4096;++i){char label[40];snprintf(label,sizeof(label),"Item%04zu",i);CHECK(!strcmp(r->names[i],label) && r->items[i].symbol==i);}xideCompletionResultRelease(r);
 cancelAfter=4;r=xideCompletionDecode(text,message,crExpected,crCancel,&cancelAfter);CHECK(r && !r->valid && !r->count && !r->items && !r->message);xideCompletionResultRelease(r);xrtValueRelease(message);
 message=crMake(4097);CHECK(message && crInvalid(text,message,crExpected));xrtValueRelease(message);xlsTextRelease(text);return 1;
}

static int crRange(xvalue* message,int sl,int sc,int el,int ec)
{
 char buffer[220];snprintf(buffer,sizeof(buffer),"{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}",sl,sc,el,ec);
 xvalue* body=crGet(message,"result");if(!crSet(body,"replacementRange",crParse(buffer))) return 0;
 xvalue* items=crGet(body,"items");for(size_t i=0;i<xrtValueCount(items);++i) if(!crSet(crGet(xrtValueArrayGet(items,i),"textEdit"),"range",xrtValueDeepClone(crGet(body,"replacementRange")))) return 0;return 1;
}
static int crLabel(xvalue* item,const char* label)
{return crSet(item,"label",xrtValueString(xrtStrView(label))) && crSet(item,"filterText",xrtValueString(xrtStrView(label))) && crSet(crGet(item,"textEdit"),"newText",xrtValueString(xrtStrView(label)));}
static int crEdges(void)
{
 const char* source="int 结😀Suffix\r\n";xls_text* text=xlsTextCreate(source,strlen(source),4096,NULL);xvalue* message=crMake(1);CHECK(text && message && crRange(message,0,4,0,13) && crSet(crGet(message,"result"),"prefix",xrtValueString(xrtStrView("结😀"))));
 xide_completion_result* r=xideCompletionDecode(text,message,(xide_completion_expected){7,3,11},NULL,NULL);CHECK(r && r->valid && r->start==4 && r->end==17);xideCompletionResultRelease(r);
 CHECK(crRange(message,0,6,0,13) && crInvalid(text,message,(xide_completion_expected){7,3,11}));
 CHECK(crRange(message,0,4,0,6) && crInvalid(text,message,(xide_completion_expected){7,3,11}));
 CHECK(crRange(message,0,4,0,13) && crSet(crGet(message,"result"),"prefix",xrtValueString((xstrview){source+4,4})) && crInvalid(text,message,(xide_completion_expected){7,3,8}));xrtValueRelease(message);xlsTextRelease(text);
 source="\xef\xbb\xbf" "int 结😀Suffix\r\n";text=xlsTextCreate(source,strlen(source),4096,NULL);message=crMake(1);CHECK(text && message && crRange(message,0,5,0,14) && crSet(crGet(message,"result"),"prefix",xrtValueString(xrtStrView("结😀"))));
 r=xideCompletionDecode(text,message,(xide_completion_expected){7,3,14},NULL,NULL);CHECK(r && r->valid && r->start==7 && r->end==20);xideCompletionResultRelease(r);xrtValueRelease(message);xlsTextRelease(text);
 text=xlsTextCreate(crSource,strlen(crSource),4096,NULL);message=crMake(2);CHECK(text && message);xvalue* body=crGet(message,"result"),*items=crGet(body,"items"),*item=xrtValueArrayGet(items,0);
 CHECK(crLabel(item,"结果") && crLabel(xrtValueArrayGet(items,1),"结果"));r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid && r->count==2 && !strcmp(r->names[0],"结果") && !strcmp(r->names[1],"结果") && r->items[0].symbol!=r->items[1].symbol);xideCompletionResultRelease(r);
 CHECK(crSet(crGet(item,"data"),"symbolId",xrtValueInt(INT64_C(4294967295))));r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid && r->items[0].symbol==UINT32_MAX);xideCompletionResultRelease(r);
 int kinds[]={2,3,5,6,7,21};for(size_t i=0;i<6;++i){CHECK(crSet(item,"kind",xrtValueInt(kinds[i])));r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid);xideCompletionResultRelease(r);}
 char label[514];memset(label,'a',513);label[512]=0;CHECK(crLabel(item,label));r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid);xideCompletionResultRelease(r);label[512]='a';label[513]=0;CHECK(crLabel(item,label) && crInvalid(text,message,crExpected));CHECK(crLabel(item,"normal"));
 char* detail=malloc(8194);CHECK(detail);memset(detail,'x',8193);detail[8192]=0;CHECK(crSet(item,"detail",xrtValueString(xrtStrView(detail))));r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid);xideCompletionResultRelease(r);detail[8192]='x';detail[8193]=0;CHECK(crSet(item,"detail",xrtValueString(xrtStrView(detail))) && crInvalid(text,message,crExpected));free(detail);
 CHECK(crSet(item,"detail",xrtValueString(xrtStrView(""))) && crSet(item,"sortText",xrtValueString(xrtStrView(""))));r=xideCompletionDecode(text,message,crExpected,NULL,NULL);CHECK(r && r->valid && !*r->items[0].detail && !*r->items[0].sort);xideCompletionResultRelease(r);xrtValueRelease(message);xlsTextRelease(text);return 1;
}
typedef struct crDrop {xatomic32 count,thread;} crDrop;
static void crDropped(ptr handle,ptr user)
{crDrop* state=user;if(handle!=state)return;xrtAtomic32Store(&state->thread,(int32)GetCurrentThreadId(),XMEMORY_RELAXED);xrtAtomic32FetchAdd(&state->count,1,XMEMORY_RELEASE);}
static xvalue* crOwned(crDrop* state)
{static const xvaluehandleops ops={.Drop=crDropped};ptr owned=state;return xrtValueHandleTake(&owned,&ops,state);}
static int crAttach(xvalue* message,crDrop* state) {return crSet(message,"testLifetime",crOwned(state));}
static int crWaitDrop(crDrop* state)
{uint64_t until=GetTickCount64()+10000;while(!xrtAtomic32Load(&state->count,XMEMORY_ACQUIRE) && GetTickCount64()<until)Sleep(1);return xrtAtomic32Load(&state->count,XMEMORY_ACQUIRE)==1;}
typedef struct crGate {HANDLE entered,release;volatile LONG block,finished;} crGate;
static void crWake(void* user)
{crGate* gate=user;if(InterlockedExchange(&gate->block,0)){SetEvent(gate->entered);WaitForSingleObject(gate->release,10000);}InterlockedIncrement(&gate->finished);}
static xide_completion_result* crWait(xide_completion_decoder* d)
{uint64_t until=GetTickCount64()+10000;xide_completion_result* r=NULL;do{if((r=xideCompletionDecoderTake(d)))return r;Sleep(1);}while(GetTickCount64()<until);return NULL;}
static int crEmit(void* user,const char* method,const xvalue* params) {(void)user;(void)method;(void)params;return 1;}
static int crWorkers(void)
{
 DWORD owner=GetCurrentThreadId();crGate gate={CreateEventW(NULL,TRUE,FALSE,NULL),CreateEventW(NULL,TRUE,FALSE,NULL),1,0};CHECK(gate.entered && gate.release);
 xrtAtomic32Store(&failAllocation,1,XMEMORY_RELAXED);CHECK(!xideCompletionDecoderCreate(crWake,&gate));xrtAtomic32Store(&failAllocation,0,XMEMORY_RELAXED);
 xide_completion_decoder* d=xideCompletionDecoderCreate(crWake,&gate);CHECK(d);
 xls_sync* sync=xlsSyncCreate(2);xls_sync_document document={"file:///completion.xl",crSource,strlen(crSource),1,1};CHECK(sync && xlsSyncUpdate(sync,&document,1,crEmit,NULL));
 xls_text* text=xlsSyncRetainText(sync,document.uri,1);crDrop first={0};xvalue* message=crMake(1);CHECK(text && message && crAttach(message,&first));
 CHECK(!xideCompletionDecoderSubmit(NULL,1,message,text,crExpected) && !xideCompletionDecoderSubmit(d,0,message,text,crExpected));
 xrtAtomic32Store(&failAllocation,1,XMEMORY_RELAXED);CHECK(!xideCompletionDecoderSubmit(d,11,message,text,crExpected));xrtAtomic32Store(&failAllocation,0,XMEMORY_RELAXED);
 CHECK(!xrtAtomic32Load(&first.count,XMEMORY_ACQUIRE) && xrtValueType(message)==XVALUE_OBJECT);
 CHECK(xideCompletionDecoderSubmit(d,11,message,text,crExpected));message=NULL;text=NULL;xlsSyncDestroy(sync);
 CHECK(WaitForSingleObject(gate.entered,10000)==WAIT_OBJECT_0);xide_completion_result* answer=xideCompletionDecoderTake(d);
 CHECK(answer && answer->valid && answer->token==11 && answer->count==1 && !strcmp(answer->items[0].label,"Item0000") && !xrtAtomic32Load(&first.count,XMEMORY_ACQUIRE));
 text=xlsTextCreate(crSource,strlen(crSource),4096,NULL);crDrop second={0};message=crMake(1);CHECK(text && message && crAttach(message,&second));
 CHECK(xideCompletionDecoderSubmit(d,12,message,xlsTextRetain(text),crExpected));message=NULL;
 crDrop third={0};xvalue* rejected=crMake(1);xls_text* lease=xlsTextRetain(text);CHECK(rejected && crAttach(rejected,&third));
 CHECK(!xideCompletionDecoderSubmit(d,13,rejected,lease,crExpected) && !xideCompletionDecoderDiscard(d,rejected));xlsTextRelease(lease);
 CHECK(xrtValueType(rejected)==XVALUE_OBJECT && !xrtAtomic32Load(&third.count,XMEMORY_ACQUIRE));
 CHECK(xideCompletionDecoderRetire(d,answer));xide_completion_result* extra=xrtCalloc(1,sizeof(*extra));CHECK(extra && !xideCompletionDecoderRetire(d,extra));xideCompletionResultRelease(extra);
 xideCompletionDecoderCancel(d);CHECK(!xrtAtomic32Load(&first.count,XMEMORY_ACQUIRE));SetEvent(gate.release);
 CHECK(crWaitDrop(&first) && crWaitDrop(&second) && (DWORD)xrtAtomic32Load(&first.thread,XMEMORY_RELAXED)!=owner && (DWORD)xrtAtomic32Load(&second.thread,XMEMORY_RELAXED)!=owner && !xideCompletionDecoderTake(d));
 CHECK(xideCompletionDecoderSubmit(d,14,rejected,xlsTextRetain(text),crExpected));rejected=NULL;answer=crWait(d);
 CHECK(answer && answer->valid && answer->token==14 && !strcmp(answer->items[0].label,"Item0000"));
 crDrop disposable={0};xvalue* garbage=crOwned(&disposable);CHECK(garbage && xideCompletionDecoderDiscard(d,garbage));
 CHECK(crWaitDrop(&disposable) && (DWORD)xrtAtomic32Load(&disposable.thread,XMEMORY_RELAXED)!=owner && !xrtAtomic32Load(&third.count,XMEMORY_ACQUIRE));
 CHECK(xideCompletionDecoderRetire(d,answer) && crWaitDrop(&third) && (DWORD)xrtAtomic32Load(&third.thread,XMEMORY_RELAXED)!=owner);
 crDrop cancelledResult={0};message=crMake(1);CHECK(message && crAttach(message,&cancelledResult) && xideCompletionDecoderSubmit(d,15,message,xlsTextRetain(text),crExpected));
 /* Cancel a completed but untaken answer; its JSON must be collected by the worker. */
 uint64_t until=GetTickCount64()+10000;int completed=0;do{xrtMutexLock(&d->lock);completed=d->completed!=NULL;xrtMutexUnlock(&d->lock);if(!completed)Sleep(1);}while(!completed && GetTickCount64()<until);
 CHECK(completed);xideCompletionDecoderCancel(d);CHECK(crWaitDrop(&cancelledResult) && !xideCompletionDecoderTake(d) && (DWORD)xrtAtomic32Load(&cancelledResult.thread,XMEMORY_RELAXED)!=owner);
 crDrop shutdown={0};message=crMake(1);CHECK(message && crAttach(message,&shutdown) && xideCompletionDecoderSubmit(d,16,message,xlsTextRetain(text),crExpected));
 xideCompletionDecoderDestroy(d);CHECK(crWaitDrop(&shutdown) && (DWORD)xrtAtomic32Load(&shutdown.thread,XMEMORY_RELAXED)!=owner);
 xlsTextRelease(text);CloseHandle(gate.entered);CloseHandle(gate.release);return 1;
}
int main(void)
{if(!crParsing() || !crEdges() || !crWorkers())return 1;printf("completion_result_test passed: %d checks; complete strict batch/Unicode/range/bounds/allocation validation, 4096 stable candidates, exact source lease, gated bounded worker slots, stale cancellation and worker-thread ownership retirement.\n",checks);return 0;}
