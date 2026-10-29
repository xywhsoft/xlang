/* Actual product providers; ordinary vectors and logical allocation prefixes. */
#include "release/include/xl_clock.h"
#include "src/runtime/xl6_ws_internal.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const xstrview request=XRT_STR_LITERAL("GET /ws HTTP/1.1\r\nHost: local.test\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n");
static const uint8 frame[]={0x82,0x83,0x12,0x34,0x56,0x78};
typedef void (*Destroy)(void*);
static Destroy drops[]={
    xl6WsFrameConfigDestroyV2,xl6WsFrameDestroyV2,xl6WsFrameDestroyV2,
    xl6WsCloseDestroyV2,xl6WsCloseDestroyV2,xl6WsMessageConfigDestroyV2,
    xl6WsMessageStateDestroyV2,xl6WsExtensionsDestroyV2,xl6WsExtensionDestroyV2,
    xl6WsExtensionDestroyV2,xl6WsDeflateOfferDestroyV2,xl6WsDeflateResponseDestroyV2,
    xl6WsDeflateDirectionDestroyV2,xl6WsInflaterConfigDestroyV2,xl6WsInflaterDestroyV2,
    xl6WsDeflaterConfigDestroyV2,xl6WsDeflaterDestroyV2,xl6WsClientOptionsDestroyV2,
    xl6WsServerOptionsDestroyV2,xl6WsUpgradeDestroyV2,xl6WsUpgradeDestroyV2,
    xl6WsStreamOptionsDestroyV2};
static void* construct(size_t kind,void* extensions,void* response,void* head)
{
    switch(kind){
    case 0:return xl6WsFrameConfigCreateV2(1048576,XWS_MASK_ANY,0,0xffff);
    case 1:return xl6WsFrameMakeDefaultV2(XWS_OPCODE_BINARY,3,XWS_FRAME_FIN|XWS_FRAME_MASKED,0x12345678);
    case 2:return xl6WsFrameParseDefaultSpanV2((xbytesview){frame,sizeof(frame)},-1);
    case 3:return xl6WsCloseMakeV2(1000,XRT_STR_LITERAL("a\0b"),-1);
    case 4:return xl6WsCloseParseSpanV2((xbytesview){(const uint8*)"\x03\xe8" "a\0b",5},-1);
    case 5:return xl6WsMessageConfigCreateV2(1048576,0,0,0,true);
    case 6:return xl6WsMessageStateCreateDefaultV2();
    case 7:return xl6WsExtensionsParseTextV2(XRT_STR_LITERAL("permessage-deflate; client_max_window_bits, custom; x=\"fast\""),-1);
    case 8:return xl6WsExtensionsAtV2(extensions,0);
    case 9:return xl6WsExtensionMakeV2(XRT_STR_LITERAL("custom"),XRT_STR_LITERAL("x=\"fast\"; bare"));
    case 10:return xl6WsDeflateOfferCreateV2(0,15,15);
    case 11:return xl6WsDeflateResponseCreateV2(0,15,15);
    case 12:return xl6WsDeflateResponseDirectionV2(response,XWS_ROLE_CLIENT,false);
    case 13:return xl6WsInflaterConfigCreateV2(1048576,15,false,true);
    case 14:return xl6WsInflaterCreateDefaultV2();
    case 15:return xl6WsDeflaterConfigCreateV2(1048576,15,false,true,6,0);
    case 16:return xl6WsDeflaterCreateDefaultV2();
    case 17:return xl6WsClientOptionsCreateV2(XRT_STR_LITERAL("chat, other"));
    case 18:return xl6WsServerOptionsCreateV2(XRT_STR_LITERAL("chat"),true,false);
    case 19:return xl6WsUpgradeClientDefaultV2(XRT_STR_LITERAL("local.test"));
    case 20:return xl6WsUpgradeServerDefaultV2(head);
    case 21:return xl6WsStreamOptionsCreateV2(1048576,1048576,1048576,1024,5000,true);
    default:assert(false);return NULL;
    }
}
static void balance(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount);
}
static void factories(void)
{
    void* extensions=xl6WsExtensionsParseTextV2(XRT_STR_LITERAL("permessage-deflate; client_max_window_bits"),-1);
    void* response=xl6WsDeflateResponseCreateV2(0,15,15);
    void* head=xl6Http1HeadRequestTextV2(request,65536,8192,8192,100,-1);
    assert(extensions && response && head && !xrtGetError());size_t prefixes=0,hits=0;
    for(size_t kind=0;kind<sizeof(drops)/sizeof(drops[0]);kind++){
        /* Warm platform RNG initialization before the fault baseline. */
        void* warm=construct(kind,extensions,response,head);
        if(!warm || xrtGetError())fprintf(stderr,"factory warmup failed: kind=%zu\n",kind);
        assert(warm && !xrtGetError());drops[kind](warm);
        bool complete=false;
        for(size_t point=0;point<128;point++){
            xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
            void* object=construct(kind,extensions,response,head);
            bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();xerror* error=xrtTakeError();
            if(hit){assert(!object && error);hits++;}else{assert(object && !error);complete=true;}
            if(object)drops[kind](object);
            xrtErrorFree(error);balance(&before);prefixes++;
            if(complete)break;
        }
        assert(complete);
    }
    xl6WsExtensionsDestroyV2(extensions);xl6WsDeflateResponseDestroyV2(response);xl6Http1HeadDestroyV2(head);
    printf("WebSocket product factories: %zu kinds, %zu complete prefixes, %zu checked failures; balanced\n",
        sizeof(drops)/sizeof(drops[0]),prefixes,hits);
}
static void performance(void)
{
    enum{iterations=100000};volatile uint64 sum=0;xwsframeconfig config;xrtWsFrameConfigInit(&config);config.MaxPayload=1048576;
    xbytesview input={frame,sizeof(frame)};uint64 begin=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;i++){xwsframe value;assert(xrtWsFrameParse(input,&value,&config,NULL)==XWS_FRAME_READY);sum+=value.HeadSize;}
    uint64 borrowed=xl6MonotonicMillisecondsV1()-begin;begin=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;i++){
        XL6_WsFrame value={0};value.config=config;
        assert(xrtWsFrameParse(input,&value.value,&value.config,NULL)==XWS_FRAME_READY);
        XL6_WsFrame* owned=xrtMalloc(sizeof(*owned));assert(owned);*owned=value;sum+=owned->value.HeadSize;xrtFree(owned);
    }
    uint64 handwritten=xl6MonotonicMillisecondsV1()-begin;xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);begin=xl6MonotonicMillisecondsV1();
    for(int i=0;i<iterations;i++){
        void* owned=xl6WsFrameParseDefaultSpanV2(input,-1);assert(owned);sum+=xl6WsFrameHeaderSizeV2(owned);xl6WsFrameDestroyV2(owned);
    }
    uint64 adapter=xl6MonotonicMillisecondsV1()-begin;xrtMemDebugSnapshot(&after);balance(&before);
    printf("WebSocket frame cost (%d iterations, same product runtime, memdebug=%d): borrowed-C=%llu us; owning-C=%llu us; owned-adapter=%llu us; adapter allocations=%llu; payload copies=0; checksum=%llu\n",
        iterations,(int)xrtMemDebugEnabled(),(unsigned long long)borrowed,(unsigned long long)handwritten,
        (unsigned long long)adapter,(unsigned long long)(after.AllocCount-before.AllocCount),(unsigned long long)sum);
    if(xrtMemDebugEnabled())assert(after.AllocCount-before.AllocCount==iterations);
}
static void defaultLimits(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    const size_t size=64u*1024u*1024u+1u;uint8* input=xrtCalloc(size,1);assert(input);
    void* inflater=xl6WsInflaterCreateDefaultV2();void* deflater=xl6WsDeflaterCreateDefaultV2();assert(inflater&&deflater);
    xbuffer output={0};
    for(int round=0;round<2;round++){
        xl6WsInflaterBeginV2(inflater,false);
        assert(xl6WsInflaterWriteIntoSpanV2(inflater,(xbytesview){input,size},&output,-1)==0);
        assert(xrtGetError()&&output.Size==0);xrtClearError();
        xl6WsDeflaterBeginV2(deflater,false);
        assert(xl6WsDeflaterWriteIntoSpanV2(deflater,(xbytesview){input,size},&output,-1)==0);
        assert(xrtGetError()&&output.Size==0);xrtClearError();
        // A default reset uses the same finite language policy as creation.
        xl6WsInflaterResetDefaultV2(inflater);xl6WsDeflaterResetDefaultV2(deflater);assert(!xrtGetError());
    }
    xl6WsInflaterDestroyV2(inflater);xl6WsDeflaterDestroyV2(deflater);xrtBufferUnit(&output);xrtFree(input);balance(&before);
    puts("WebSocket default create/reset: 64 MiB limit, passthrough boundary rejected before output, balanced");
}
int main(int argc,char** argv)
{
    if(argc==2 && !strcmp(argv[1],"--benchmark-only")){performance();return 0;}
    assert(xrtMemDebugEnable(true));factories();defaultLimits();performance();return 0;
}
