#include "src/runtime/xl6_nullable_slot_map_key.h"
#include "src/api/xl_foreign_error.h"
#include "test/support/module_product_memory.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static bool armed;
static unsigned flavor;
static size_t point,seen;
extern bool __real_xl6SlotMapKeyUnboxV1(const xvalue*,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyUnboxV1(const xvalue* input,XL6_SlotMapKeyV1* out)
{
    if(!armed || seen++!=point)return __real_xl6SlotMapKeyUnboxV1(input,out);
    if(!flavor)return false;
    bool ok=__real_xl6SlotMapKeyUnboxV1(input,out);
    if(ok && flavor==2){xrtSetErrorKind(XERR_MEMORY);return true;}
    return false;
}
static bool foreignRetain(ptr input,ptr* out,ptr context)
{(void)context;*out=input;return true;}
static void foreignDrop(ptr input,ptr context){(void)input;(void)context;}
static uint64 foreignHash(ptr input,ptr context){(void)input;(void)context;return 7;}
static bool foreignEqual(ptr left,ptr right,ptr context){(void)context;return left==right;}
static xvalue* genuine(void)
{
    XL6_SlotMapKeyV1 key={xl6SlotMapInstanceCreateV1(),7};assert(key.instance);
    xvalue* value=xl6SlotMapKeyBoxV1(key);xl6SlotMapKeyDropValueV1(key);assert(value);return value;
}
static void vectors(void)
{
    static const xvaluehandleops foreign={foreignRetain,foreignDrop,foreignHash,foreignEqual};
    for(unsigned i=0;i<1000;++i){
        xvalue* key=genuine(),*integer=xrtValueInt(42);assert(integer);
        ptr opaque=(ptr)(uintptr_t)1;xvalue* fake=xrtValueHandleTake(&opaque,&foreign,NULL);assert(fake && !opaque);
        assert(xrtValueTypeIdBind(fake,XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID));
        const xvalue* inputs[]={key,integer,xrtValueNull(),fake};
        for(unsigned n=0;n<4;++n){
            XL6_NullableSlotMapKeyV1 out={0};
            assert(xl6NullableSlotMapKeyTryUnboxV1(inputs[n],&out) && out.hasValue==(n==0) && !xrtGetError());
            xl6NullableSlotMapKeyDropValueV1(out);
        }
        XL6_NullableSlotMapKeyV1 out={0};assert(!xl6NullableSlotMapKeyTryUnboxV1(NULL,&out));xrtClearError();
        assert(!xl6NullableSlotMapKeyTryUnboxV1(key,NULL));xrtClearError();
        assert(xl6NullableSlotMapKeyTryUnboxV1(key,&out));XL6_NullableSlotMapKeyV1 prior=out;
        assert(!xl6NullableSlotMapKeyTryUnboxV1(integer,&out) && xl6NullableSlotMapKeyEqualV1(out,prior));xrtClearError();
        xl6NullableSlotMapKeyDropValueV1(out);out=(XL6_NullableSlotMapKeyV1){0};
        xrtSetErrorKind(XERR_STATE);const xerror* inherited=xrtGetError();assert(inherited);
        assert(xl6NullableSlotMapKeyTryUnboxV1(integer,&out) && !out.hasValue && xrtGetError()==inherited);xrtClearError();
        xrtValueRelease(key);xrtValueRelease(integer);xrtValueRelease(fake);
    }
}
static void fault(const char* lane,size_t at)
{
    xvalue* input=genuine();XL6_NullableSlotMapKeyV1 out={0};bool oom=!strcmp(lane,"oom");
    flavor=!strcmp(lane,"partial")?1:!strcmp(lane,"diagnostic")?2:0;point=at;seen=0;
    assert(oom || flavor || !strcmp(lane,"copy"));if(oom)assert(xrtMemDebugFailAfter(at));else armed=true;
    bool ok=xl6NullableSlotMapKeyTryUnboxV1(input,&out);
    bool hit=oom?xrtMemDebugFailTriggered():seen>point;armed=false;xrtMemDebugFailClear();
    assert(ok==!hit);if(hit)assert(!out.hasValue && !out.value.instance && !out.value.slot);
    xrtClearError();xl6NullableSlotMapKeyDropValueV1(out);out=(XL6_NullableSlotMapKeyV1){0};
    assert(xl6NullableSlotMapKeyTryUnboxV1(input,&out) && out.hasValue);xl6NullableSlotMapKeyDropValueV1(out);xrtValueRelease(input);
    printf("lane=%s point=%zu hit=%u balanced\n",lane,at,(unsigned)hit);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1)vectors();else{assert(argc==3);fault(argv[1],(size_t)strtoull(argv[2],NULL,10));}
    assert(!xl6ProductMemoryEnd() && !xrtGetError());xl6ProductMemoryVerify();return 0;
}
