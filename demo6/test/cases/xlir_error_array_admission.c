/* Native callers cannot forge Error elements by supplying only a TypeId. */
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_error_value.h"
#include <assert.h>
#include <stdio.h>

#define LAYOUT(id) UINT64_C(101),id,3,sizeof(xvalue*),xrtTypeValue()
static void clear(xerrkind kind)
{
    if (!xrtGetError() || xrtErrorKind(xrtGetError())!=kind)
        fprintf(stderr,"expected error %d, got %d\n",(int)kind,xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : -1);
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind);xrtClearError();
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static unsigned drops;
static void foreignDrop(void* data,void* user)
{ (void)user;assert(data==(void*)(uintptr_t)1);++drops; }
static const xvaluehandleops foreignOps={.Drop=foreignDrop};
static void refused(xvalue* array,uint64_t identity,xvalue* wrong)
{
    /* Rejection precedes allocation and never changes the existing element. */
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayPushV1(array,LAYOUT(identity),&wrong));clear(XERR_TYPE);
    assert(!xl6ArraySetV1(array,LAYOUT(identity),0,&wrong));clear(XERR_TYPE);
    assert(!xl6ArrayErasedPushV1(array,wrong));clear(wrong ? XERR_TYPE : XERR_ARGUMENT);
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
    assert(xl6ArraySizeV1(array,LAYOUT(identity),false)==1);
}
static void vectors(const xerror* error,bool nullable)
{
    uint64_t identity=nullable ? 202 : 201;
    xvalue* array=xl6SequenceErrorCreateV1(XL6_SEQUENCE_ARRAY_V1,101,identity,201,nullable);
    xvalue* good=xl6ErrorValueRefV1(error,identity);assert(array && good);
    assert(xl6ArrayPushV1(array,LAYOUT(identity),&good));
    xvalue* alias=xl6ArrayEraseV1(array,LAYOUT(identity),NULL);assert(alias==array);
    xvalue* wrong=xl6ErrorValueRefV1(error,identity+10);assert(wrong);
    refused(array,identity,wrong);xrtValueRelease(wrong);
    void* poison=(void*)(uintptr_t)1;
    wrong=xrtValueHandleTake(&poison,&foreignOps,NULL);assert(wrong && !poison);
    assert(xrtValueTypeIdBind(wrong,identity));refused(array,identity,wrong);
    assert(!drops);xrtValueRelease(wrong);assert(drops==1);drops=0;
    if (!nullable) {
        refused(array,identity,NULL);refused(array,identity,xrtValueNull());
        assert(!xl6ArrayResizeV1(array,LAYOUT(identity),4));clear(XERR_TYPE);
    } else {
        assert(xl6ArrayResizeV1(array,LAYOUT(identity),4));
        xvalue* absent=NULL;
        assert(xl6ArrayReadV1(array,LAYOUT(identity),3,&absent));
        assert(!absent || xrtValueType(absent)==XVALUE_NULL);
        xerror* empty=(xerror*)error;
        assert(xl6ErrorValueReadRefV1(absent ? absent : xrtValueNull(),identity,&empty) && !empty);
        xrtValueRelease(absent);
    }
    xvalue* copy=xl6ArrayCloneV1(array,LAYOUT(identity));assert(copy);
    xrtValueRelease(alias);xrtValueRelease(array);xrtValueRelease(good);
    xvalue* read=NULL;assert(xl6ArrayReadV1(copy,LAYOUT(identity),0,&read));
    xerror* projected=NULL;assert(xl6ErrorValueReadRefV1(read,identity,&projected) && projected==error);
    xrtErrorFree(projected);xrtValueRelease(read);xrtValueRelease(copy);
}
static bool operation(const xerror* error)
{
    xvalue* array=xl6SequenceErrorCreateV1(XL6_SEQUENCE_ARRAY_V1,101,202,201,true);
    xvalue* boxed=array ? xl6ErrorValueRefV1(error,202) : NULL;
    bool ok=array && boxed && xl6ArrayPushV1(array,LAYOUT(202),&boxed) &&
        xl6ArrayResizeV1(array,LAYOUT(202),33);
    xvalue* copy=ok ? xl6ArrayCloneV1(array,LAYOUT(202)) : NULL;
    ok=ok && copy;
    xrtValueRelease(copy);xrtValueRelease(boxed);xrtValueRelease(array);return ok;
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    xerror* error=xrtErrorCreate(XERR_VALUE,"native",1,"error array test");assert(error);
    /* Warm immutable singleton metadata before taking allocation snapshots. */
    assert(operation(error));
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    vectors(error,false);vectors(error,true);balanced(&before);
    assert(!xl6SequenceErrorCreateV1(XL6_SEQUENCE_ARRAY_V1,0,201,201,false));clear(XERR_ARGUMENT);
    assert(!xl6SequenceErrorCreateV1(XL6_SEQUENCE_ARRAY_V1,101,201,201,true));clear(XERR_ARGUMENT);
    assert(!xl6SequenceErrorCreateV1(XL6_SEQUENCE_ARRAY_V1,101,202,201,false));clear(XERR_ARGUMENT);
    unsigned failures=0,successes=0;
    for (size_t point=0;point<64;++point) {
        xrtMemDebugSnapshot(&before);assert(xrtMemDebugFailAfter(point));
        bool ok=operation(error),hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if (hit) {assert(!ok);clear(XERR_MEMORY);++failures;} else {assert(ok);++successes;}
        balanced(&before);
    }
    assert(failures && successes);xrtErrorFree(error);
    puts("error Array admission: provenance, nullable defaults and 64 OOM prefixes balanced");return 0;
}
