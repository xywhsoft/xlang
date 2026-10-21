#include "xl6_net_vector_input.h"
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
bool xl6NetVectorSelectSizeV1(Inputs* input,int64_t size)
{
    if(size< -1){xrtSetErrorKind(XERR_RANGE);return false;}
    size_t total=0;
    for(size_t i=0;i<input->count;++i){if(input->spans[i].Size>(uint64_t)INT64_MAX-total){xrtSetErrorKind(XERR_RANGE);return false;}total+=input->spans[i].Size;}
    size_t selected=size<0||((uint64_t)size>total)?total:(size_t)size;
    for(size_t i=0;i<input->count;++i){size_t bytes=input->spans[i].Size;if(bytes>selected)bytes=selected;
        input->spans[i].Size=bytes;selected-=bytes;}
    return true;
}
