#include "xl6_udp_transfer_internal.h"
XL6_ProductI64U64V1 xl6UdpBatchSendInternalV1(XL6_NativeClassArrayContextV1 c,void* receiver,const xvalue* array,
    void* progress,int64_t start,int64_t requested,bool owned,const XL6_NativeHandlePolicyV1* policy,XL6_UdpBatchFillV1 fill)
{
    enum {STACK_ITEMS=64};XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};XL6_NetBatchProgressV1* report=progress;
    XL6_NativeClassArrayPinV1 pin={0};XL6_UdpScopeV1 scope={0};xl6_error_scope work={0};
    if(!xl6ErrorScopeBegin(&work)){xl6UdpFailedV1();return result;}if(report)xl6NetBatchProgressClearV1(report);
    xnetudpownedmessage localOwned[STACK_ITEMS]={{0}};xnetdgramsend localCopy[STACK_ITEMS]={{0}};xvalue* localHeld[STACK_ITEMS]={0};
    xnetudpownedmessage* messages=localOwned;xnetdgramsend* packets=localCopy;xvalue** held=localHeld;void* workspace=NULL;size_t count=0,captured=0;
    bool ok=report&&start>=0&&requested>=-1;if(!ok)xrtSetErrorKind(report?XERR_RANGE:XERR_ARGUMENT);
    if(ok)ok=xl6ExecutionUdpEnterInternalV1(receiver,true,&scope)&&xl6NativeClassArrayBeginV1(&pin,c,array,policy);
    if(ok){size_t total=xrtTypedArrayCount(xl6SequenceStorageV1(pin.owner));
        if((uint64_t)start>total){xrtSetErrorKind(XERR_RANGE);ok=false;}
        else{count=total-(size_t)start;if(requested>=0&&(uint64_t)requested<count)count=(size_t)requested;}}
    if(ok&&count>STACK_ITEMS){
        size_t item=owned?sizeof(*messages):sizeof(*packets);
        _Static_assert(sizeof(xnetudpownedmessage)%_Alignof(xvalue*)==0&&sizeof(xnetdgramsend)%_Alignof(xvalue*)==0,"Aligned actual handle tail");
        if(count>SIZE_MAX/(item+sizeof(*held))){xrtSetErrorKind(XERR_RANGE);ok=false;}
        else{workspace=xrtMalloc(count*(item+sizeof(*held)));ok=workspace!=NULL;if(ok){held=(xvalue**)((uint8_t*)workspace+count*item);if(owned)messages=workspace;else packets=workspace;}}
    }
    for(size_t i=0;ok&&i<count;++i){void* raw=NULL;held[i]=xl6NativeClassArrayRetainAtV1(&pin,(size_t)start+i,&raw);if(!held[i]){ok=false;break;}captured=i+1;
        ok=fill&&(owned?fill(&messages[i],raw):fill(&packets[i],raw));}

    if(ok){size_t accepted=0;
        result.e0=owned?xrtNetUdpSendOwnedBatch(scope.udp,messages,count,&accepted):xrtNetUdpSendBatch(scope.udp,packets,count,&accepted);
        if(accepted>count)abort();
        result.e1=accepted;*report=(XL6_NetBatchProgressV1){accepted,(uint64_t)start,result.e0,true};
        ok=result.e0!=XNET_RESULT_ERROR;}
    if(!ok||xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    if(scope.execution.resource)xl6ExecutionUdpLeaveInternalV1(&scope);
    for(size_t i=captured;i>0;--i){xrtValueRelease(held[i-1]);if(xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);}
    xl6NativeClassArrayEndV1(&pin);xrtFree(workspace);xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);return result;
}
