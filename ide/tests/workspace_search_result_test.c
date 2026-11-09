#include "services/workspace_symbols.h"
#include "services/workspace_search_result.h"
#include "services/language_check.h"
#include "platform/path.h"
#include <xrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
static size_t operations,faultAt;static int tracking;static LONG64 owners;
static HANDLE entered,resume;static xatomic32 gated;static xvalue* trackedMessage;static uint64_t releasedMessage;static xide_language_batch* trackedBatch;static uint64_t releasedBatch;
static int operation(void){return !tracking || ++operations!=faultAt;}
static void* fixtureCalloc(size_t n,size_t size){return operation()?xrtCalloc(n,size):NULL;}
static void* fixtureMalloc(size_t n){void* p=operation()?malloc(n):NULL;if(p)InterlockedIncrement64(&owners);return p;}
static char* fixtureUri(const char* uri,xide_model_error* e){if(xrtAtomic32Load(&gated,XMEMORY_ACQUIRE)==1){xrtAtomic32Store(&gated,2,XMEMORY_RELEASE);SetEvent(entered);WaitForSingleObject(resume,5000);}char* p=operation()?xideLspUriToPath(uri,e):NULL;if(p)InterlockedIncrement64(&owners);return p;}
static wchar_t* fixtureKey(const char* path){wchar_t* p=operation()?xidePathNativeWide(path):NULL;if(p)InterlockedIncrement64(&owners);return p;}
static bool fixtureHash(const void* data,size_t n,void* hash){return operation() && xrtSha256(data,n,hash);}
static void fixtureFree(void* p){if(p)InterlockedDecrement64(&owners);free(p);}
static void fixtureRelease(xvalue* p){if(p && p==trackedMessage)releasedMessage=xrtThreadCurrentId();xrtValueRelease(p);}
static void fixtureBatchFree(xide_language_batch* p){if(p && p==trackedBatch)releasedBatch=xrtThreadCurrentId();xideLanguageBatchDestroy(p);}
#define xrtCalloc fixtureCalloc
#define malloc fixtureMalloc
#define free fixtureFree
#define xideLspUriToPath fixtureUri
#define xidePathNativeWide fixtureKey
#define xrtSha256 fixtureHash
#define xrtValueRelease fixtureRelease
#define xideLanguageBatchDestroy fixtureBatchFree
#include "../src/services/workspace_search_result.c"
#undef xrtCalloc
#undef malloc
#undef free
#undef xideLspUriToPath
#undef xidePathNativeWide
#undef xrtSha256
#undef xrtValueRelease
#undef xideLanguageBatchDestroy
static unsigned checks;
#define CHECK(v) do{++checks;if(!(v)){fprintf(stderr,"workspace_search_result_test:%d:%s\n",__LINE__,#v);return 1;}}while(0)
static int put(xvalue* o,const char* k,xvalue* v){return v && xrtValueObjectSetNew(o,xrtStrView(k),v);}
static xvalue* response(xvalue* result,int id){xvalue* message=xrtValueObject();if(message && put(message,"jsonrpc",xrtValueString(xrtStrView("2.0"))) && put(message,"id",xrtValueInt(id)) && put(message,"result",result))return message;xrtValueRelease(message);return NULL;}
static xvalue* finish(xide_workspace_symbol_query* q)
{enum xide_workspace_symbol_state state=XIDE_WORKSPACE_SYMBOL_PENDING;for(unsigned i=0;q && i<30000 && state==XIDE_WORKSPACE_SYMBOL_PENDING;++i){state=xideWorkspaceSymbolsResult(q,NULL,NULL,NULL,0);if(state==XIDE_WORKSPACE_SYMBOL_PENDING)xrtSleep(1);}xvalue* out=state==XIDE_WORKSPACE_SYMBOL_READY?xideWorkspaceSymbolsTakeLsp(q):NULL;xideWorkspaceSymbolsQueryDestroy(q);return out;}
static xide_workspace_search_result* waitResult(xide_workspace_search_decoder* d)
{xide_workspace_search_result* r=NULL;for(unsigned i=0;i<5000 && !r;++i){r=xideWorkspaceSearchDecoderTake(d);if(!r)xrtSleep(1);}return r;}
static xide_language_batch* waitBatch(xide_workspace_search_decoder* d,uint64_t token)
{xide_language_batch* b=NULL;for(unsigned i=0;i<5000 && !b;++i){b=xideWorkspaceSearchDecoderTakeBatch(d,token);if(!b)xrtSleep(1);}return b;}
static int stopAt(void* p){size_t* v=p;return ++v[0]>=v[1];}
int main(void)
{
 char *directory=xrtPathAbs("build/workspace-search-result/中文😀"),*path=xrtPathJoin(directory,"main.xl"),*compiler=xrtPathJoin(directory,"absent-xl.exe");CHECK(directory && path && compiler && xrtDirCreateAll(directory));
 const char* body="\xef\xbb\xbf/* 中文😀 */\r\nclass Payload\r\n int number\r\nend\r\nfunc main(...) as int\r\n return 0\r\nend\r\n";CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}));
 xls_context_spec spec={.kind=XLS_CONTEXT_XLANG,.entry=path,.directory=directory,.analysisProfile="xlang-ls/compiler-xrt-v1",.targetKind="exe"};xls_context* context=xlsContextCreate(&spec,1024*1024,NULL);xls_context* scopes[]={context};xide_toolchains* cache=xideToolchainsCreate();xide_workspace_symbols* store=xideWorkspaceSymbolsCreate(cache,compiler,NULL);CHECK(context && cache && store);
 xvalue* rows=finish(xideWorkspaceSymbolsSubmitLsp(store,scopes,1,NULL,0,"Payload number",200,0));CHECK(rows && xrtValueCount(rows)==1);xvalue* selection=xrtValueRetain(xrtValueArrayGet(rows,0));xvalue* list=response(rows,7);CHECK(list);xide_workspace_search_result* decoded=xideWorkspaceSearchDecode(xrtValueRetain(list),NULL,NULL,0,NULL,NULL);CHECK(decoded && decoded->valid && decoded->count==1 && strstr(decoded->items[0].label,"number") && xidePathLexicalEqual(decoded->items[0].path,path));xideWorkspaceSearchResultRelease(decoded);CHECK(!owners);
 xvalue* proof=finish(xideWorkspaceSymbolsSubmitNavigation(store,scopes,1,NULL,0,"Payload number",200,0,selection));CHECK(proof);xvalue* navigation=response(proof,8);CHECK(navigation);decoded=xideWorkspaceSearchDecode(xrtValueRetain(navigation),selection,NULL,0,NULL,NULL);CHECK(decoded && decoded->valid && decoded->navigation && decoded->count==1 && decoded->snapshot.bom && decoded->snapshot.eol==XIDE_TEXT_EOL_CRLF && decoded->snapshot.rawLength==strlen(body) && !memcmp(decoded->snapshot.raw,body,strlen(body)) && decoded->items[0].end.offset>decoded->items[0].start.offset);xideWorkspaceSearchResultRelease(decoded);CHECK(!owners);
 for(unsigned test=0;test<10;++test){xvalue* message=xrtValueDeepClone(navigation);xvalue* result=xrtValueObjectGet(message,xrtStrView("result"));xvalue* document=xrtValueObjectGet(result,xrtStrView("document"));
  if(test==0)CHECK(put(document,"normalizedSha256",xrtValueString(xrtStrView("0000000000000000000000000000000000000000000000000000000000000000"))));
  if(test==1)CHECK(put(document,"normalizedSize",xrtValueInt(8*1024*1024+1)));
  if(test==2)CHECK(put(document,"origin",xrtValueString(xrtStrView("overlay"))));
  if(test==3)CHECK(put(result,"symbol",xrtValueNull()));
  if(test==4)CHECK(put(result,"extra",xrtValueArray()));
  if(test==5)CHECK(put(message,"jsonrpc",xrtValueString(xrtStrView("1.0"))));
  if(test==6)CHECK(put(message,"method",xrtValueNull()));
  if(test==7)CHECK(put(message,"id",xrtValueInt(1)));
  if(test==8)CHECK(xrtValueObjectRemove(message,xrtStrView("result")));
  if(test==9)CHECK(put(document,"version",xrtValueInt(1)));
  decoded=xideWorkspaceSearchDecode(message,selection,NULL,0,NULL,NULL);CHECK(decoded && !decoded->valid);xideWorkspaceSearchResultRelease(decoded);CHECK(!owners);
 }
 xvalue* repeated=xrtValueArray();CHECK(repeated);for(unsigned i=0;i<200;++i)CHECK(xrtValueArrayAppend(repeated,selection));xvalue* many=response(repeated,9);decoded=xideWorkspaceSearchDecode(xrtValueRetain(many),NULL,NULL,0,NULL,NULL);CHECK(decoded && decoded->valid && decoded->count==200);xideWorkspaceSearchResultRelease(decoded);CHECK(xrtValueArrayAppend(repeated,selection));decoded=xideWorkspaceSearchDecode(many,NULL,NULL,0,NULL,NULL);CHECK(decoded && !decoded->valid);xideWorkspaceSearchResultRelease(decoded);CHECK(!owners);
 const char* overlay="/* 中文😀 */\nclass Payload\n int number\nend\nfunc main(...) as int\n return 9\nend\n";xls_text* text=xlsTextCreate(overlay,strlen(overlay),1024*1024,NULL);xls_analysis_document doc={"entry",path,text,3,0};CHECK(text);proof=finish(xideWorkspaceSymbolsSubmitNavigation(store,scopes,1,&doc,1,"Payload number",200,0,selection));CHECK(proof);xvalue* overlayMessage=response(proof,10),*lease=xrtValueString(xrtStrView(overlay));CHECK(overlayMessage && lease);decoded=xideWorkspaceSearchDecode(xrtValueRetain(overlayMessage),selection,lease,INT_MIN,NULL,NULL);CHECK(decoded && decoded->valid && !decoded->snapshot.raw);xideWorkspaceSearchResultRelease(decoded);decoded=xideWorkspaceSearchDecode(xrtValueRetain(overlayMessage),selection,lease,0,NULL,NULL);CHECK(decoded && !decoded->valid);xideWorkspaceSearchResultRelease(decoded);
 tracking=1;operations=faultAt=0;decoded=xideWorkspaceSearchDecode(xrtValueRetain(navigation),selection,NULL,0,NULL,NULL);tracking=0;CHECK(decoded && decoded->valid);size_t refusals=operations;xideWorkspaceSearchResultRelease(decoded);
 for(size_t at=1;at<=refusals;++at){tracking=1;operations=0;faultAt=at;decoded=xideWorkspaceSearchDecode(xrtValueRetain(navigation),selection,NULL,0,NULL,NULL);tracking=0;CHECK(!decoded || !decoded->valid);xideWorkspaceSearchResultRelease(decoded);CHECK(!owners);}
 size_t cancel[2]={0,SIZE_MAX};decoded=xideWorkspaceSearchDecode(xrtValueRetain(navigation),selection,NULL,0,stopAt,cancel);CHECK(decoded && decoded->valid);xideWorkspaceSearchResultRelease(decoded);size_t gates=cancel[0];for(size_t at=1;at<=gates;++at){cancel[0]=0;cancel[1]=at;decoded=xideWorkspaceSearchDecode(xrtValueRetain(navigation),selection,NULL,0,stopAt,cancel);CHECK(decoded && !decoded->valid);xideWorkspaceSearchResultRelease(decoded);CHECK(!owners);}
 CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)overlay,strlen(overlay)}));decoded=xideWorkspaceSearchDecode(xrtValueRetain(navigation),selection,NULL,0,NULL,NULL);CHECK(decoded && !decoded->valid);xideWorkspaceSearchResultRelease(decoded);CHECK(xrtFileWriteAtomic(path,(xbytesview){(const unsigned char*)body,strlen(body)}));
 xrtValueRelease(navigation);proof=finish(xideWorkspaceSymbolsSubmitNavigation(store,scopes,1,NULL,0,"Payload number",200,0,selection));CHECK(proof);navigation=response(proof,8);CHECK(navigation);
 uint64_t owner=xrtThreadCurrentId();entered=CreateEventW(NULL,FALSE,FALSE,NULL);resume=CreateEventW(NULL,FALSE,FALSE,NULL);CHECK(entered && resume);xide_workspace_search_decoder* d=xideWorkspaceSearchDecoderCreate(NULL,NULL);CHECK(d && xideWorkspaceSearchDecoderArm(d,1,7,"number",NULL,NULL,0));trackedBatch=waitBatch(d,1);CHECK(trackedBatch && xideWorkspaceSearchDecoderRetireBatch(d,trackedBatch));trackedMessage=xrtValueDeepClone(list);CHECK(trackedMessage);xrtAtomic32Store(&gated,1,XMEMORY_RELEASE);CHECK(xideWorkspaceSearchDecoderReceive(trackedMessage,1000,d));CHECK(WaitForSingleObject(entered,5000)==WAIT_OBJECT_0);xideWorkspaceSearchDecoderCancel(d);CHECK(xideWorkspaceSearchDecoderArm(d,2,8,"number",selection,NULL,0) && !xideWorkspaceSearchDecoderArm(d,3,9,"number",NULL,NULL,0));SetEvent(resume);xide_language_batch* next=waitBatch(d,2);CHECK(next && xideWorkspaceSearchDecoderRetireBatch(d,next));CHECK(xideWorkspaceSearchDecoderReceive(xrtValueRetain(navigation),1000,d));decoded=waitResult(d);CHECK(decoded && decoded->valid && decoded->token==2 && decoded->workerThread!=owner && decoded->snapshot.raw);CHECK(xideWorkspaceSearchDecoderRetire(d,decoded));
 CHECK(xideWorkspaceSearchDecoderArm(d,4,11,"number",NULL,NULL,0));next=waitBatch(d,4);CHECK(next && xideWorkspaceSearchDecoderRetireBatch(d,next));xvalue* failure=xrtValueDeepClone(list);CHECK(failure && put(failure,"id",xrtValueInt(11)));tracking=1;operations=0;faultAt=1;CHECK(xideWorkspaceSearchDecoderReceive(failure,1000,d));decoded=waitResult(d);tracking=0;CHECK(decoded && !decoded->valid && decoded->token==4 && *decoded->error);CHECK(xideWorkspaceSearchDecoderRetire(d,decoded));xideWorkspaceSearchDecoderDestroy(d);CHECK(releasedMessage && releasedMessage!=owner && releasedBatch && releasedBatch!=owner && !owners);CloseHandle(entered);CloseHandle(resume);
 xrtValueRelease(list);xrtValueRelease(navigation);xrtValueRelease(overlayMessage);xrtValueRelease(lease);xrtValueRelease(selection);xlsTextRelease(text);xideWorkspaceSymbolsDestroy(store);xlsContextRelease(context);xideToolchainsDestroy(cache);xrtFree(directory);xrtFree(path);xrtFree(compiler);printf("workspace_search_result_test passed: %u checks;%zu decoder refusal points;%zu cancellation gates;actual native disk/overlay proof,BOM/CRLF/Unicode,200-row bound,queued request preparation,cancellation,emergency failure publication and worker-only disposal.\n",checks,refusals,gates);return 0;
}
