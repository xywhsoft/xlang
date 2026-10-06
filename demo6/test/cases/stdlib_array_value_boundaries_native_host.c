#include "src/runtime/xl6_array_string_read.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define IDS UINT64_C(61001),UINT64_C(61002)
#define LAYOUT IDS,2,sizeof(xstrview),xl6StringTypeV1()
static const char original[]="a\0\xE4\xBD\xA0";
static const char replacement[]="b\0c";
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void text(xstrview value,const char* expected,size_t size)
{ assert(value.Data && value.Size==size && !memcmp(value.Data,expected,size)); }
static xvalue* source(bool native)
{
    xvalue* result=native ? xl6ArrayCreateV1(LAYOUT) : xrtValueArray(); assert(result);
    if (native) {
        xstrview value={original,sizeof(original)-1}; assert(xl6ArrayPushV1(result,LAYOUT,&value));
        xvalue* alias=xl6ArrayEraseV1(result,LAYOUT,xl6StringConverterV1());
        assert(alias==result); xrtValueRelease(alias);
    } else assert(xrtValueArrayAppendNew(result,xrtValueString((xstrview){original,sizeof(original)-1})));
    return result;
}
static void checkOriginal(const xvalue* value)
{
    xstrview copied=xl6ArrayReadStringV1(value,IDS,0);
    text(copied,original,sizeof(original)-1); xrtFree((void*)copied.Data);
}
static unsigned destroyed;
static xvalue* reentrySource;
static void drop(void* value,void* context)
{
    (void)value; (void)context; ++destroyed;
    if (reentrySource) {
        assert(!xl6ArrayValueSetTakeV1(reentrySource,0,xrtValueString((xstrview){"reentry",7})));
        error(XERR_STATE);
    }
    xrtSetErrorKind(XERR_IO);
}
static const xvaluehandleops foreign={NULL,drop,NULL,NULL};
static xvalue* rejectedItem(void)
{ void* raw=(void*)(uintptr_t)1; xvalue* value=xrtValueHandleTake(&raw,&foreign,NULL); assert(value && !raw); return value; }
static void vectors(void)
{
    for (unsigned native=0;native<2;++native) {
        xvalue* value=source(native!=0); xvalue* alias=xrtValueRetain(value);
        checkOriginal(value);
        xstrview isolated=xl6ArrayReadStringV1(value,IDS,0);
        ((char*)isolated.Data)[0]='X'; checkOriginal(value); xrtFree((void*)isolated.Data);
        assert(!xl6ArrayReadStringV1(value,IDS,INT64_MIN).Data); error(XERR_RANGE);
        assert(!xl6ArrayReadStringV1(value,IDS,1).Data); error(XERR_RANGE);
        assert(xl6ArrayValueSetTakeV1(alias,-1,xrtValueString((xstrview){replacement,sizeof(replacement)-1})));
        xstrview copied=xl6ArrayReadStringV1(value,IDS,0);
        text(copied,replacement,sizeof(replacement)-1); xrtFree((void*)copied.Data);
        assert(xl6ArrayValueAppendTakeV1(value,xrtValueString((xstrview){"",0})));
        copied=xl6ArrayReadStringV1(value,IDS,-1); text(copied,"",0); xrtFree((void*)copied.Data);
        assert(!xl6ArrayValueSetTakeV1(value,INT64_MIN,rejectedItem())); error(XERR_RANGE);
        if (native) {
            reentrySource=value;
            assert(!xl6ArrayValueAppendTakeV1(value,rejectedItem())); error(XERR_TYPE);
            reentrySource=NULL;
            assert(!xl6ArrayReadStringV1(value,61000,61002,0).Data); error(XERR_TYPE);
            assert(!xl6ArrayReadStringV1(value,61001,61000,0).Data); error(XERR_TYPE);
            XL6_Sequence* pin=xl6SequenceBeginV1(value,LAYOUT); assert(pin);
            assert(!xl6ArrayReadStringV1(value,IDS,0).Data); error(XERR_STATE);
            assert(!xl6ArrayValueSetTakeV1(value,0,rejectedItem())); error(XERR_STATE);
            xl6SequenceEndV1(pin);
        }
        xvalue* key=xrtValueInt(0); assert(key);
        assert(xl6ValueIndexSetTakeV1(value,key,xrtValueString((xstrview){original,sizeof(original)-1})));
        xrtValueRelease(key); checkOriginal(value);
        xrtValueRelease(alias); xrtValueRelease(value);
    }
    xvalue* unbound=xl6ArrayCreateV1(LAYOUT); assert(unbound);
    assert(!xl6ArrayValueAppendTakeV1(unbound,rejectedItem())); error(XERR_STATE); xrtValueRelease(unbound);
    assert(!xl6ArrayValueAppendTakeV1(NULL,rejectedItem())); error(XERR_ARGUMENT);
    assert(!xl6ArrayReadStringV1(NULL,IDS,0).Data); error(XERR_ARGUMENT);
    assert(!xl6ArrayReadStringV1(xrtValueNull(),IDS,0).Data); error(XERR_TYPE);
    /* Same layout and even copied descriptor metadata are not the resident
     * identity. The reader must reject before invoking unknown slot code. */
    xrttype copiedType=*xl6StringTypeV1();
    xvalue* impostor=xl6ArrayCreateV1(IDS,2,sizeof(xstrview),&copiedType); assert(impostor);
    xstrview view={original,sizeof(original)-1};
    assert(xl6ArrayPushV1(impostor,IDS,2,sizeof(xstrview),&copiedType,&view));
    xvalue* impostorAlias=xl6ArrayEraseV1(impostor,IDS,2,sizeof(xstrview),&copiedType,NULL); assert(impostorAlias==impostor);
    assert(!xl6ArrayReadStringV1(impostor,IDS,0).Data); error(XERR_TYPE);
    xrtValueRelease(impostorAlias); xrtValueRelease(impostor);
    xvalue* stack=xl6SequenceWrapAsV1(xrtTypedArrayCreate(xl6StringTypeV1()),XL6_SEQUENCE_STACK_V1,LAYOUT); assert(stack);
    assert(!xl6ArrayReadStringV1(stack,IDS,0).Data); error(XERR_TYPE); xrtValueRelease(stack);
    xvalue* wire=xrtValueArray(); assert(wire);
    assert(xrtValueArrayAppendNew(wire,xrtValueInt(7)));
    assert(!xl6ArrayReadStringV1(wire,IDS,0).Data); error(XERR_TYPE); xrtValueRelease(wire);
    xvalue* object=xrtValueObject(); xvalue* sparse=xrtValueIntMap(); assert(object && sparse);
    xvalue* textKey=xrtValueString((xstrview){"a\0b",3}); xvalue* numberKey=xrtValueInt(INT64_MIN); assert(textKey && numberKey);
    assert(xl6ValueIndexSetTakeV1(object,textKey,xrtValueInt(7)));
    assert(xl6ValueIndexSetTakeV1(sparse,numberKey,xrtValueInt(8)));
    int64_t n=0; assert(xrtValueGetInt(xrtValueObjectGet(object,(xstrview){"a\0b",3}),&n) && n==7);
    assert(xrtValueGetInt(xrtValueIntMapGet(sparse,INT64_MIN),&n) && n==8);
    assert(!xl6ValueIndexSetTakeV1(object,numberKey,rejectedItem())); error(XERR_TYPE);
    xrtValueRelease(numberKey); xrtValueRelease(textKey); xrtValueRelease(sparse); xrtValueRelease(object);
}
static void sweep(const char* operation,size_t point)
{
    bool native=strstr(operation,"native")!=NULL;
    xvalue* value=source(native);
    if (!native && !strncmp(operation,"append-",7)) assert(xrtValueTrim(value));
    xvalue* item=xrtValueString((xstrview){replacement,sizeof(replacement)-1}); assert(item);
    assert(xrtMemDebugFailAfter(point));
    bool ok;
    if (!strncmp(operation,"copy-",5)) {
        xstrview copied=xl6ArrayReadStringV1(value,IDS,-1); ok=copied.Data!=NULL;
        if (ok) { text(copied,original,sizeof(original)-1); xrtFree((void*)copied.Data); }
        xrtValueRelease(item);
    } else if (!strncmp(operation,"set-",4)) ok=xl6ArrayValueSetTakeV1(value,-1,item);
    else { assert(!strncmp(operation,"append-",7)); ok=xl6ArrayValueAppendTakeV1(value,item); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (!ok) { assert(hit); error(XERR_MEMORY); checkOriginal(value); }
    else assert(!xrtGetError());
    xrtValueRelease(value); printf("hit=%u balanced\n",hit ? 1u : 0u);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before,after;
    xrtMemDebugSnapshot(&before);
    if (argc==3) sweep(argv[1],(size_t)strtoull(argv[2],NULL,10));
    else { assert(argc==1); for (unsigned i=0;i<100;++i) vectors(); assert(destroyed==700); }
    xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
    if (argc==1) puts("Array value boundaries: 100 physical cycles balanced");
    return 0;
}
