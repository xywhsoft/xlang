#include "xl6_net_buffer_internal.h"
#define INPUT(Name,Prepend) \
void xl6NetBuffer##Name##TextV1(void* raw,xstrview text,int64_t size) \
{xbytesview view={(const uint8_t*)text.Data,text.Size};if(!xl6BufferSelect(&view,size)){xl6NetBufferFailedV1();return;} \
 (void)xl6NetBufferAppendViewV1(raw,view,Prepend);} \
void xl6NetBuffer##Name##BytesV1(void* raw,const xvalue* value,int64_t size) \
{xbytesview view;if(!xl6BufferBytesInput(value,size,&view)){xl6NetBufferFailedV1();return;} \
 (void)xl6NetBufferAppendViewV1(raw,view,Prepend);}
INPUT(Append,false)
INPUT(Prepend,true)
#undef INPUT
static xvalue* copyValue(void* raw,int64_t offset,int64_t size,bool consume)
{
    XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 guard={0};size_t count=0;bytes data=NULL;xvalue* value=NULL;
    if(!xl6NetBufferEnterV1(b,&guard)){xl6NetBufferFailedV1();return NULL;}bool ok=xl6NetBufferSelectRangeV1(b,offset,size,&count);
    if(ok){data=xrtMalloc(count?count:1);ok=data!=NULL;}
    if(ok&&xrtNetBufPeek(&b->native,(size_t)offset,data,count)!=count){xrtSetErrorKind(XERR_INTERNAL);ok=false;}
    if(ok){value=xrtValueBytesTake(&data,count);ok=value!=NULL;}
    xrtFree(data);if(ok&&consume)(void)xrtNetBufConsume(&b->native,count);
    xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();return value;
}
xvalue* xl6NetBufferPeekV1(void* raw,int64_t offset,int64_t size){return copyValue(raw,offset,size,false);}
xvalue* xl6NetBufferReadV1(void* raw,int64_t size){return copyValue(raw,0,size,true);}
