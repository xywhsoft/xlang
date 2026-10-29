#include "xl6_net_frame_internal.h"
#include "xl6_native_handle.h"
/* This permanent resident value owns nothing. Language shells still use their
 * normal owning handles. The no-output-allocation nextInto path is separate. */
static const XL6_NetFrameResultV1 more={{0},0,0,false};
static xatomic64 nextIdentity={0};
bool xl6NetFrameIdentityV1(uint64_t* output)
{
    uint64_t previous=xrtAtomic64Load(&nextIdentity,XMEMORY_RELAXED);
    for(;;){if(previous==UINT64_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
        if(xrtAtomic64CompareExchange(&nextIdentity,&previous,previous+1,XMEMORY_RELAXED,XMEMORY_RELAXED)){
            *output=previous+1;return true;}}
}
void* xl6NetFrameMoreV1(void){return (void*)&more;}
void xl6NetFrameResultDestroyV1(void* raw){if(raw!=(const void*)&more)xrtFree(raw);}
void* xl6NetFrameCopyV1(const XL6_NetBufferV1* input,const xnetframe* frame,uint64_t issuer,uint64_t revision)
{
    if(frame->PayloadSize>SIZE_MAX-sizeof(XL6_NetFrameResultV1)){xrtSetErrorKind(XERR_RANGE);return NULL;}
    XL6_NetFrameResultV1* value=xrtMalloc(sizeof(*value)+frame->PayloadSize);if(!value)return NULL;
    value->frame=*frame;value->issuer=issuer;value->revision=revision;value->ready=true;
    if(xrtNetFrameCopy(&input->native,frame,value->data,frame->PayloadSize)!=frame->PayloadSize){
        xrtFree(value);xrtSetErrorKind(XERR_INTERNAL);return NULL;}
    return value;
}
const XL6_NetFrameResultV1* xl6NetFrameReadyV1(const void* raw)
{
    const XL6_NetFrameResultV1* value=raw;
    if(raw==(const void*)&more||!value||!value->ready){xrtSetErrorKind(XERR_STATE);xl6NetBufferFailedV1();return NULL;}return value;
}
int64_t xl6NetFrameResultStatusV1(const void* raw){return raw==(const void*)&more?0:1;}
int64_t xl6NetFrameResultPayloadOffsetV1(const void* raw)
{const XL6_NetFrameResultV1* v=xl6NetFrameReadyV1(raw);return v?(int64_t)v->frame.PayloadOffset:0;}
int64_t xl6NetFrameResultPayloadSizeV1(const void* raw)
{const XL6_NetFrameResultV1* v=xl6NetFrameReadyV1(raw);return v?(int64_t)v->frame.PayloadSize:0;}
int64_t xl6NetFrameResultFrameSizeV1(const void* raw)
{const XL6_NetFrameResultV1* v=xl6NetFrameReadyV1(raw);return v?(int64_t)v->frame.FrameSize:0;}
uint64_t xl6NetFrameResultDeclaredV1(const void* raw)
{const XL6_NetFrameResultV1* v=xl6NetFrameReadyV1(raw);return v?v->frame.Declared:0;}
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6NetFrameResultHandlePolicyV1Get,xl6NetFrameResultDestroyV1)
