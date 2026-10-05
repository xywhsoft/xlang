#include "src/runtime/xl6_collection_count.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#define LAYOUT UINT64_C(7001),UINT64_C(7002),1,sizeof(int64_t),xrtTypeInt64()

static void error(xerrkind kind)
{
    if(!xrtGetError() || xrtErrorKind(xrtGetError())!=kind)
        fprintf(stderr,"count expected error=%d actual=%d\n",(int)kind,
            xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : -1);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError();
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static xvalue* native(bool bind)
{
    xvalue* source=xl6ArrayCreateV1(LAYOUT); assert(source);
    for(int64_t i=0;i<3;++i) assert(xl6ArrayPushV1(source,LAYOUT,&i));
    if(bind) {
        xvalue* alias=xl6ArrayEraseV1(source,LAYOUT,NULL); assert(alias==source); xrtValueRelease(alias);
    }
    return source;
}
static void valid(xvalue* source,int64_t size)
{
    int64_t result=-9;
    assert(xrtMemDebugFailAfter(0));
    assert(xl6CollectionCountV1(source)==size && xl6CollectionCountIfPresentV1(source,&result) && result==size);
    assert(!xrtMemDebugFailTriggered() && !xrtGetError()); xrtMemDebugFailClear();
    xvalue* boxed=xl6CollectionCountOrNullV1(source); result=-9;
    assert(boxed && xrtValueGetInt(boxed,&result) && result==size); xrtValueRelease(boxed);
}
static void rejected(const xvalue* source,xerrkind kind)
{
    int64_t result=91;
    assert(xl6CollectionCountV1(source)==0); error(kind);
    assert(!xl6CollectionCountIfPresentV1(source,&result) && result==91); error(kind);
    assert(!xl6CollectionCountOrNullV1(source)); error(kind);
}
static void drop(void* value,void* context) { (void)value; (void)context; }
static const xvaluehandleops foreignOps={NULL,drop,NULL,NULL};
static void vectors(void)
{
    xvalue* source=native(true); valid(source,3);
    xvalue* alias=xrtValueRetain(source); valid(alias,3);
    XL6_Sequence* pin=xl6SequenceBeginV1(source,LAYOUT); assert(pin);
    rejected(source,XERR_STATE);
    xl6SequenceEndV1(pin);
    valid(source,3); xrtValueRelease(alias); xrtValueRelease(source);
    source=native(false); rejected(source,XERR_STATE); xrtValueRelease(source);
    source=xl6SequenceWrapAsV1(xrtTypedArrayCreate(xrtTypeInt64()),XL6_SEQUENCE_STACK_V1,LAYOUT);
    assert(source); rejected(source,XERR_TYPE); xrtValueRelease(source);
    void* raw=(void*)(uintptr_t)1;
    source=xrtValueHandleTake(&raw,&foreignOps,NULL); assert(source && !raw);
    assert(xrtValueTypeIdBind(source,7001)); rejected(source,XERR_TYPE); xrtValueRelease(source);
    int64_t result=91;
    assert(!xl6CollectionCountIfPresentV1(NULL,&result) && result==91 && !xrtGetError());
    assert(!xl6CollectionCountIfPresentV1(xrtValueNull(),&result) && result==91 && !xrtGetError());
    assert(!xl6CollectionCountIfPresentV1(NULL,NULL)); error(XERR_ARGUMENT);
    assert(!xl6CollectionCountV1(NULL)); error(XERR_ARGUMENT);
    assert(!xl6CollectionCountV1(xrtValueNull())); error(XERR_TYPE);
    xvalue* absent=xl6CollectionCountOrNullV1(NULL); assert(absent && xrtValueType(absent)==XVALUE_NULL);
    xrtValueRelease(absent); rejected(xrtValueBool(true),XERR_TYPE);
    xvalue* array=xrtValueArray(); assert(array); valid(array,0);
    assert(xrtValueArrayAppendNew(array,xrtValueInt(9))); valid(array,1); xrtValueRelease(array);
    xvalue* list=xrtValueIntMap(); assert(list);
    assert(xrtValueIntMapSetNew(list,31,xrtValueInt(9))); valid(list,1); xrtValueRelease(list);
    xvalue* dict=xrtValueObject(); assert(dict);
    assert(xrtValueObjectSetNew(dict,(xstrview){"key",3},xrtValueInt(9))); valid(dict,1); xrtValueRelease(dict);
    xvalue* set=xrtValueSet(); assert(set);
    assert(xrtValueSetAddNew(set,xrtValueInt(9))); valid(set,1); xrtValueRelease(set);
}
static void fault(bool useNative,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* source=useNative ? native(true) : xrtValueArray(); assert(source);
    if(!useNative) for(unsigned i=0;i<3;++i) assert(xrtValueArrayAppendNew(source,xrtValueInt(i)));
    assert(xrtMemDebugFailAfter(point));
    xvalue* boxed=xl6CollectionCountOrNullV1(source);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if(hit) { assert(!boxed); error(XERR_MEMORY); }
    else { int64_t size; assert(boxed && xrtValueGetInt(boxed,&size) && size==3); }
    xrtValueRelease(boxed); valid(source,3); xrtValueRelease(source); balanced(&before);
    printf("count fault native=%d point=%zu hit=%d balanced\n",(int)useNative,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if(argc==3) { fault(atoi(argv[1])!=0,(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1);
    for(unsigned i=0;i<100;++i) {
        xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); vectors(); balanced(&before);
    }
    puts("collection count: 100 physical cycles balanced; scalar/present count zero allocations");
    return 0;
}
