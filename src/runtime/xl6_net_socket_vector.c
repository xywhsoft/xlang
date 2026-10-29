#include "xl6_net_vector_internal.h"
typedef XL6_NetVectorInputsV1 Inputs;
XL6_ProductI64U64V1 xl6NetVectorSendV1(XL6_NetVectorCaptureV1 acquire,XL6_NativeClassArrayContextV1 context,void* raw,const xvalue* array,
    const void* target,const void* control,int mode,int64_t size)
{
    XL6_ProductI64U64V1 result={XNET_RESULT_ERROR,0};xl6_error_scope work={0};
    if(!xl6ErrorScopeBegin(&work)){xl6ErrorScopePropagateFailure((xl6_error_result){false,xrtTakeError()});return result;}
    Inputs input={0};XL6_NetSocketScopeV1 socket={0};
    bool ok=acquire&&((mode!=1&&mode!=3)||target);if(!ok)xrtSetErrorKind(XERR_ARGUMENT);
    if(ok&&mode>=2&&!control){xrtSetErrorKind(XERR_ARGUMENT);ok=false;}
    if(ok)ok=xl6NetSocketOwnerEnterV1(raw,&socket);
    if(ok)ok=acquire(&input,context,array);
    if(ok)ok=xl6NetVectorSelectSizeV1(&input,size);
    if(ok){size_t sent=0;
        result.e0=mode==0?xrtNetSocketSendVec(socket.native,input.spans,input.count,&sent):
            mode==1?xrtNetSocketSendToVec(socket.native,input.spans,input.count,&sent,target):
            xrtNetSocketSendMsgVec(socket.native,input.spans,input.count,&sent,target,control);
        result.e1=sent;ok=result.e0!=XNET_RESULT_ERROR;
    }
    xl6NetVectorCaptureFailureV1(&work,ok);
    if(socket.owner)xl6NetSocketOwnerLeaveV1(&socket);
    xl6NetVectorDiscardV1(&input,&work);
    xl6_error_result outcome=xl6ErrorScopeEnd(&work);
    if(!outcome.ok)xl6ErrorScopePropagateFailure(outcome);else xrtErrorFree(outcome.error);
    return result;
}
