#include "xl6_net_frame_internal.h"
#include "xl6_net_buffer_abi.h"
#include "xl6_native_handle.h"
typedef XL6_NetLineFramerV1 Line;
static void* create(xbytesview delimiter,int64_t maximum,bool include,int64_t limit,int64_t size)
{
    if(!xl6BufferSelect(&delimiter,size)||maximum<0||(uint64_t)maximum>SIZE_MAX||
       !delimiter.Size||delimiter.Size>SIZE_MAX-sizeof(Line)){
        if(!xrtGetError())xrtSetErrorKind(XERR_RANGE);
        xl6NetBufferFailedV1();return NULL;}
    Line* line=xrtCalloc(1,sizeof(*line)+delimiter.Size);bool ok=line!=NULL;
    if(ok)ok=xl6NetBufferInitializeV1(&line->input,limit)&&xl6NetFrameIdentityV1(&line->issuer);
    if(ok&&delimiter.Size>line->input.limit){xrtSetErrorKind(XERR_ARGUMENT);ok=false;}
    if(ok){memcpy(line->delimiter,delimiter.Data,delimiter.Size);
        xnetlineconfig config={{line->delimiter,delimiter.Size},(size_t)maximum,include};ok=xrtNetLineInit(&line->parser,&config);}
    if(!ok){xrtFree(line);line=NULL;xl6NetBufferFailedV1();}return line;
}
void* xl6NetLineFramerCreateTextV1(xstrview text,int64_t maximum,bool include,int64_t limit,int64_t size)
{return create((xbytesview){(const uint8_t*)text.Data,text.Size},maximum,include,limit,size);}
void* xl6NetLineFramerCreateBytesV1(const xvalue* value,int64_t maximum,bool include,int64_t limit,int64_t size)
{xbytesview data;if(!xl6BufferBytesInput(value,size,&data)){xl6NetBufferFailedV1();return NULL;}return create(data,maximum,include,limit,-1);}
void xl6NetLineFramerDestroyV1(void* raw)
{Line* line=raw;if(line){xrtNetBufClear(&line->input.native);xrtFree(line);}}
static bool restart(Line* line,bool clear)
{
    bool ok=xl6NetFrameCanAdvanceV1(line->revision)&&xrtNetLineReset(&line->parser);
    if(ok){++line->revision;line->input.faulted=false;if(clear)xrtNetBufClear(&line->input.native);}return ok;
}
void xl6NetLineFramerResetV1(void* raw)
{Line* line=raw;XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(&line->input,&guard)){xl6NetBufferFailedV1();return;}bool ok=restart(line,false);xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();}
void xl6NetLineFramerClearV1(void* raw)
{Line* line=raw;XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(&line->input,&guard)){xl6NetBufferFailedV1();return;}bool ok=restart(line,true);xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();}
xnetframestatus xl6NetLineParseV1(Line* line,xnetframe* frame)
{
    if(line->input.faulted){xrtSetErrorKind(XERR_STATE);return XNET_FRAME_ERROR;}
    xnetframestatus status=xrtNetLineNext(&line->parser,&line->input.native,frame);
    if(status==XNET_FRAME_ERROR)line->input.faulted=true;
    return status;
}
void* xl6NetLineFramerNextV1(void* raw)
{
    Line* line=raw;XL6_NetBufferGuardV1 guard={0};xnetframe frame={0};void* result=NULL;
    if(!xl6NetBufferEnterV1(&line->input,&guard)){xl6NetBufferFailedV1();return NULL;}xnetframestatus status=xl6NetLineParseV1(line,&frame);
    if(status==XNET_FRAME_MORE)result=xl6NetFrameMoreV1();
    else if(status==XNET_FRAME_READY)result=xl6NetFrameCopyV1(&line->input,&frame,line->issuer,line->revision);
    xl6NetBufferLeaveV1(&guard);if(!result)xl6NetBufferFailedV1();return result;
}
void xl6NetLineFramerCommitV1(void* raw,const void* snapshot)
{
    Line* line=raw;const XL6_NetFrameResultV1* result=xl6NetFrameReadyV1(snapshot);if(!result)return;
    XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(&line->input,&guard)){xl6NetBufferFailedV1();return;}bool ok=false;
    if(line->input.faulted||result->issuer!=line->issuer||result->revision!=line->revision)xrtSetErrorKind(XERR_STATE);
    else if(xl6NetFrameCanAdvanceV1(line->revision)){
        ok=xrtNetFrameConsume(&line->input.native,&result->frame);
        if(ok){++line->revision;ok=xrtNetLineReset(&line->parser);}}
    xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();
}
#define FRAME_NAME xl6NetLineFramer
#define FRAME_INPUT(Raw) (&((Line*)(Raw))->input)
#include "xl6_net_frame_feed.inc"
XL6_DEFINE_NET_BUFFER_HANDLE_POLICY(xl6NetLineFramerHandlePolicyV1Get,xl6NetLineFramerDestroyV1,&((Line*)raw)->input)
