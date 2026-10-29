#include "xl6_net_buffer_internal.h"
#include "xl6_net_bytes_internal.h"
void xl6NetBufferAppendRefV1(void* receiver,void* input,int64_t size,int64_t offset)
{
    xbytesview view={0};XL6_NetBufferV1* buffer=receiver;XL6_NetBufferGuardV1 guard={0};
    bool ok=xl6NetBytesRangeV1(input,size,offset,&view);
    if(ok)ok=xl6NetBufferEnterV1(buffer,&guard);
    if(ok)ok=xl6NetBufferCanAppendV1(buffer,view.Size)&&
        xrtNetBufAppendBytes(&buffer->native,input,(size_t)offset,view.Size);
    if(guard.owner)xl6NetBufferLeaveV1(&guard);
    if(!ok)xl6NetBufferFailedV1();
}
