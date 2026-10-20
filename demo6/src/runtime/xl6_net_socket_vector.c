#include "xl6_net_vector_internal.h"
typedef XL6_NetVectorInputsV1 Inputs;
void xl6NetVectorCaptureFailureV1(xl6_error_scope* scope,bool ok)
{if(!ok||xrtGetError()||scope->failed)xl6ErrorScopeCaptureFailure(scope);}
static bool sizeCheck(size_t count)
{if(count<=VECTOR_MAX)return true;xrtSetErrorKind(XERR_RANGE);return false;}
bool xl6NetVectorPrimitiveCaptureV1(Inputs* input,const xvalue* array,int kind,
    bool (*admit)(const XL6_SequenceAdmissionV3*,uint64_t))
{
    XL6_SequenceLayoutV1 layout={0};const xvalueconverter* converter=NULL;
    int found=xl6SequenceBeginErasedArrayV1(array,&input->primitive,&layout,&converter);
    if(found!=1){if(!found)xrtSetErrorKind(XERR_TYPE);return false;}
    if(!xl6SequenceAdmissionCopyV3(input->primitive,&input->admission))return false;
    if(!admit||!admit(&input->admission,layout.elementIdentity)){xrtSetErrorKind(XERR_TYPE);return false;}
    xtypedarray* storage=xl6SequenceStorageV1(input->primitive);size_t count=xrtTypedArrayCount(storage);
    if(!sizeCheck(count))return false;
    for(size_t i=0;i<count;++i){
        const void* slot=xrtTypedArrayConstGet(storage,i);if(!slot||!xl6SequenceAdmitSlotV1(input->primitive,slot))return false;
        xbytesview bytes={0};
        if(kind==TEXT_INPUT){const xstrview* text=slot;bytes=(xbytesview){(const uint8_t*)text->Data,text->Size};}
        else if(!xrtValueGetBytes(*(xvalue* const*)slot,&bytes)){xrtSetErrorKind(XERR_TYPE);return false;}
        input->spans[i]=(xnetspan){bytes.Data,bytes.Size};
    }
    input->count=count;return true;
}
void xl6NetVectorDiscardV1(Inputs* input,xl6_error_scope* scope)
{
    for(size_t i=input->count;i>0;--i){xrtValueRelease(input->held[i-1]);xl6NetVectorCaptureFailureV1(scope,true);}
    xl6NativeClassArrayEndV1(&input->classes);xl6NetVectorCaptureFailureV1(scope,true);
    xl6SequenceAdmissionDropV3(&input->admission);
    if(input->primitive)xl6SequenceEndV1(input->primitive);
    xl6NetVectorCaptureFailureV1(scope,true);
}
static bool selectSize(Inputs* input,int64_t size)
{
    if(size< -1){xrtSetErrorKind(XERR_RANGE);return false;}
    size_t total=0;
    for(size_t i=0;i<input->count;++i){if(input->spans[i].Size>(uint64_t)INT64_MAX-total){xrtSetErrorKind(XERR_RANGE);return false;}total+=input->spans[i].Size;}
    size_t selected=size<0||((uint64_t)size>total)?total:(size_t)size;
    for(size_t i=0;i<input->count;++i){size_t bytes=input->spans[i].Size;if(bytes>selected)bytes=selected;
        input->spans[i].Size=bytes;selected-=bytes;}
    return true;
}
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
    if(ok)ok=selectSize(&input,size);
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
