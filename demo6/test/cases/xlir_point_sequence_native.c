#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_extract.h"
#include "src/runtime/xl6_stack.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int first, second;
#define LAYOUT 101,201,1,sizeof(void*),xrtTypePointer()
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static void clearType(void)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError(); }
static void vectors(void)
{
    xvalue* owner=xl6SequenceCScalarCreateV1(1,101,201,xrtTypePointer(),201);assert(owner);
    void* pointer=&first;assert(xl6ArrayPushV1(owner,LAYOUT,&pointer));
    assert(xl6ArrayResizeV1(owner,LAYOUT,3));
    void* output=&second;assert(xl6ArrayReadV1(owner,LAYOUT,1,&output) && !output);
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT,NULL);assert(alias);
    xvalue* item=xl6ArrayErasedReadV1(alias,0);assert(item);
    assert(xrtValueType(item)==XVALUE_POINTER && xrtValueTypeId(item)==201);
    assert(!xrtValueScalarDomainV1(item));
    assert(xrtValueGetPointer(item,&output) && output==&first);
    assert(xl6ArrayErasedPushV1(alias,item));xrtValueRelease(item);
    item=xrtValuePointer(&second);assert(item && xrtValueTypeIdBind(item,202));
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayErasedSetV1(alias,0,item));clearType();
    assert(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();xrtValueRelease(item);
    item=xrtValueUInt(0);assert(item);
    assert(!xl6ArrayErasedPushV1(alias,item));clearType();xrtValueRelease(item);
    assert(!xl6ArrayErasedPushV1(alias,xrtValueNull()));clearType();
    assert(xl6ArrayExtractTrivialV1(alias,101,201,0,false,xrtTypePointer(),xrtTypePointer(),NULL,
        XL6_VALUE_ADMIT_NONE,201,&output) && output==&first);
    xrtValueRelease(alias);xrtValueRelease(owner);

    owner=xl6SequenceCScalarCreateV1(2,101,201,xrtTypePointer(),201);assert(owner);
    assert(xl6StackPushV1(owner,LAYOUT,&pointer));output=NULL;
    assert(xl6StackTakeV1(owner,LAYOUT,0,&output,true) && output==&first);xrtValueRelease(owner);
    /* Optional pointer storage shares the native pointer-sentinel carrier.
     * Erased writes still require a pointer box, including pointer(NULL). */
    owner=xl6SequenceCScalarCreateV1(1,101,201,xrtTypePointer(),200);assert(owner);
    alias=xl6ArrayEraseV1(owner,LAYOUT,NULL);assert(alias);
    assert(!xl6ArrayErasedPushV1(alias,xrtValueNull()));clearType();
    item=xrtValuePointer(NULL);assert(item && xl6ArrayErasedPushV1(alias,item));xrtValueRelease(item);output=&second;
    assert(xl6ArrayReadV1(owner,LAYOUT,0,&output) && !output);
    xrtValueRelease(alias);xrtValueRelease(owner);
    assert(!xl6SequenceCScalarCreateV1(1,101,201,xrtTypeInt64(),200));
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
}
static int fault(size_t point)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    assert(xrtMemDebugFailAfter(point));
    void* pointer=&first;xvalue* owner=xl6SequenceCScalarCreateV1(1,101,201,xrtTypePointer(),201);
    xvalue* alias=NULL; xvalue* boxed=NULL; xvalue* copy=NULL;
    bool ok=owner && xl6ArrayPushV1(owner,LAYOUT,&pointer) && xl6ArrayResizeV1(owner,LAYOUT,64);
    if(ok){alias=xl6ArrayEraseV1(owner,LAYOUT,NULL);ok=alias!=NULL;}
    if(ok){boxed=xl6ArrayErasedReadV1(alias,0);ok=boxed!=NULL;}
    if(ok)ok=xl6ArrayErasedPushV1(alias,boxed);
    if(ok){copy=xl6ArrayCloneV1(owner,LAYOUT);ok=copy!=NULL;}
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    assert(hit ? !ok : ok);
    if(hit){assert(xrtGetError() && xrtErrorKind(xrtGetError())==XERR_MEMORY);xrtClearError();}
    xrtValueRelease(copy);xrtValueRelease(boxed);xrtValueRelease(alias);xrtValueRelease(owner);balanced(&before);
    printf("point Sequence fault=%zu hit=%u balanced\n",point,(unsigned)hit);return hit?0:2;
}
int main(int argc,char** argv)
{
    if(argc==2)return fault((size_t)strtoull(argv[1],NULL,10));
    assert(argc==1);xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);vectors();balanced(&before);
    puts("point Sequence native admission, type identities, null sentinel, rejection-before-allocation and actual addresses balanced");return 0;
}
