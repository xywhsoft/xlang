#include "xl6_tcp_internal.h"
#include "xl6_native_class_borrow.h"
/* Pin real class handles only through atomic native acceptance. The native
 * batch retains its own immutable blocks; no language callback escapes. */
int64_t xl6TcpStreamSendRefsV1(XL6_NativeClassArrayContextV1 context,
    void* receiver,const xvalue* array,int64_t size)
{
    int64_t result=XNET_RESULT_ERROR;xl6_error_scope work={0};
    if(!xl6ErrorScopeBegin(&work)){xl6TcpFailedV1();return result;}
    enum {STACK_REFS=64};
    _Static_assert(sizeof(xnetbytesref)%_Alignof(xvalue*)==0,"Aligned tail array of owned handles");
    XL6_NativeClassArrayPinV1 input={0};XL6_TcpScopeV1 scope={0};
    xnetbytesref localRefs[STACK_REFS]={{0}};xvalue* localHeld[STACK_REFS]={0};
    xnetbytesref* refs=localRefs;xvalue** held=localHeld;void* workspace=NULL;
    size_t captured=0;uint64_t total=0;
    bool ok=xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope)&&
        xl6NativeClassArrayBeginV1(&input,context,array,xl6NetBytesHandlePolicyV1Get());
    size_t count=ok?xrtTypedArrayCount(xl6SequenceStorageV1(input.owner)):0;
    if(ok&&size< -1){xrtSetErrorKind(XERR_RANGE);ok=false;}
    if(ok&&count>STACK_REFS){
        if(count>SIZE_MAX/(sizeof(*refs)+sizeof(*held))){xrtSetErrorKind(XERR_RANGE);ok=false;}
        else{
            workspace=xrtMalloc(count*(sizeof(*refs)+sizeof(*held)));ok=workspace!=NULL;
            if(ok){refs=workspace;held=(xvalue**)((unsigned char*)workspace+count*sizeof(*refs));}
        }
    }
    for(size_t i=0;ok&&i<count;++i){
        void* raw=NULL;held[i]=xl6NativeClassArrayRetainAtV1(&input,i,&raw);
        if(!held[i]){ok=false;break;}
        captured=i+1;xbytesview view=xrtNetBytesView(raw);
        if(view.Size>(uint64_t)INT64_MAX-total||view.Size>(uint64_t)SIZE_MAX-total){xrtSetErrorKind(XERR_RANGE);ok=false;break;}
        total+=view.Size;refs[i]=(xnetbytesref){raw,0,view.Size};
    }
    if(ok){
        size_t remaining=size<0||(uint64_t)size>total?(size_t)total:(size_t)size;
        for(size_t i=0;i<count;++i){if(refs[i].Size>remaining)refs[i].Size=remaining;remaining-=refs[i].Size;}
        result=xrtNetStreamSendBytesVec(scope.stream,refs,count);ok=result!=XNET_RESULT_ERROR;
    }
    if(!ok||xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    if(scope.execution.resource)xl6ExecutionTcpLeaveInternalV1(&scope);
    for(size_t i=captured;i>0;--i){
        xrtValueRelease(held[i-1]);
        if(xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    }
    xl6NativeClassArrayEndV1(&input);xrtFree(workspace);
    if(xrtGetError()||work.failed)xl6ErrorScopeCaptureFailure(&work);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return result;
}
