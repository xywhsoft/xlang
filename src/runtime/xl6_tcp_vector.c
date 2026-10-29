#include "xl6_tcp_vector_internal.h"
/* The typed array and actual Buffer handles stay pinned only through XRT's
 * synchronous gather-copy. No language reference enters the native queue. */
int64_t xl6TcpVectorSendInternalV1(XL6_NetVectorCaptureV1 acquire,
    XL6_NativeClassArrayContextV1 context,void* receiver,const xvalue* array,int64_t size)
{
    int64_t result=XNET_RESULT_ERROR;xl6_error_scope work={0};
    if(!xl6ErrorScopeBegin(&work)){xl6TcpFailedV1();return result;}
    XL6_NetVectorInputsV1 input={0};XL6_TcpScopeV1 scope={0};
    bool ok=xl6ExecutionTcpEnterInternalV1(receiver,XL6_EXECUTION_TCP_STREAM_V1,true,&scope);
    if(ok)ok=acquire&&acquire(&input,context,array)&&xl6NetVectorSelectSizeV1(&input,size);
    if(ok){result=xrtNetStreamSendVec(scope.stream,input.spans,input.count);ok=result!=XNET_RESULT_ERROR;}
    xl6NetVectorCaptureFailureV1(&work,ok);
    if(scope.execution.resource)xl6ExecutionTcpLeaveInternalV1(&scope);
    xl6NetVectorDiscardV1(&input,&work);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return result;
}
