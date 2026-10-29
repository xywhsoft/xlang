#include "xl6_net_frame_internal.h"
#include "xl6_net_buffer_abi.h"
#include "xl6_native_handle.h"
typedef XL6_NetLengthFramerV1 Length;
void* xl6NetLengthFramerCreateV1(int64_t offset,int64_t width,int64_t adjustment,int64_t strip,int64_t maximum,bool little,int64_t limit)
{
    if(offset<0||width<=0||width>8||strip<0||maximum<=0||(uint64_t)offset>SIZE_MAX||
       (uint64_t)strip>SIZE_MAX||(uint64_t)maximum>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);xl6NetBufferFailedV1();return NULL;}
    Length* length=xrtCalloc(1,sizeof(*length));bool ok=length!=NULL;
    if(ok)ok=xl6NetBufferInitializeV1(&length->input,limit)&&xl6NetFrameIdentityV1(&length->issuer);
    if(ok){xnetlengthconfig config={(size_t)offset,(size_t)width,adjustment,(size_t)strip,(size_t)maximum,
            little?XNET_FRAME_LITTLE_ENDIAN:XNET_FRAME_BIG_ENDIAN};ok=xrtNetLengthInit(&length->parser,&config);
        if(ok&&(size_t)offset+(size_t)width>length->input.limit){xrtSetErrorKind(XERR_ARGUMENT);ok=false;}}
    if(!ok){xrtFree(length);length=NULL;xl6NetBufferFailedV1();}return length;
}
void xl6NetLengthFramerDestroyV1(void* raw)
{Length* length=raw;if(length){xrtNetBufClear(&length->input.native);xrtFree(length);}}
void xl6NetLengthFramerResetV1(void* raw)
{Length* length=raw;XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(&length->input,&guard)){xl6NetBufferFailedV1();return;}bool ok=xl6NetFrameCanAdvanceV1(length->revision);
 if(ok){++length->revision;length->input.faulted=false;}xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();}
void xl6NetLengthFramerClearV1(void* raw)
{Length* length=raw;XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(&length->input,&guard)){xl6NetBufferFailedV1();return;}bool ok=xl6NetFrameCanAdvanceV1(length->revision);
 if(ok){++length->revision;length->input.faulted=false;xrtNetBufClear(&length->input.native);}xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();}
xnetframestatus xl6NetLengthParseV1(Length* length,xnetframe* frame)
{
    if(length->input.faulted){xrtSetErrorKind(XERR_STATE);return XNET_FRAME_ERROR;}
    xnetframestatus status=xrtNetLengthNext(&length->parser,&length->input.native,frame);
    if(status==XNET_FRAME_ERROR)length->input.faulted=true;
    return status;
}
void* xl6NetLengthFramerNextV1(void* raw)
{
    Length* length=raw;XL6_NetBufferGuardV1 guard={0};xnetframe frame={0};void* result=NULL;
    if(!xl6NetBufferEnterV1(&length->input,&guard)){xl6NetBufferFailedV1();return NULL;}xnetframestatus status=xl6NetLengthParseV1(length,&frame);
    if(status==XNET_FRAME_MORE)result=xl6NetFrameMoreV1();
    else if(status==XNET_FRAME_READY)result=xl6NetFrameCopyV1(&length->input,&frame,length->issuer,length->revision);
    xl6NetBufferLeaveV1(&guard);if(!result)xl6NetBufferFailedV1();return result;
}
void xl6NetLengthFramerCommitV1(void* raw,const void* snapshot)
{
    Length* length=raw;const XL6_NetFrameResultV1* result=xl6NetFrameReadyV1(snapshot);if(!result)return;
    XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(&length->input,&guard)){xl6NetBufferFailedV1();return;}bool ok=false;
    if(length->input.faulted||result->issuer!=length->issuer||result->revision!=length->revision)xrtSetErrorKind(XERR_STATE);
    else if(xl6NetFrameCanAdvanceV1(length->revision)){
        ok=xrtNetFrameConsume(&length->input.native,&result->frame);if(ok)++length->revision;}
    xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();
}
#define FRAME_NAME xl6NetLengthFramer
#define FRAME_INPUT(Raw) (&((Length*)(Raw))->input)
#include "xl6_net_frame_feed.inc"
XL6_DEFINE_NET_BUFFER_HANDLE_POLICY(xl6NetLengthFramerHandlePolicyV1Get,xl6NetLengthFramerDestroyV1,&((Length*)raw)->input)
