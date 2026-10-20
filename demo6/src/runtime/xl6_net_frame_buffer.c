#include "xl6_net_frame_internal.h"
void xl6NetFrameResultCopyIntoV1(const void* raw,void* target)
{
    const XL6_NetFrameResultV1* value=xl6NetFrameReadyV1(raw);xbuffer* output=xl6BufferChecked(target);
    if(!value||!output){xl6NetBufferFailedV1();return;}
    if(value->frame.PayloadSize>(size_t)INT64_MAX-output->Size){xrtSetErrorKind(XERR_RANGE);xl6NetBufferFailedV1();return;}
    if(!xl6BufferAllocation(xrtBufferAppend(output,(xbytesview){value->data,value->frame.PayloadSize})))xl6NetBufferFailedV1();
}
