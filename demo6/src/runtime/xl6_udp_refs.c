#include "xl6_udp_internal.h"
#include "xl6_native_class_borrow.h"
static int64_t send(XL6_NativeClassArrayContextV1 c,void* receiver,const void* target,const void* control,const xvalue* array,int64_t size)
{
    XL6_NativeClassArrayPinV1 pin={0};XL6_UdpScopeV1 scope={0};xl6_error_scope work={0};int64_t result=XNET_RESULT_ERROR;
    if(!xl6ErrorScopeBegin(&work)){xl6UdpFailedV1();return result;}
    xnetbytesref refs[XNET_UDP_VECTOR_MAX]={{0}};xvalue* held[XNET_UDP_VECTOR_MAX]={0};size_t captured=0;uint64_t total=0;
    bool ok=size>=-1;if(!ok)xrtSetErrorKind(XERR_RANGE);
    if(ok)ok=xl6ExecutionUdpEnterInternalV1(receiver,true,&scope)&&xl6NativeClassArrayBeginV1(&pin,c,array,xl6NetBytesHandlePolicyV1Get());
    size_t count=ok?xrtTypedArrayCount(xl6SequenceStorageV1(pin.owner)):0;
    if(ok&&count>XNET_UDP_VECTOR_MAX){xrtSetErrorKind(XERR_RANGE);ok=false;}
    for(size_t i=0;ok&&i<count;++i){void* raw=NULL;held[i]=xl6NativeClassArrayRetainAtV1(&pin,i,&raw);if(!held[i]){ok=false;break;}captured=i+1;
        xbytesview view=xrtNetBytesView(raw);if(view.Size>(uint64_t)INT64_MAX-total||view.Size>(uint64_t)SIZE_MAX-total){xrtSetErrorKind(XERR_RANGE);ok=false;break;}
        total+=view.Size;refs[i]=(xnetbytesref){raw,0,view.Size};}
    if(ok){size_t remaining=size<0||(uint64_t)size>total?(size_t)total:(size_t)size;
        if(remaining>XNET_UDP_PAYLOAD_MAX){xrtSetErrorKind(XERR_RANGE);ok=false;}
        else{for(size_t i=0;i<count;++i){if(refs[i].Size>remaining)refs[i].Size=remaining;remaining-=refs[i].Size;}
            result=xrtNetUdpSendMsgBytesVec(scope.udp,target,control,refs,count);ok=result!=XNET_RESULT_ERROR;}}
    if(!ok||xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    if(scope.execution.resource)xl6ExecutionUdpLeaveInternalV1(&scope);
    for(size_t i=captured;i>0;--i){xrtValueRelease(held[i-1]);if(xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);}
    xl6NativeClassArrayEndV1(&pin);xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);return result;
}
int64_t xl6UdpSendRefsV1(XL6_NativeClassArrayContextV1 c,void* r,const xvalue* a,int64_t s){return send(c,r,NULL,NULL,a,s);}
int64_t xl6UdpSendRefsToV1(XL6_NativeClassArrayContextV1 c,void* r,const void* t,const xvalue* a,int64_t s){return send(c,r,t,NULL,a,s);}
int64_t xl6UdpSendRefsMessageV1(XL6_NativeClassArrayContextV1 c,void* r,const void* t,const void* control,const xvalue* a,int64_t s){return send(c,r,t,control,a,s);}
int64_t xl6UdpSendRefsConnectedMessageV1(XL6_NativeClassArrayContextV1 c,void* r,const void* control,const xvalue* a,int64_t s){return send(c,r,NULL,control,a,s);}
