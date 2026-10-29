#include "src/runtime/xl6_slot_map.h"
#include "src/runtime/xl6_slot_map_key_value.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_reference_value.h"
#include "src/runtime/xl6_string_type.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int copyFailure,iteratorFailure; static bool reenter;
static const xvalue* sourceMap; static XL6_SlotMapLayoutV1 sourceLayout;
extern bool __real_xrtTypedArrayPush(xtypedarray*,const void*);
extern ptr __real_xrtSlotMapIterNext(xslotmapiter*,xslot*);
bool __wrap_xrtTypedArrayPush(xtypedarray* output,const void* input)
{
    bool ok=__real_xrtTypedArrayPush(output,input);
    if (ok && copyFailure) {
        int selected=copyFailure; copyFailure=0;
        if (selected==1) xrtSetErrorKind(XERR_STATE);
        else if (selected<0) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        return false;
    }
    if (ok && reenter) { reenter=false; assert(xl6SlotMapCountV1(sourceMap,&sourceLayout)==-1); }
    return ok;
}
ptr __wrap_xrtSlotMapIterNext(xslotmapiter* iterator,xslot* slot)
{
    if (iteratorFailure) {
        int selected=iteratorFailure; iteratorFailure=0;
        if (selected==1) xrtSetErrorKind(XERR_STATE);
        else if (selected<0) xl6ErrorScopePropagateFailure((xl6_error_result){false,NULL});
        return NULL;
    }
    return __real_xrtSlotMapIterNext(iterator,slot);
}
static XL6_SlotMapLayoutV1 textLayout(void)
{ return (XL6_SlotMapLayoutV1){0xCA11,{0xCA12,0xCA13,2,sizeof(xstrview),xl6StringTypeV1()}}; }
static xvalue* make(const XL6_SlotMapLayoutV1* layout,unsigned rule)
{
    const XL6_SequenceLayoutV1* v=&layout->value;
    xvalue* witness=v->transfer==5 ? xl6SequenceSlotMapKeyCreateV1(1,v->identity) :
        v->transfer==4 ? xl6SequenceFutureCreateV1(1,v->identity,v->elementIdentity,v->elementIdentity,false) :
        v->transfer==3 ?
        xl6SequenceReferenceCreateV1(1,v->identity,v->elementIdentity,rule,false,v->elementIdentity) :
        xl6SequencePrimitiveCreateV1(1,v->identity,v->elementIdentity,v->transfer,v->size,v->type,rule,false,v->elementIdentity);
    return xl6SlotMapCreateTakeV1(layout,witness);
}
static XL6_Sequence* valuesBegin(const xvalue* value,const XL6_SlotMapLayoutV1* layout)
{
    const XL6_SequenceLayoutV1* v=&layout->value;
    return xl6SequenceBeginRoleV1(value,1,v->identity,v->elementIdentity,v->transfer,v->size,v->type);
}
static XL6_Sequence* keysBegin(const xvalue* value)
{ return xl6SequenceBeginRoleV1(value,1,0xCA14,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,5,sizeof(XL6_SlotMapKeyV1),xl6SlotMapKeyTypeV1()); }
static void dropKey(XL6_SlotMapKeyV1* key) { xrtTypeDropValue(xl6SlotMapKeyTypeV1(),key); }
static void textVectors(void)
{
    XL6_SlotMapLayoutV1 layout=textLayout(); xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    const xstrview first={"a\0你",5},second={"b",1},third={"c",1}; XL6_SlotMapKeyV1 a={0},b={0},c={0}; bool found=false;
    assert(xl6SlotMapInsertV1(map,&layout,&first,&a) && xl6SlotMapInsertV1(map,&layout,&second,&b));
    assert(xl6SlotMapKeyV1(map,&layout,a,true,&found) && found);
    assert(xl6SlotMapInsertV1(map,&layout,&third,&c)); /* a hole reused with a new generation */
    xvalue* keys=xl6SlotMapKeysV1(map,&layout,0xCA14);
    xvalue* values=xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1()); assert(keys && values);
    XL6_Sequence* kp=keysBegin(keys); XL6_Sequence* vp=valuesBegin(values,&layout); assert(kp && vp);
    assert(xrtTypedArrayCount(xl6SequenceStorageV1(kp))==2 && xrtTypedArrayCount(xl6SequenceStorageV1(vp))==2);
    const XL6_SlotMapKeyV1* key=xrtTypedArrayConstGet(xl6SequenceStorageV1(kp),0);
    assert(xl6SlotMapKeyEqualV1(*key,c) && !xl6SlotMapKeyEqualV1(*key,a));
    assert(((const xstrview*)xrtTypedArrayConstGet(xl6SequenceStorageV1(vp),0))->Data!=third.Data);
    xrtValueRelease(map); dropKey(&a); dropKey(&b); dropKey(&c);
    assert(((const xstrview*)xrtTypedArrayConstGet(xl6SequenceStorageV1(vp),1))->Size==1);
    xvalue* box=xrtValueFromTyped(xl6SlotMapKeyTypeV1(),key,xl6SlotMapKeyConverterV1());
    assert(box && xl6SlotMapKeyValueAdmitsV1(box)); xrtValueRelease(box);
    xl6SequenceEndV1(vp); xl6SequenceEndV1(kp); xrtValueRelease(keys); xrtValueRelease(values);

    map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    assert(xl6SlotMapInsertV1(map,&layout,&first,&a));
    values=xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1()); assert(values);
    assert(xl6SlotMapSetV1(map,&layout,a,&second,&found) && found);
    vp=valuesBegin(values,&layout); assert(vp);
    const xstrview* snapshot=xrtTypedArrayConstGet(xl6SequenceStorageV1(vp),0);
    assert(snapshot->Size==5 && !memcmp(snapshot->Data,first.Data,5));
    xl6SequenceEndV1(vp); xrtValueRelease(values);
    xvalue* witness=xl6SequenceSlotMapKeyCreateV1(1,0xCA14); xvalue* alias=xrtValueRetain(witness); assert(alias);
    keys=xl6SlotMapSnapshotTakeV1(map,&layout,true,witness,xl6SlotMapKeyConverterV1()); assert(keys && keys!=alias);
    kp=keysBegin(alias); assert(kp && !xrtTypedArrayCount(xl6SequenceStorageV1(kp))); xl6SequenceEndV1(kp);
    xrtValueRelease(alias); xrtValueRelease(keys);
    dropKey(&a); xrtValueRelease(map);
}
static void refusedAndFailed(void)
{
    XL6_SlotMapLayoutV1 layout=textLayout(); xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map);
    XL6_SlotMapKeyV1 key={0}; xstrview text={"a\0你",5}; assert(xl6SlotMapInsertV1(map,&layout,&text,&key));
    XL6_SlotMapLayoutV1 wrong=layout; ++wrong.value.elementIdentity;
    assert(!xl6SlotMapValuesV1(map,&wrong,xl6StringConverterV1())); xrtClearError();
    xvalueconverter bad=*xl6StringConverterV1(); bad.Context=(ptr)1;
    assert(!xl6SlotMapValuesV1(map,&layout,&bad)); xrtClearError();
    assert(!xl6SlotMapKeysV1(map,&layout,0)); xrtClearError();
    assert(!xl6SlotMapKeysV1(map,&layout,layout.identity)); xrtClearError();
    xvalue* witness=xl6SequenceSlotMapKeyCreateV1(1,0xCA14); assert(witness);
    XL6_SequenceRoleV1 role=0; XL6_SequenceLayoutV1 schema={0}; XL6_SequenceAdmissionV3 policy={0};
    assert(xl6SequenceSchemaCopyV1(witness,&role,&schema,&policy)); xrtValueRelease(witness);
    ++policy.boxedIdentity; witness=xl6SequenceCreateAdmittedV1(role,schema.identity,&policy); assert(witness);
    assert(!xl6SlotMapSnapshotTakeV1(map,&layout,true,witness,xl6SlotMapKeyConverterV1())); xrtClearError();
    witness=xl6SequenceSlotMapKeyCreateV1(1,0xCA14); assert(witness);
    XL6_Sequence* pin=keysBegin(witness); assert(pin && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&key));
    xl6SequenceEndV1(pin); xvalue* alias=xrtValueRetain(witness); assert(alias);
    assert(!xl6SlotMapSnapshotTakeV1(map,&layout,true,witness,xl6SlotMapKeyConverterV1())); xrtClearError();
    pin=keysBegin(alias); assert(pin && xrtTypedArrayCount(xl6SequenceStorageV1(pin))==1);
    xl6SequenceEndV1(pin); xrtValueRelease(alias);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    xvalue* preserved=xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1()); assert(preserved && xrtGetError()==prior);
    xrtClearError(); xrtValueRelease(preserved);
    for (int lane=0;lane<4;++lane) for (int failure=-1;failure<=1;failure+=2) {
        xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
        if (lane<2) copyFailure=failure; else iteratorFailure=failure;
        xvalue* result=lane%2 ? xl6SlotMapKeysV1(map,&layout,0xCA14) :
            xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1());
        assert(!result && !copyFailure && !iteratorFailure);
        xl6_error_result outcome=xl6ErrorScopeEnd(&parent); assert(!outcome.ok); xrtErrorFree(outcome.error);
        assert(xl6SlotMapCountV1(map,&layout)==1);
        result=xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1()); assert(result); xrtValueRelease(result);
    }
    sourceMap=map; sourceLayout=layout; reenter=true;
    assert(!xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1()) && !reenter); xrtClearError();
    assert(xl6SlotMapCountV1(map,&layout)==1);
    iteratorFailure=2;
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent));
    assert(!xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1()) && !iteratorFailure);
    xl6_error_result shortRead=xl6ErrorScopeEnd(&parent); assert(!shortRead.ok); xrtErrorFree(shortRead.error);
    dropKey(&key); xrtValueRelease(map);
}
static void otherDomains(void)
{
    XL6_SlotMapLayoutV1 layout={0xCB11,{0xCB12,0xCB13,1,sizeof(int64_t),xrtTypeInt64()}};
    xvalue* map=make(&layout,XL6_VALUE_ADMIT_I64); assert(map);
    xvalue* empty=xl6SlotMapValuesV1(map,&layout,NULL); assert(empty);
    XL6_Sequence* pin=valuesBegin(empty,&layout); assert(pin && !xrtTypedArrayCount(xl6SequenceStorageV1(pin)));
    xl6SequenceEndV1(pin); xrtValueRelease(empty);
    empty=xl6SlotMapKeysV1(map,&layout,0xCA14); assert(empty);
    pin=keysBegin(empty); assert(pin && !xrtTypedArrayCount(xl6SequenceStorageV1(pin)));
    xl6SequenceEndV1(pin); xrtValueRelease(empty);
    XL6_SlotMapKeyV1 key={0}; assert(xl6SlotMapInsertV1(map,&layout,&(int64_t){42},&key));
    xvalue* snapshot=xl6SlotMapValuesV1(map,&layout,NULL); assert(snapshot);
    dropKey(&key); xrtValueRelease(map);
    pin=valuesBegin(snapshot,&layout); assert(pin && *(const int64_t*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0)==42);
    xl6SequenceEndV1(pin); xrtValueRelease(snapshot);

    layout=(XL6_SlotMapLayoutV1){0xCC11,{0xCC12,0xCC13,3,sizeof(xvalue*),xrtTypeValue()}};
    map=make(&layout,XL6_VALUE_ADMIT_ANY); assert(map); xvalue* shared=xrtValueArray(); assert(shared);
    assert(xl6SlotMapInsertV1(map,&layout,&shared,&key));
    snapshot=xl6SlotMapValuesV1(map,&layout,xl6ArrayReferenceConverterV1()); assert(snapshot);
    dropKey(&key); xrtValueRelease(map); xrtValueRelease(shared);
    pin=valuesBegin(snapshot,&layout); assert(pin);
    xvalue* retained=*(xvalue* const*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0);
    assert(retained==shared && xrtValueArrayAppendNew(retained,xrtValueInt(7)));
    assert(xrtValueCount(retained)==1); xl6SequenceEndV1(pin); xrtValueRelease(snapshot);

    layout=(XL6_SlotMapLayoutV1){0xCD11,{0xCD12,0xCD13,4,sizeof(xfuture*),xrtTypeFuture()}};
    map=make(&layout,0); assert(map); xfuture* future=NULL; xpromise* promise=xrtPromiseCreate(&future,NULL); assert(promise);
    assert(xl6SlotMapInsertV1(map,&layout,&future,&key)); snapshot=xl6SlotMapValuesV1(map,&layout,NULL); assert(snapshot);
    dropKey(&key); xrtValueRelease(map); xrtPromiseDestroy(promise); xrtFutureDestroy(future);
    pin=valuesBegin(snapshot,&layout); assert(pin);
    assert(*(xfuture* const*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0)==future);
    xl6SequenceEndV1(pin); xrtValueRelease(snapshot);

    XL6_SlotMapLayoutV1 source=textLayout(); map=make(&source,XL6_VALUE_ADMIT_STRING); assert(map);
    assert(xl6SlotMapInsertV1(map,&source,&(xstrview){"key",3},&key));
    layout=(XL6_SlotMapLayoutV1){0xCE11,{0xCE12,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID,5,sizeof(XL6_SlotMapKeyV1),xl6SlotMapKeyTypeV1()}};
    xvalue* keyMap=make(&layout,0); assert(keyMap); XL6_SlotMapKeyV1 entry={0};
    assert(xl6SlotMapInsertV1(keyMap,&layout,&key,&entry));
    snapshot=xl6SlotMapValuesV1(keyMap,&layout,xl6SlotMapKeyConverterV1()); assert(snapshot);
    dropKey(&entry); xrtValueRelease(keyMap); xrtValueRelease(map);
    pin=valuesBegin(snapshot,&layout); assert(pin);
    assert(xl6SlotMapKeyEqualV1(*(const XL6_SlotMapKeyV1*)xrtTypedArrayConstGet(xl6SequenceStorageV1(pin),0),key));
    dropKey(&key); xl6SequenceEndV1(pin); xrtValueRelease(snapshot);
}
static bool fault(bool keys,size_t point)
{
    xl6ProductMemoryBegin(SIZE_MAX); XL6_SlotMapLayoutV1 layout=textLayout();
    xvalue* map=make(&layout,XL6_VALUE_ADMIT_STRING); assert(map); XL6_SlotMapKeyV1 key={0};
    xstrview text={"a\0你",5}; assert(xl6SlotMapInsertV1(map,&layout,&text,&key));
    xl6_error_scope parent={0}; assert(xl6ErrorScopeBegin(&parent)); xrtMemDebugFailAfter(point);
    xvalue* result=keys ? xl6SlotMapKeysV1(map,&layout,0xCA14) : xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1());
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear(); xl6_error_result outcome=xl6ErrorScopeEnd(&parent);
    assert(hit ? !result && !outcome.ok : result && outcome.ok);
    if (hit) assert(xrtErrorKind(outcome.error)==XERR_MEMORY);
    assert(xl6SlotMapCountV1(map,&layout)==1);
    xstrview unchanged={0}; bool found=false;
    assert(xl6SlotMapReadV1(map,&layout,key,&unchanged,&found) && found && unchanged.Size==text.Size);
    assert(!memcmp(unchanged.Data,text.Data,text.Size)); xrtFree((ptr)unchanged.Data);
    xvalue* retry=keys ? xl6SlotMapKeysV1(map,&layout,0xCA14) : xl6SlotMapValuesV1(map,&layout,xl6StringConverterV1());
    assert(retry); xrtValueRelease(retry);
    xrtErrorFree(outcome.error); xrtValueRelease(result); dropKey(&key); xrtValueRelease(map);
    xrtClearError(); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); return hit;
}
int main(int argc,char** argv)
{
    assert(argc==2);
    if (!strcmp(argv[1],"vectors")) {
        xl6ProductMemoryBegin(SIZE_MAX); textVectors(); otherDomains(); refusedAndFailed();
        assert(!xrtGetError() && !xl6ProductMemoryEnd()); xl6ProductMemoryVerify(); puts("[PASS] owned SlotMap snapshots");
    } else {
        assert(!strcmp(argv[1],"prefixes"));
        for (unsigned keys=0;keys<2;++keys) {
            size_t at=0; while (at<4096 && fault(keys!=0,at)) ++at; assert(at<4096);
            printf("slot-map-snapshot %s runs=%zu first-no-hit=%zu\n",keys ? "keys" : "values",at+1,at);
        }
    }
    return 0;
}
