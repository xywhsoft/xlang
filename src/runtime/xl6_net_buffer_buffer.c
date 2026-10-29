#include "xl6_net_buffer_internal.h"
void xl6NetBufferAppendBufferV1(void* raw,void* input,int64_t size)
{xbytesview view;if(!xl6BufferInput(input,size,&view)){xl6NetBufferFailedV1();return;}(void)xl6NetBufferAppendViewV1(raw,view,false);}
void xl6NetBufferPrependBufferV1(void* raw,void* input,int64_t size)
{xbytesview view;if(!xl6BufferInput(input,size,&view)){xl6NetBufferFailedV1();return;}(void)xl6NetBufferAppendViewV1(raw,view,true);}
bool xl6NetBufferWriteIntoV1(const XL6_NetBufferV1* input,xbuffer* output,size_t offset,size_t count)
{
    if(count>(size_t)INT64_MAX-output->Size){xrtSetErrorKind(XERR_RANGE);return false;}
    if(!xl6BufferAllocation(xrtBufferReserve(output,output->Size+count)))return false;
    if(xrtNetBufPeek(&input->native,offset,count?output->Data+output->Size:NULL,count)!=count){xrtSetErrorKind(XERR_INTERNAL);return false;}
    output->Size+=count;return true;
}
static int64_t copyInto(void* raw,void* target,int64_t offset,int64_t size,bool consume)
{
    xbuffer* output=xl6BufferChecked(target);if(!output){xl6NetBufferFailedV1();return 0;}
    XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 guard={0};size_t count=0;if(!xl6NetBufferEnterV1(b,&guard)){xl6NetBufferFailedV1();return 0;}
    bool ok=xl6NetBufferSelectRangeV1(b,offset,size,&count)&&xl6NetBufferWriteIntoV1(b,output,(size_t)offset,count);
    if(ok&&consume)(void)xrtNetBufConsume(&b->native,count);
    xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();return ok?(int64_t)count:0;
}
int64_t xl6NetBufferPeekIntoV1(void* raw,void* target,int64_t offset,int64_t size){return copyInto(raw,target,offset,size,false);}
int64_t xl6NetBufferReadIntoV1(void* raw,void* target,int64_t size){return copyInto(raw,target,0,size,true);}
