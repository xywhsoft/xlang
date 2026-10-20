#include "xl6_net_frame_internal.h"
xvalue* xl6NetFrameResultBytesV1(const void* raw)
{
    const XL6_NetFrameResultV1* value=xl6NetFrameReadyV1(raw);if(!value)return NULL;
    xvalue* result=xrtValueBytes((xbytesview){value->data,value->frame.PayloadSize});
    if(!result)xl6NetBufferFailedV1();
    return result;
}
