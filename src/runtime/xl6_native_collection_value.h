#ifndef XL6_NATIVE_COLLECTION_VALUE_H
#define XL6_NATIVE_COLLECTION_VALUE_H
#include "xl6_text_value.h"
/* Permanent resident leaf codec. Logical nullable values flatten to null or
 * the authenticated child Value; they are never boxed optional handles.
 * Identity describes the slot; boxedIdentity describes a present projection.
 * Slot/Value are error-transparent; Copy/ToTyped are checked and failure-atomic
 * into a private empty destination. No module callbacks or graph edges. */
typedef struct XL6_NativeCollectionValueProfileV1 {
    const xrttype* (*Type)(void);
    uint64_t identity,boxedIdentity;
    const xtypedsetkeypolicy* (*Content)(void);
    bool (*Empty)(const void*);
    bool (*Copy)(const void*,void*);
    bool (*Slot)(const void*);
    bool (*Value)(const xvalue*);
    const xvalueconverter* (*Converter)(void);
} XL6_NativeCollectionValueProfileV1;
static inline bool xl6NativeCollectionValueProfileValidV1(const XL6_NativeCollectionValueProfileV1* p)
{
    if(!p || !p->Type || !p->identity || !p->boxedIdentity || !p->Content ||
        !p->Empty || !p->Copy || !p->Slot || !p->Value || !p->Converter)return false;
    const xrttype* t=p->Type();const xvalueconverter* c=p->Converter();
    const xtypedsetkeypolicy* content=p->Content();
    return t && t->Size && t->Align && !(t->Align&(t->Align-1)) && t->Size%t->Align==0 &&
        (t->Kind==XRT_TYPE_RECORD || (t->Kind==XRT_TYPE_OPTIONAL && (t->Flags&XRT_TYPE_FLAG_NULLABLE))) &&
        !(t->Flags&(XRT_TYPE_FLAG_REFERENCE|XRT_TYPE_FLAG_TRIVIAL_DROP)) && xrtTypeIsCopyable(t) &&
        t->Ops && t->Ops->Copy && t->Ops->Drop && c && c->ToTyped && c->FromTyped &&
        content && content->Hash && content->Equal;
}
#endif
