#include "test_numeric_arg.h"
#include "src/runtime/xl6_array_default.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void error(xerrkind kind)
{
    assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind);
    xrtClearError();
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static void defaultValue(const xvalue* value,unsigned abi)
{
    bool b=true; int64 i=1; uint64 u=1; uint32 c=1; double f=1; xtime t=1;
    xstrview text={0}; xbytesview data={0};
    switch (abi) {
    case XL6_VALUE_DEFAULT_NULL: assert(xrtValueType(value)==XVALUE_NULL); break;
    case XL6_VALUE_DEFAULT_BOOL: assert(xrtValueGetBool(value,&b) && !b); break;
    case XL6_VALUE_DEFAULT_INT: assert(xrtValueGetInt(value,&i) && !i); break;
    case XL6_VALUE_DEFAULT_UINT: assert(xrtValueGetUInt(value,&u) && !u); break;
    case XL6_VALUE_DEFAULT_FLOAT: assert(xrtValueGetFloat(value,&f) && f==0); break;
    case XL6_VALUE_DEFAULT_CHAR: assert(xrtValueGetChar(value,&c) && !c); break;
    case XL6_VALUE_DEFAULT_STRING: assert(xrtValueGetString(value,&text) && text.Data && !text.Size); break;
    case XL6_VALUE_DEFAULT_BYTES: assert(xrtValueGetBytes(value,&data) && !data.Size); break;
    case XL6_VALUE_DEFAULT_TIME: assert(xrtValueGetTime(value,&t) && !t); break;
    default: assert(0);
    }
}
static void vectors(void)
{
    for (unsigned abi=XL6_VALUE_DEFAULT_NULL;abi<=XL6_VALUE_DEFAULT_TIME;++abi) {
        xvalue* array=xrtValueArray(); xvalue* first=xrtValueInt(77);
        assert(array && first && xrtValueArrayAppendNew(array,first));
        xvalue* alias=xrtValueRetain(array); xvalue* snapshot=xrtValueClone(array);
        assert(alias==array && snapshot && xl6ArrayResizeDefaultV1(array,4,abi));
        assert(xrtValueCount(alias)==4 && xrtValueCount(snapshot)==1 && xrtValueArrayAt(alias,0)==first);
        for (size_t i=1;i<4;++i) defaultValue(xrtValueArrayAt(alias,i),abi);
        assert(xl6ArrayResizeDefaultV1(array,4,abi));
        assert(xl6ArrayResizeDefaultV1(array,1,abi));
        assert(xrtValueArrayAt(alias,0)==first && xrtValueCount(alias)==1);
        xrtValueRelease(snapshot); xrtValueRelease(alias); xrtValueRelease(array);
    }
    xvalue* array=xrtValueArray(); assert(array && xrtValueArrayAppendNew(array,xrtValueObject()));
    size_t capacity=xrtValueCapacity(array); const xvalue* first=xrtValueArrayAt(array,0);
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayResizeDefaultV1(array,2,XL6_VALUE_DEFAULT_FORBIDDEN)); error(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(!xl6ArrayResizeDefaultV1(array,-1,XL6_VALUE_DEFAULT_INT)); error(XERR_RANGE);
    assert(!xl6ArrayResizeDefaultV1(array,2,XL6_VALUE_DEFAULT_INVALID)); error(XERR_ARGUMENT);
    assert(!xl6ArrayResizeDefaultV1(array,2,UINT32_MAX)); error(XERR_ARGUMENT);
    assert(!xl6ArrayResizeDefaultV1(NULL,2,XL6_VALUE_DEFAULT_INT)); error(XERR_ARGUMENT);
    assert(xrtValueCount(array)==1 && xrtValueCapacity(array)==capacity && xrtValueArrayAt(array,0)==first);
    assert(xl6ArrayResizeDefaultV1(array,1,XL6_VALUE_DEFAULT_FORBIDDEN));
    assert(xl6ArrayResizeDefaultV1(array,0,XL6_VALUE_DEFAULT_FORBIDDEN));
    xrtValueRelease(array);
}
static void fault(unsigned abi,int point,bool shrink)
{
    xvalue* array=xrtValueArray(); assert(array && xrtValueReserve(array,16));
    assert(xrtValueArrayAppendNew(array,xrtValueInt(77)));
    xvalue* alias=xrtValueRetain(array); xvalue* snapshot=xrtValueClone(array); assert(snapshot);
    const xvalue* first=xrtValueArrayAt(array,0); size_t capacity=xrtValueCapacity(array);
    assert(xrtMemDebugFailAfter(point));
    bool ok=xl6ArrayResizeDefaultV1(array,shrink ? 0 : 4,abi);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (ok) {
        assert(!xrtGetError() && xrtValueCount(alias)==(shrink ? 0u : 4u));
        if (!shrink) {
            assert(xrtValueArrayAt(alias,0)==first);
            for (size_t i=1;i<4;++i) defaultValue(xrtValueArrayAt(alias,i),abi);
        }
    } else {
        error(XERR_MEMORY);
        assert(xrtValueCount(array)==1 && xrtValueCapacity(array)==capacity && xrtValueArrayAt(array,0)==first);
    }
    assert(xrtValueCount(snapshot)==1 && xrtValueArrayAt(snapshot,0)==first);
    xrtValueRelease(snapshot); xrtValueRelease(alias); xrtValueRelease(array);
    printf("hit=%d balanced\n",hit ? 1 : 0);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    if (argc==1) { vectors(); puts("boxed Array defaults: 9 tags, aliases/snapshots, forbidden/range/ABI refusal"); }
    else { assert(argc==3); unsigned abi=(unsigned)strtoul(argv[1],NULL,10);
        fault(abi ? abi : XL6_VALUE_DEFAULT_INT,xlTestIntArg(argv[2]),!abi); }
    balanced(&before); return 0;
}
