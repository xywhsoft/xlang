#include "xl6_net_frame_internal.h"
#include "xl6_text_internal.h"
xstrview xl6NetFrameResultTextV1(const void* raw)
{
    const XL6_NetFrameResultV1* value=xl6NetFrameReadyV1(raw);if(!value)return (xstrview){0};
    xstrview view={(const char*)value->data,value->frame.PayloadSize};
    if(!xl6TextSelect(&view,-1)){xl6NetBufferFailedV1();return (xstrview){0};}
    xstrview result=xl6TextCopy(view);if(!result.Data)xl6NetBufferFailedV1();return result;
}
