#include "xl6_net_buffer_internal.h"
#include "xl6_native_handle.h"
bool xl6NetBufferEnterV1(XL6_NetBufferV1* buffer,XL6_NetBufferGuardV1* guard)
{
    if(!buffer||!guard||guard->owner){xrtSetErrorKind(XERR_ARGUMENT);return false;}
    if(!xrtOwnershipMutationBegin(&guard->mutation))return false;
    uint32_t available=0;
    if(!xrtAtomic32CompareExchange(&buffer->gate,&available,1,XMEMORY_ACQUIRE,XMEMORY_RELAXED)){
        if(!xrtOwnershipScopeEnd(&guard->mutation))abort();
        xrtSetErrorKind(XERR_STATE);return false;}
    guard->owner=buffer;return true;
}
void xl6NetBufferLeaveV1(XL6_NetBufferGuardV1* guard)
{
    if(!guard||!guard->owner)abort();
    xrtAtomic32Store(&guard->owner->gate,0,XMEMORY_RELEASE);guard->owner=NULL;
    if(!xrtOwnershipScopeEnd(&guard->mutation))abort();
}
bool xl6NetBufferInitializeV1(XL6_NetBufferV1* buffer,int64_t limit)
{
    if(limit<=0||(uint64_t)limit>SIZE_MAX){xrtSetErrorKind(XERR_RANGE);return false;}
    buffer->limit=(size_t)limit;return xrtNetBufInit(&buffer->native,NULL);
}
void* xl6NetBufferCreateV1(int64_t limit)
{
    XL6_NetBufferV1* buffer=xrtCalloc(1,sizeof(*buffer));
    if(buffer&&!xl6NetBufferInitializeV1(buffer,limit)){xrtFree(buffer);buffer=NULL;}
    if(!buffer)xl6NetBufferFailedV1();
    return buffer;
}
void xl6NetBufferDestroyV1(void* raw)
{XL6_NetBufferV1* buffer=raw;if(buffer){xrtNetBufClear(&buffer->native);xrtFree(buffer);}}
int64_t xl6NetBufferLengthV1(void* raw)
{XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetBufferEnterV1(b,&g)){xl6NetBufferFailedV1();return 0;}size_t n=b->native.Size;xl6NetBufferLeaveV1(&g);return (int64_t)n;}
int64_t xl6NetBufferSpanCountV1(void* raw)
{XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetBufferEnterV1(b,&g)){xl6NetBufferFailedV1();return 0;}size_t n=xrtNetBufSpanCount(&b->native);xl6NetBufferLeaveV1(&g);return (int64_t)n;}
int64_t xl6NetBufferLimitV1(const void* raw){return (int64_t)((const XL6_NetBufferV1*)raw)->limit;}
void xl6NetBufferClearV1(void* raw)
{XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 g={0};if(!xl6NetBufferEnterV1(b,&g)){xl6NetBufferFailedV1();return;}xrtNetBufClear(&b->native);b->faulted=false;xl6NetBufferLeaveV1(&g);}
bool xl6NetBufferSelectRangeV1(const XL6_NetBufferV1* buffer,int64_t offset,int64_t size,size_t* count)
{
    if(offset<0||(uint64_t)offset>buffer->native.Size||size< -1){xrtSetErrorKind(XERR_RANGE);return false;}
    size_t available=buffer->native.Size-(size_t)offset;
    *count=size<0||(uint64_t)size>available?available:(size_t)size;return true;
}
bool xl6NetBufferAppendViewV1(XL6_NetBufferV1* buffer,xbytesview input,bool prepend)
{
    XL6_NetBufferGuardV1 guard={0};if(!xl6NetBufferEnterV1(buffer,&guard)){xl6NetBufferFailedV1();return false;}
    bool ok=xl6NetBufferCanAppendV1(buffer,input.Size)&&(prepend?
        xrtNetBufPrepend(&buffer->native,input.Data,input.Size):xrtNetBufAppend(&buffer->native,input.Data,input.Size));
    xl6NetBufferLeaveV1(&guard);if(!ok)xl6NetBufferFailedV1();return ok;
}
void xl6NetBufferMoveInputV1(XL6_NetBufferV1* target,XL6_NetBufferV1* source)
{
    if(target==source){xrtSetErrorKind(XERR_ARGUMENT);xl6NetBufferFailedV1();return;}
    /* A single total lock order for opposite cross-thread moves. This moves
     * actual owned blocks; neither endpoint retains an object/pool reference. */
    XL6_NetBufferV1* first=(uintptr_t)target<(uintptr_t)source?target:source;
    XL6_NetBufferV1* second=first==target?source:target;
    XL6_NetBufferGuardV1 a={0},b={0};if(!xl6NetBufferEnterV1(first,&a)){xl6NetBufferFailedV1();return;}
    if(!xl6NetBufferEnterV1(second,&b)){xl6NetBufferLeaveV1(&a);xl6NetBufferFailedV1();return;}
    bool ok=xl6NetBufferCanAppendV1(target,source->native.Size)&&xrtNetBufMove(&target->native,&source->native);
    xl6NetBufferLeaveV1(&b);xl6NetBufferLeaveV1(&a);if(!ok)xl6NetBufferFailedV1();
}
void xl6NetBufferAppendMoveV1(void* target,void* source){xl6NetBufferMoveInputV1(target,source);}
int64_t xl6NetBufferConsumeV1(void* raw,int64_t size)
{
    XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 g={0};size_t count=0;if(!xl6NetBufferEnterV1(b,&g)){xl6NetBufferFailedV1();return 0;}
    bool ok=xl6NetBufferSelectRangeV1(b,0,size,&count);if(ok)count=xrtNetBufConsume(&b->native,count);
    xl6NetBufferLeaveV1(&g);if(!ok)xl6NetBufferFailedV1();return (int64_t)count;
}
int64_t xl6NetBufferFindV1(void* raw,int64_t byte,int64_t offset)
{
    if(byte<0||byte>255){xrtSetErrorKind(XERR_RANGE);xl6NetBufferFailedV1();return -1;}
    XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 g={0};size_t count=0,index=XRT_NPOS;if(!xl6NetBufferEnterV1(b,&g)){xl6NetBufferFailedV1();return -1;}
    bool ok=xl6NetBufferSelectRangeV1(b,offset,0,&count);if(ok)index=xrtNetBufFind(&b->native,(uint8)byte,(size_t)offset);
    xl6NetBufferLeaveV1(&g);if(!ok)xl6NetBufferFailedV1();return index==XRT_NPOS?-1:(int64_t)index;
}
void xl6NetBufferCompactV1(void* raw,int64_t size)
{
    XL6_NetBufferV1* b=raw;XL6_NetBufferGuardV1 g={0};size_t count=0;xnetspan span={0};if(!xl6NetBufferEnterV1(b,&g)){xl6NetBufferFailedV1();return;}
    bool ok=xl6NetBufferSelectRangeV1(b,0,size,&count)&&xrtNetBufPullup(&b->native,count,&span);
    xl6NetBufferLeaveV1(&g);if(!ok)xl6NetBufferFailedV1();
}
XL6_DEFINE_NET_BUFFER_HANDLE_POLICY(xl6NetBufferHandlePolicyV1Get,xl6NetBufferDestroyV1,raw)
