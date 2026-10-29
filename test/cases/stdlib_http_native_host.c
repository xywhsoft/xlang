/* Actual product runtime, not replacement providers. Ordinary offline vectors
 * plus logical allocation failures at every successful-construction prefix. */
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_text_value.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const xstrview block=XRT_STR_LITERAL("Host: example.test\r\nAccept-Encoding: gzip\r\nContent-Encoding: gzip\r\n");
static const xstrview request=XRT_STR_LITERAL("POST / HTTP/1.1\r\nHost: example.test\r\nContent-Length: 3\r\n\r\na\0bNEXT");
static const xstrview getMethod=XRT_STR_LITERAL("GET");
typedef void (*Destroy)(void*);
static Destroy drops[]={xl6HttpHeadersDestroyV2,xl6HttpParametersDestroyV2,xl6HttpAuthorityDestroyV2,
    xl6HttpTargetDestroyV2,xl6HttpTokensDestroyV2,xl6HttpWeightedTokensDestroyV2,xl6HttpListDestroyV2,
    xl6HttpExpectationsDestroyV2,xl6HttpUpgradesDestroyV2,xl6HttpTeDestroyV2,xl6HttpConnectionDestroyV2,
    xl6HttpAcceptEncodingDestroyV2,xl6HttpContentEncodingsDestroyV2,xl6Http1HeadDestroyV2,
    xl6HttpBodyPlanDestroyV2,xl6Http1BodyReaderDestroyV2,xl6Http1MessageDestroyV2,xl6HttpContentDecoderDestroyV2,
    xl6HttpTransferCodingsDestroyV2};
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount&&after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount&&after.DoubleFreeCount==before->DoubleFreeCount&&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void* construct(size_t kind,void* headers,void* head)
{
    switch(kind){
    case 0:return xl6HttpHeadersParseTextV2(block,-1);
    case 1:return xl6HttpParametersParseV2(XRT_STR_LITERAL("name=\"a\\\"b\"; empty=\"\"; bare"),-1);
    case 2:return xl6HttpAuthorityParseV2(XRT_STR_LITERAL("[::1]:443"),-1);
    case 3:return xl6HttpTargetParseV2(getMethod,XRT_STR_LITERAL("https://example.test/path?q=1"),-1);
    case 4:return xl6HttpTokensParseV2(XRT_STR_LITERAL("gzip, deflate"),-1);
    case 5:return xl6HttpWeightedTokensParseV2(XRT_STR_LITERAL("gzip;q=0.5, deflate"),-1);
    case 6:return xl6HttpListParseV2(XRT_STR_LITERAL("token, \"a,b\""),-1);
    case 7:return xl6HttpExpectationsParseV2(XRT_STR_LITERAL("100-continue, custom=\"a\\\"b\"; x=one"),-1);
    case 8:return xl6HttpUpgradesParseV2(XRT_STR_LITERAL("websocket, example/1"),-1);
    case 9:return xl6HttpTeParseV2(XRT_STR_LITERAL("trailers, gzip;q=0.5"),-1);
    case 10:return xl6HttpConnectionFromHeadersV2(headers,11,0);
    case 11:return xl6HttpAcceptEncodingFromHeadersV2(headers);
    case 12:return xl6HttpContentEncodingsFromHeadersV2(headers);
    case 13:return xl6Http1HeadRequestTextV2(request,65536,8192,8192,100,-1);
    case 14:return xl6HttpBodyPlanCreateV2(1,3);
    case 15:return xl6Http1BodyReaderCreateHeadV2(head,getMethod,16777216,8192,16384,8192,32);
    case 16:return xl6Http1MessageRequestSpanV2((xbytesview){(const uint8*)request.Data,request.Size},false,65536,8192,8192,100,16777216,8192,16384,8192,32,-1);
    case 17:return xl6HttpContentDecoderCreateV2(headers,16777216,65536,4,false);
    case 18:return xl6HttpTransferCodingsParseV2(XRT_STR_LITERAL("gzip; level=one, chunked"),-1);
    default:assert(false);return NULL;
    }
}
static void construction(void)
{
    xmemdebugsnapshot start;xrtMemDebugSnapshot(&start);
    void* headers=xl6HttpHeadersParseTextV2(block,-1);
    void* head=xl6Http1HeadRequestTextV2(request,65536,8192,8192,100,-1);
    assert(headers&&head&&!xrtGetError());size_t prefixes=0;
    for(size_t kind=0;kind<sizeof(drops)/sizeof(drops[0]);++kind){
        bool complete=false;
        for(size_t point=0;point<128;++point){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));void* object=construct(kind,headers,head);
            bool failed=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
            xerror* error=xrtTakeError();
            if(failed)assert(!object&&error);
            else{assert(object&&!error);complete=true;}
            if(object)drops[kind](object);
            xrtErrorFree(error);balance(&before);++prefixes;
            if(complete)break;
        }
        assert(complete);
    }
    xl6Http1HeadDestroyV2(head);xl6HttpHeadersDestroyV2(headers);balance(&start);
    printf("HTTP product factories: %zu complete allocation prefixes, 19 resource kinds balanced\n",prefixes);
}
static void mutation(void)
{
    for(size_t point=0;point<10;++point){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        void* fields=xl6HttpHeadersParseTextV2(XRT_STR_LITERAL("X: old\r\nX: second\r\n"),-1);assert(fields);
        assert(xrtMemDebugFailAfter(point));
        xl6HttpHeadersSetTextV2(fields,XRT_STR_LITERAL("X"),XRT_STR_LITERAL("replacement"),-1);
        bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xerror* error=xrtTakeError();
        assert((error!=NULL)==hit);
        assert(xl6HttpHeadersCountV2(fields,XRT_STR_LITERAL("X"))==(hit?2:1));
        xstrview text=xl6HttpHeadersTextAtV2(fields,0);
        assert(text.Size==(hit?3:11)&&!memcmp(text.Data,hit?"old":"replacement",text.Size));
        xrtFree((void*)text.Data);xrtErrorFree(error);xl6HttpHeadersDestroyV2(fields);balance(&before);
        if(!hit)break;
    }
    puts("HTTP header set: allocation failure leaves old repeated fields intact");
}
typedef struct OwnedHead {xhttp1head head;xhttpfield fields[];} OwnedHead;
static xstrview relocated(xstrview view,const char* data)
{return view.Data?(xstrview){data+(view.Data-request.Data),view.Size}:(xstrview){NULL,0};}
/* A C application which needs the same independent lifetime must also own the
 * head bytes and relocate its views. The borrowed baseline is reported apart. */
static OwnedHead* handwritten(const xhttp1limits* limits)
{
    xhttpfield fields[100];xhttp1head parsed;xrtHttp1HeadInit(&parsed,fields,100);
    assert(xrtHttp1RequestParse((xbytesview){(const uint8*)request.Data,request.Size},&parsed,limits,NULL)==XHTTP1_READY);
    OwnedHead* owned=xrtMalloc(sizeof(*owned)+parsed.FieldCount*sizeof(xhttpfield)+parsed.Bytes);assert(owned);
    owned->head=parsed;owned->head.Fields=owned->fields;owned->head.FieldCapacity=parsed.FieldCount;
    char* data=(char*)(owned->fields+parsed.FieldCount);memcpy(data,request.Data,parsed.Bytes);
    owned->head.Method=relocated(parsed.Method,data);owned->head.Target=relocated(parsed.Target,data);
    owned->head.Reason=relocated(parsed.Reason,data);
    for(size_t i=0;i<parsed.FieldCount;++i)owned->fields[i]=(xhttpfield){relocated(fields[i].Name,data),relocated(fields[i].Value,data)};
    return owned;
}
static void performance(void)
{
    enum{iterations=20000};volatile uint64 sum=0;
    xhttp1limits limits;xrtHttp1LimitsInit(&limits);
    uint64 begin=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;++i){xhttpfield fields[100];xhttp1head head;
        xrtHttp1HeadInit(&head,fields,100);
        assert(xrtHttp1RequestParse((xbytesview){(const uint8*)request.Data,request.Size},&head,&limits,NULL)==XHTTP1_READY);
        sum+=head.Bytes;}
    uint64 rawTicks=xl6MonotonicMillisecondsV1()-begin;
    begin=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;++i){OwnedHead* owned=handwritten(&limits);sum+=owned->head.Bytes;xrtFree(owned);}
    uint64 handwrittenTicks=xl6MonotonicMillisecondsV1()-begin;
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);begin=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;++i){void* head=xl6Http1HeadRequestTextV2(request,65536,8192,8192,100,-1);assert(head);
        sum+=(uint64)xl6Http1HeadHeaderBytesV2(head);xl6Http1HeadDestroyV2(head);}
    uint64 ownedTicks=xl6MonotonicMillisecondsV1()-begin;xrtMemDebugSnapshot(&after);balance(&before);
    printf("HTTP head cost (same runtime, %d iterations, memory-debug=%d): borrowed-C=%llu us; owning-C=%llu us; owned-adapter=%llu us; adapter allocs=%llu; copied bytes=%llu; no second parse\n",
        iterations,(int)xrtMemDebugEnabled(),(unsigned long long)rawTicks,(unsigned long long)handwrittenTicks,(unsigned long long)ownedTicks,(unsigned long long)(after.AllocCount-before.AllocCount),
        (unsigned long long)(sum/3));
    if(xrtMemDebugEnabled())assert(after.AllocCount-before.AllocCount==iterations);
}
static void bodyPerformance(void)
{
    enum{iterations=20000};static const uint8 data[65536]={0};xbuffer out={0};
    assert(xrtBufferReserve(&out,sizeof(data)));xhttp1bodyplan plan={XHTTP1_BODY_FIXED,sizeof(data)};
    xhttp1bodylimits limits;xrtHttp1BodyLimitsInit(&limits);volatile uint64 total=0;
    uint64 start=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;++i){xhttp1body body;size_t consumed=0;xbytesview part={0};out.Size=0;
        assert(xrtHttp1BodyInit(&body,&plan,NULL,0,&limits));
        assert(xrtHttp1BodyRead(&body,(xbytesview){data,sizeof(data)},false,&consumed,&part,NULL)==XHTTP1_BODY_DATA);
        assert(xrtBufferAppend(&out,part));total+=consumed;}
    uint64 raw=xl6MonotonicMillisecondsV1()-start;
    void* ownedPlan=xl6HttpBodyPlanCreateV2(XHTTP1_BODY_FIXED,sizeof(data));assert(ownedPlan);
    void* reader=xl6Http1BodyReaderCreateV2(ownedPlan,16777216,8192,16384,8192,32);assert(reader);
    start=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;++i){out.Size=0;xl6Http1BodyReaderResetV2(reader,ownedPlan,16777216,8192,16384,8192,32);
        assert(xl6Http1BodyReaderReadSpanV2(reader,(xbytesview){data,sizeof(data)},&out,false,-1)==XHTTP1_BODY_DATA);
        total+=out.Size;}
    printf("HTTP body reuse iterations=%d bytes=%llu owning-C-us=%llu adapter-us=%llu\n",iterations,
        (unsigned long long)(total/2),(unsigned long long)raw,(unsigned long long)(xl6MonotonicMillisecondsV1()-start));
    if(xrtMemDebugEnabled()){
        assert(xrtMemDebugFailAfter(0));out.Size=0;
        xl6Http1BodyReaderResetV2(reader,ownedPlan,16777216,8192,16384,8192,32);
        assert(xl6Http1BodyReaderReadSpanV2(reader,(xbytesview){data,sizeof(data)},&out,false,-1)==XHTTP1_BODY_DATA);
        assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    }
    xl6Http1BodyReaderDestroyV2(reader);xl6HttpBodyPlanDestroyV2(ownedPlan);xrtBufferUnit(&out);
}
int main(int argc,char** argv)
{
    if(argc==2&&!strcmp(argv[1],"--benchmark-only")){performance();bodyPerformance();return xrtGetError()?1:0;}
    assert(xrtMemDebugEnable(true));construction();mutation();performance();bodyPerformance();
    assert(!xrtGetError());puts("HTTP native product contracts passed");return 0;
}
