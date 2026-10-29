#include "xl6_net_bytes_internal.h"
void* xl6NetBytesFromBufferV1(void* raw,int64_t size)
{
    xbytesview view={0};
    if(!xl6BufferInput(raw,size,&view)){xl6NetBytesFailedV1();return NULL;}
    xnetbytes* result=xrtNetBytesCopy(view.Data,view.Size);if(!result)xl6NetBytesFailedV1();return result;
}
int64_t xl6NetBytesAppendToV1(const void* raw,void* target,int64_t size,int64_t offset)
{
    xbytesview view={0};xbuffer* buffer=xl6BufferChecked(target);
    if(!buffer||!xl6NetBytesRangeV1(raw,size,offset,&view)){xl6NetBytesFailedV1();return 0;}
    if(view.Size>(size_t)INT64_MAX-buffer->Size){xrtSetErrorKind(XERR_RANGE);xl6NetBytesFailedV1();return 0;}
    if(!xl6BufferAllocation(xrtBufferAppend(buffer,view))){xl6NetBytesFailedV1();return 0;}
    return (int64_t)view.Size;
}
