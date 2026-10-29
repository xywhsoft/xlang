#ifndef XL6_CHANNEL_SLOT_H
#define XL6_CHANNEL_SLOT_H
#include "xl6_sequence.h"

/* Compiler-private owned message slots. Reuse the schema's independently
 * owned resident policy; a physical pointer is not a slot/graph capability.
 * This adds no generated callback, converter, temporary Value or Future.
 * Callers retain the complete admission for as long as any slot is live. */
static inline bool xl6ChannelSlotPolicyV1(XL6_SequenceLayoutV1 layout,const XL6_SequenceAdmissionV3* p)
{
    return p && p->ops && p->data && p->ops->Slot && p->ops->Retain && p->ops->Drop &&
        p->elementIdentity==layout.elementIdentity && p->transfer==layout.transfer &&
        p->size==layout.size && p->type==layout.type &&
        ((layout.transfer>=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)==(p->ops->TraceSlot!=NULL)) &&
        (layout.transfer!=XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1 || xl6SequenceNativeValuePolicyV1(p)) &&
        xl6SequenceTupleV1(layout.identity,layout.elementIdentity,layout.transfer,layout.size,layout.type);
}
static inline bool xl6ChannelSlotEmptyV1(XL6_SequenceLayoutV1 layout,const XL6_SequenceAdmissionV3* p,const void* slot)
{
    if(!slot)return false;
    switch(layout.transfer){
    case 1:return true;
    case 2:return !((const xstrview*)slot)->Data && !((const xstrview*)slot)->Size;
    case 3:return !*(xvalue* const*)slot;
    case XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1:return !*(xfuture* const*)slot;
    case XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1:{
        const XL6_SequenceNativeValuePolicyV1* native=xl6SequenceNativeValuePolicyV1(p);
        return native && native->Empty(slot);
    }
    default:return false;
    }
}
static inline bool xl6ChannelSlotCopyV1(XL6_SequenceLayoutV1 layout,const XL6_SequenceAdmissionV3* p,
    void* output,const void* source)
{
    if(layout.transfer==XL6_SEQUENCE_TRANSFER_NATIVE_VALUE_V1){
        const XL6_SequenceNativeValuePolicyV1* native=xl6SequenceNativeValuePolicyV1(p);
        return native && native->Copy(source,output);
    }
    return xrtTypeCopyValue(xl6SequenceStorageTypeV1(layout.transfer,layout.type),output,source);
}
static inline void xl6ChannelSlotDropV1(XL6_SequenceLayoutV1 layout,void* slot)
{if(slot)xrtTypeDropValue(xl6SequenceStorageTypeV1(layout.transfer,layout.type),slot);}
static inline bool xl6ChannelSlotTraceV1(XL6_SequenceLayoutV1 layout,const XL6_SequenceAdmissionV3* p,
    const void* slot,xrtownershipvisitor visit,void* context)
{
    if(!visit||!slot)return false;
    if(layout.transfer==3)return visit(xrtValueOwnership(*(xvalue* const*)slot),context);
    if(layout.transfer>=XL6_SEQUENCE_TRANSFER_FUTURE_REF_V1)
        return p && p->ops && p->ops->TraceSlot && p->ops->TraceSlot(p->data,slot,visit,context);
    return true;
}
#endif
