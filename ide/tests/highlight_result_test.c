#include "services/highlight_result.h"
#include "protocol/lsp_wire.h"
#include "protocol/document_sync.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

static unsigned checks;
#define CHECK(v) do {++checks;if(!(v)){fprintf(stderr,"highlight_result_test:%d: %s\n",__LINE__,#v);return 0;}}while(0)
static xvalue* parse(const char* s) { return xrtJsonParse(xrtStrView(s)); }
static int invalid(const xls_text* text,const char* json)
{
    xvalue* value=parse(json);CHECK(value);
    xide_highlight_result* result=xideHighlightDecode(text,value,NULL,NULL);
    CHECK(result && !result->valid && !result->count && !result->ranges);
    xideHighlightResultRelease(result);xrtValueRelease(value);return 1;
}
static int cancel(void* user) { int* count=user;return --*count<=0; }
static void wake(void* user) { xrtAtomic32FetchAdd(user,1,XMEMORY_RELAXED); }
typedef struct wake_gate { xatomic32 entered,hold; } wake_gate;
static void gatedWake(void* user)
{
    wake_gate* gate=user;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    for(int i=0;i<3000 && xrtAtomic32Load(&gate->hold,XMEMORY_ACQUIRE);++i) xrtSleep(1);
}
static int emit(void* user,const char* method,const xvalue* params)
{ (void)method;(void)params;return *(int*)user; }
static xide_highlight_result* waitResult(xide_highlight_decoder* decoder)
{ for(int i=0;i<3000;++i) { xide_highlight_result* r=xideHighlightDecoderTake(decoder);if(r) return r;xrtSleep(1); }return NULL; }
static int submit(xide_highlight_decoder* decoder,uint64_t token,const char* response,xls_text* text)
{
    xvalue* message=parse(response);xls_text* owned=xlsTextRetain(text);
    if(!message || !owned) { xrtValueRelease(message);xlsTextRelease(owned);return 0; }
    for(int i=0;i<3000;++i) { if(xideHighlightDecoderSubmit(decoder,token,message,owned)) return 1;xrtSleep(1); }
    xrtValueRelease(message);xlsTextRelease(owned);return 0;
}
static int verify(void)
{
    const char* source="\xef\xbb\xbf" "A😀中\r\nend\n";
    xls_text* text=xlsTextCreate(source,strlen(source),4096,NULL);CHECK(text);
    const char* item="{\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":4}}}";
    xvalue* value=parse("[{\"range\":{\"start\":{\"line\":1,\"character\":0},\"end\":{\"line\":1,\"character\":3}},\"kind\":3},{\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":4}}},{\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":4}},\"kind\":1}]");CHECK(value);
    xide_highlight_result* result=xideHighlightDecode(text,value,NULL,NULL);
    CHECK(result && result->valid && result->count==2 && result->ranges[0].start==4 && result->ranges[0].end==8 && result->ranges[0].kind==1);
    CHECK(result->ranges[1].start==13 && result->ranges[1].end==16 && result->ranges[1].kind==3);
    xideHighlightResultRelease(result);xrtValueRelease(value);
    const char* bad[]={"{}","1","[null]","[{\"range\":{}}]",
      "[{\"range\":{\"start\":{\"line\":0,\"character\":3},\"end\":{\"line\":0,\"character\":4}}}]",
      "[{\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":3}}}]",
      "[{\"range\":{\"start\":{\"line\":0,\"character\":0},\"end\":{\"line\":0,\"character\":6}}}]",
      "[{\"range\":{\"start\":{\"line\":-1,\"character\":0},\"end\":{\"line\":0,\"character\":1}}}]",
      "[{\"range\":{\"start\":{\"line\":0,\"character\":4},\"end\":{\"line\":0,\"character\":2}}}]",
      "[{\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":2}}}]",
      "[{\"range\":{\"start\":{\"line\":2147483648,\"character\":0},\"end\":{\"line\":0,\"character\":1}}}]",
      "[{\"range\":{\"start\":{\"line\":0.5,\"character\":0},\"end\":{\"line\":0,\"character\":1}}}]"};
    for(size_t i=0;i<sizeof(bad)/sizeof(*bad);++i) CHECK(invalid(text,bad[i]));
    for(int kind=0;kind<7;++kind) {
        char json[320];const char* kinds[]={"0","4","null","true","\"1\"","1.5","-1"};
        snprintf(json,sizeof(json),"[{\"range\":{\"start\":{\"line\":0,\"character\":2},\"end\":{\"line\":0,\"character\":4}},\"kind\":%s}]",kinds[kind]);CHECK(invalid(text,json));
    }
    for(int empty=0;empty<2;++empty) {
        value=parse(empty ? "null" : "[]");result=xideHighlightDecode(text,value,NULL,NULL);
        CHECK(result && result->valid && !result->count && !result->ranges);xideHighlightResultRelease(result);xrtValueRelease(value);
    }
    value=xrtValueArray();CHECK(value);
    for(size_t i=0;i<65536;++i) { xvalue* range=parse(item);CHECK(range && xrtValueArrayAppendNew(value,range)); }
    result=xideHighlightDecode(text,value,NULL,NULL);CHECK(result && result->valid && result->count==1);xideHighlightResultRelease(result);
    int remaining=10;result=xideHighlightDecode(text,value,cancel,&remaining);CHECK(result && !result->valid && !result->count && !result->ranges);xideHighlightResultRelease(result);
    CHECK(xrtValueArrayAppendNew(value,parse(item)));result=xideHighlightDecode(text,value,NULL,NULL);CHECK(result && !result->valid && !result->count);xideHighlightResultRelease(result);xrtValueRelease(value);
    xide_lsp_position positions[2]={{0,2,701},{0,4,702}};remaining=4;
    CHECK(!xideLspManyPositionsToOffsets(source,positions,2,cancel,&remaining) && positions[0].offset==701 && positions[1].offset==702);
    CHECK(!xideLspManyPositionsToOffsets(source,positions,131073,NULL,NULL) && positions[0].offset==701);
    CHECK(!xideLspPositionsToOffsets(source,positions,8193) && positions[1].offset==702);
    xstrbuf corpus;xrtStrBufInit(&corpus);value=xrtValueArray();CHECK(value);
    for(int i=5999;i>=0;--i) {
        char json[220];snprintf(json,sizeof(json),"{\"range\":{\"start\":{\"line\":%d,\"character\":3},\"end\":{\"line\":%d,\"character\":4}},\"kind\":%d}",i,i,1+i%3);
        CHECK(xrtStrBufAppend(&corpus,xrtStrView("中😀A\n")) && xrtValueArrayAppendNew(value,parse(json)));
    }
    xls_text* many=xlsTextCreate(corpus.Data,corpus.Size,8u*1024u*1024u,NULL);CHECK(many);
    result=xideHighlightDecode(many,value,NULL,NULL);CHECK(result && result->valid && result->count==6000);
    for(int i=0;i<6000;++i) CHECK(result->ranges[i].start==i*9+7 && result->ranges[i].end==i*9+8 && result->ranges[i].kind==1+i%3);
    xideHighlightResultRelease(result);xlsTextRelease(many);xrtValueRelease(value);xrtStrBufFree(&corpus);
    /* Transfer an exact synced body, then destroy the synchronizer before the
     * worker returns. The worker owns the JSON envelope and an immutable lease. */
    int allow=1;xls_sync* sync=xlsSyncCreate(2);xls_sync_document doc={"file:///lease.xl",source,strlen(source),44,1};
    CHECK(sync && xlsSyncUpdate(sync,&doc,1,emit,&allow));
    xls_text* lease=xlsSyncRetainText(sync,doc.uri,1);CHECK(lease);
    xatomic32 wakes;xrtAtomic32Init(&wakes,0);
    xide_highlight_decoder* decoder=xideHighlightDecoderCreate(wake,&wakes);CHECK(decoder);
    char response[512];snprintf(response,sizeof(response),"{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":[%s]}",item);
    xvalue* message=parse(response);CHECK(message && xideHighlightDecoderSubmit(decoder,1,message,lease));xlsSyncDestroy(sync);
    result=waitResult(decoder);CHECK(result && result->token==1 && result->valid && result->count==1 && result->ranges[0].start==4);xideHighlightResultRelease(result);
    for(uint64_t token=2;token<82;++token) {
        CHECK(submit(decoder,token,response,text));
        xideHighlightDecoderCancel(decoder);CHECK(!xideHighlightDecoderTake(decoder));
    }
    CHECK(submit(decoder,82,response,text));result=waitResult(decoder);CHECK(result && result->token==82 && result->valid);xideHighlightResultRelease(result);
    CHECK(submit(decoder,83,"{\"jsonrpc\":\"2.0\",\"id\":83,\"result\":[] ,\"error\":{\"code\":-1}}",text));result=waitResult(decoder);CHECK(result && result->token==83 && !result->valid);xideHighlightResultRelease(result);
    CHECK(submit(decoder,84,"{\"jsonrpc\":\"2.0\",\"id\":84,\"result\":null}",text));result=waitResult(decoder);CHECK(result && result->token==84 && result->valid && !result->count);xideHighlightResultRelease(result);
    CHECK(xrtAtomic32Load(&wakes,XMEMORY_RELAXED)>0 && !xideHighlightDecoderTake(decoder));
    CHECK(!xideHighlightDecoderSubmit(NULL,1,NULL,NULL) && !xideHighlightDecoderSubmit(decoder,0,NULL,NULL));
    CHECK(submit(decoder,85,response,text));xideHighlightDecoderDestroy(decoder);xlsTextRelease(text);
    /* Gate the worker's notification to deterministically fill its only
     * pending slot. Rejected submissions retain both caller-owned inputs. */
    text=xlsTextCreate(source,strlen(source),4096,NULL);CHECK(text);
    wake_gate gate;xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.hold,1);
    decoder=xideHighlightDecoderCreate(gatedWake,&gate);CHECK(decoder && submit(decoder,1,response,text));
    int entered=0;for(int i=0;i<3000;++i) { if(xrtAtomic32Load(&gate.entered,XMEMORY_ACQUIRE)) { entered=1;break; }xrtSleep(1); }CHECK(entered);
    CHECK(submit(decoder,2,response,text));message=parse(response);lease=xlsTextRetain(text);
    CHECK(message && !xideHighlightDecoderSubmit(decoder,3,message,lease) && xrtValueType(message)==XVALUE_OBJECT && !strcmp(xlsTextData(lease),source));
    xrtValueRelease(message);xlsTextRelease(lease);xideHighlightDecoderCancel(decoder);CHECK(!xideHighlightDecoderTake(decoder));
    xrtAtomic32Store(&gate.hold,0,XMEMORY_RELEASE);
    CHECK(submit(decoder,4,response,text));result=waitResult(decoder);CHECK(result && result->valid && result->token==4);xideHighlightResultRelease(result);
    xideHighlightDecoderDestroy(decoder);xlsTextRelease(text);
    return 1;
}
int main(void)
{
    if(!verify()) return 1;
    printf("highlight_result_test passed: %u checks; strict complete batch ranges, Unicode/BOM/CRLF, capacity, cancellation atomicity, exact sync leases and bounded XRT worker ownership.\n",checks);return 0;
}
