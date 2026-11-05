#ifndef XL6_NATIVE_CLASS_BORROW_H
#define XL6_NATIVE_CLASS_BORROW_H
#include "xl6_native_class_abi.h"
#include "xl6_native_handle.h"
#include "xl6_sequence.h"
/* One operation pin. Caller ends even a failed Begin. The retained metadata
 * is resident-owned; context strings are never kept after synchronous End. */
typedef struct XL6_NativeClassArrayPinV1 {
    XL6_Sequence* owner;
    XL6_SequenceAdmissionV3 admission;
    XL6_NativeClassArrayContextV1 context;
} XL6_NativeClassArrayPinV1;
static inline bool xl6NativeClassArrayBeginV1(XL6_NativeClassArrayPinV1* pin,
    XL6_NativeClassArrayContextV1 context,const xvalue* input,const XL6_NativeHandlePolicyV1* expected)
{
    if(!pin||pin->owner||!expected||context.policy!=expected||!context.fieldName||!context.fieldSize||
        context.layout.transfer!=3||context.layout.size!=sizeof(xvalue*)||context.layout.type!=xrtTypeValue()){
        xrtSetErrorKind(XERR_ARGUMENT);return false;
    }
    pin->context=context;XL6_SequenceLayoutV1 layout=context.layout;
    pin->owner=xl6SequenceBeginRoleV1(input,XL6_SEQUENCE_ARRAY_V1,layout.identity,layout.elementIdentity,
        layout.transfer,layout.size,layout.type);
    if(!pin->owner||!xl6SequenceAdmissionCopyV3(pin->owner,&pin->admission))return false;
    if(!pin->admission.ops||!pin->admission.data){xrtSetErrorKind(XERR_TYPE);return false;}
    return true;
}
static inline xvalue* xl6NativeClassFieldRetainV1(XL6_NativeClassFieldContextV1 context,
    const xvalue* object,const XL6_NativeHandlePolicyV1* expected,void** raw)
{
    if(!raw||!expected||context.policy!=expected||!context.fieldName||!context.fieldSize){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xvalue* field=xrtValueObjectGet(object,(xstrview){context.fieldName,context.fieldSize});
    const xvaluehandleops* ops=NULL;void* user=NULL;void* value=NULL;
    if(!field||!xrtValueGetHandle(field,&value,&ops,&user)||!value||ops!=expected->Ops||user){xrtSetErrorKind(XERR_TYPE);return NULL;}
    xvalue* owned=xrtValueRetain(field);if(!owned)return NULL;*raw=value;return owned;
}
static inline xvalue* xl6NativeClassArrayRetainAtV1(XL6_NativeClassArrayPinV1* pin,size_t index,void** raw)
{
    if(!pin||!pin->owner||!raw){xrtSetErrorKind(XERR_ARGUMENT);return NULL;}
    xvalue* const* slot=xrtTypedArrayConstGet(xl6SequenceStorageV1(pin->owner),index);
    if(!slot||!xl6SequenceAdmitSlotV1(pin->owner,slot))return NULL;
    /* Retain the actual native handle, not a generated class wrapper. */
    XL6_NativeClassArrayContextV1 c=pin->context;
    return xl6NativeClassFieldRetainV1((XL6_NativeClassFieldContextV1){c.fieldName,c.fieldSize,c.policy},*slot,c.policy,raw);
}
static inline void xl6NativeClassArrayEndV1(XL6_NativeClassArrayPinV1* pin)
{
    XL6_NativeClassArrayPinV1 owned=*pin;*pin=(XL6_NativeClassArrayPinV1){0};
    xl6SequenceAdmissionDropV3(&owned.admission);if(owned.owner)xl6SequenceEndV1(owned.owner);
}
#endif
