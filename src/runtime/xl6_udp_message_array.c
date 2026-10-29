#include "xl6_udp_message_internal.h"
typedef XL6_UdpMessageV1 Message;
void* xl6UdpMessageRefsV1(XL6_NativeClassArrayContextV1 c,const xvalue* input,int64_t size)
{
    Message* m=NULL;XL6_NativeClassArrayPinV1 pin={0};xl6_error_scope work={0};
    if(!xl6ErrorScopeBegin(&work)){xl6UdpFailedV1();return NULL;}
    bool ok=size>=-1;if(!ok)xrtSetErrorKind(XERR_RANGE);
    if(ok)ok=xl6NativeClassArrayBeginV1(&pin,c,input,xl6NetBytesHandlePolicyV1Get());
    size_t count=ok?xrtTypedArrayCount(xl6SequenceStorageV1(pin.owner)):0;
    if(ok){m=xl6UdpMessageAllocateInternalV1(count);ok=m!=NULL;}
    if(ok){m->native.Kind=XNET_UDP_OWNED_VECTOR;m->native.Payload.Vector.Items=(xnetbytesref*)(m+1);}
    uint64_t total=0;
    for(size_t i=0;ok&&i<count;++i){
        void* raw=NULL;xvalue* held=xl6NativeClassArrayRetainAtV1(&pin,i,&raw);if(!held){ok=false;break;}
        xbytesview view=xrtNetBytesView(raw);xnetbytes* owned=NULL;
        if(view.Size>(uint64_t)INT64_MAX-total||view.Size>(uint64_t)SIZE_MAX-total)xrtSetErrorKind(XERR_RANGE);
        else{total+=view.Size;owned=xrtNetBytesRef(raw);}
        xerror* error=owned?NULL:xrtTakeError();xrtValueRelease(held);
        if(!owned){xrtClearError();xrtSetErrorTake(error);ok=false;break;}
        ((xnetbytesref*)m->native.Payload.Vector.Items)[i]=(xnetbytesref){owned,0,view.Size};m->native.Payload.Vector.Count=i+1;
    }
    if(ok){size_t remaining=size<0||(uint64_t)size>total?(size_t)total:(size_t)size;ok=xl6UdpMessageSizeValidInternalV1(remaining);m->length=remaining;
        for(size_t i=0;i<count;++i){xnetbytesref* ref=(xnetbytesref*)m->native.Payload.Vector.Items+i;if(ref->Size>remaining)ref->Size=remaining;remaining-=ref->Size;}}
    if(!ok||xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    xl6NativeClassArrayEndV1(&pin);
    xl6_error_result result=xl6ErrorScopeEnd(&work);
    if(!result.ok){xl6UdpMessageDestroyV1(m);xrtClearError();xl6ErrorScopePropagateFailure(result);return NULL;}
    xrtErrorFree(result.error);return m;
}
