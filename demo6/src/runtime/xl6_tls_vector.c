#include "xl6_tls_data_internal.h"
/* All array slots/Buffer handles stay pinned through native gather-copy only.
 * No language storage or array view enters the asynchronous TLS FIFO. */
xfuture* xl6TlsVectorSourceV1(XL6_NetVectorCaptureV1 acquire,XL6_NativeClassArrayContextV1 context,
    void* receiver,const xvalue* array,int64_t size,size_t* count)
{
    if(!count){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}*count=0;
    xl6_error_scope errors={0};if(!xl6ErrorScopeBegin(&errors))return NULL;
    XL6_NetVectorInputsV1 input={0};XL6_TlsScopeV1 scope={0};xfuture* result=NULL;
    bool ok=xl6ExecutionTlsStreamEnterV1(receiver,true,&scope);
    if(ok)ok=acquire&&acquire(&input,context,array)&&xl6NetVectorSelectSizeV1(&input,size);
    if(ok){
        for(size_t i=0;i<input.count;++i)*count+=input.spans[i].Size;
        result=xrtTlsStreamSendVecAsync(scope.stream,input.spans,input.count);ok=result!=NULL;
    }
    xl6NetVectorCaptureFailureV1(&errors,ok);
    if(scope.execution.resource)xl6ExecutionTlsLeaveV1(&scope);
    xl6NetVectorDiscardV1(&input,&errors);
    xl6_error_result outcome=xl6ErrorScopeEnd(&errors);
    if(!outcome.ok){
        xl6_error_scope cleanup={0};if(!xl6ErrorScopeBeginCleanup(&cleanup,true))abort();
        if(result&&xrtFutureState(result)==XFUTURE_PENDING)(void)xrtFutureCancel(result);
        xrtFutureDestroy(result);xl6_error_result discarded=xl6ErrorScopeEnd(&cleanup);xrtErrorFree(discarded.error);
        xrtSetErrorTake(outcome.error);return NULL;
    }
    xrtErrorFree(outcome.error);return result;
}
