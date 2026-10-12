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
static bool inject(void){return armed && seen++==point;}
extern bool __real_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyCopyValueV1(XL6_SlotMapKeyV1 input,XL6_SlotMapKeyV1* out)
{
    if(!inject())return __real_xl6SlotMapKeyCopyValueV1(input,out);
    if(!flavor)return false;
    bool ok=__real_xl6SlotMapKeyCopyValueV1(input,out);
    if(ok && flavor==2){xrtSetErrorKind(XERR_MEMORY);return true;}
    return false;
}
extern bool __real_xl6SlotMapKeyUnboxV1(const xvalue*,XL6_SlotMapKeyV1*);
bool __wrap_xl6SlotMapKeyUnboxV1(const xvalue* input,XL6_SlotMapKeyV1* out)
{
    if(!inject())return __real_xl6SlotMapKeyUnboxV1(input,out);
    if(!flavor)return false;
    bool ok=__real_xl6SlotMapKeyUnboxV1(input,out);
    if(ok && flavor==2){xrtSetErrorKind(XERR_MEMORY);return true;}
    return false;
}
static XL6_SlotMapKeyV1 key(void)
{
    XL6_SlotMapKeyV1 value={xl6SlotMapInstanceCreateV1(),7};assert(value.instance);return value;
}
static void vectors(void)
{
    const xrttype* type=xl6NullableSlotMapKeyTypeV1();assert(type->Size==sizeof(XL6_NullableSlotMapKeyV1));
    assert(type->Align==_Alignof(XL6_NullableSlotMapKeyV1) && type->Kind==XRT_TYPE_OPTIONAL);
    assert(type->Id==xrtTypeId(XRT_STR_LITERAL("xlang.runtime.nullable-slot-map-key.v1")));
    for(unsigned iteration=0;iteration<1000;++iteration){
        XL6_SlotMapKeyV1 source=key(),different=key(),projected={0};
        XL6_NullableSlotMapKeyV1 none={0},a={0},b={0},c={0};
        assert(xl6NullableSlotMapKeyWrapV1(source,&a) && a.hasValue);
        assert(xl6NullableSlotMapKeyCopyValueV1(a,&b));
        assert(xl6NullableSlotMapKeyEqualV1(a,b) && !xl6NullableSlotMapKeyEqualV1(a,none));
        assert(xl6NullableSlotMapKeyProjectV1(b,&projected) && xl6SlotMapKeyEqualV1(projected,source));
        assert(xl6NullableSlotMapKeyWrapV1(different,&c));
        assert(xrtTypeCopyValue(type,&c,&a) && xl6NullableSlotMapKeyEqualV1(c,a));
        assert(xrtTypeMoveValue(type,&b,&c) && !c.hasValue && !c.value.instance);
        assert(xrtTypeCopyValue(type,&b,&b) && xrtTypeMoveValue(type,&b,&b));
        xvalue* boxed=xl6NullableSlotMapKeyBoxV1(a);assert(boxed);
        assert(xl6NullableSlotMapKeyUnboxV1(boxed,&c) && xl6NullableSlotMapKeyEqualV1(c,a));
        xrtValueRelease(boxed);xrtTypeDropValue(type,&c);
        boxed=xl6NullableSlotMapKeyBoxV1(none);assert(boxed && xrtValueType(boxed)==XVALUE_NULL);
        assert(xl6NullableSlotMapKeyUnboxV1(boxed,&c) && !c.hasValue);xrtValueRelease(boxed);
        // A present empty payload is not absence: presence is never inferred.
        XL6_NullableSlotMapKeyV1 presentEmpty={true,{0}};
        assert(xl6NullableSlotMapKeyCopyValueV1(presentEmpty,&c) && c.hasValue && !c.value.instance);
        assert(!xl6NullableSlotMapKeyEqualV1(presentEmpty,none));xrtTypeDropValue(type,&c);
        boxed=xl6NullableSlotMapKeyBoxV1(presentEmpty);assert(boxed && xrtValueType(boxed)!=XVALUE_NULL);
        assert(xl6NullableSlotMapKeyUnboxV1(boxed,&c) && c.hasValue && !c.value.instance);
        xrtValueRelease(boxed);xrtTypeDropValue(type,&c);
        XL6_NullableSlotMapKeyV1 malformed={false,source};
        assert(!xl6NullableSlotMapKeyCopyValueV1(malformed,&c) && !c.hasValue && !c.value.instance);xrtClearError();
        assert(!xl6NullableSlotMapKeyCopyValueV1(a,&b) && xl6NullableSlotMapKeyEqualV1(a,b));xrtClearError();
        assert(!xl6NullableSlotMapKeyProjectV1(none,&projected));xrtClearError();
        assert(!xl6NullableSlotMapKeyCopyValueV1(a,NULL));xrtClearError();
        assert(!xl6NullableSlotMapKeyUnboxV1(NULL,&c));xrtClearError();
        xvalue* integer=xrtValueInt(1);assert(integer);
        assert(!xl6NullableSlotMapKeyUnboxV1(integer,&c) && !c.hasValue);xrtClearError();xrtValueRelease(integer);
        xrtSetErrorKind(XERR_STATE);const xerror* inherited=xrtGetError();assert(inherited);
        assert(xl6NullableSlotMapKeyCopyValueV1(a,&c) && xrtGetError()==inherited);xrtClearError();xrtTypeDropValue(type,&c);
        // Output initialized before a partial transfer is dropped even before
        // its presence flag was published by the failed acquisition.
        assert(xl6SlotMapKeyCopyValueV1(source,&c.value));xrtTypeDropValue(type,&c);
        assert(!c.hasValue && !c.value.instance && !c.value.slot);
        xl6NullableSlotMapKeyDropValueV1(a);xl6NullableSlotMapKeyDropValueV1(b);xl6NullableSlotMapKeyDropValueV1(c);
        xl6SlotMapKeyDropValueV1(source);xl6SlotMapKeyDropValueV1(different);xl6SlotMapKeyDropValueV1(projected);
    }
}
static void fault(const char* lane,unsigned operation,size_t at)
{
    bool oom=!strcmp(lane,"oom");flavor=!strcmp(lane,"partial")?1:!strcmp(lane,"diagnostic")?2:0;
    assert(oom || flavor || !strcmp(lane,"copy"));assert(operation<6 && (oom || operation!=4));
    XL6_SlotMapKeyV1 source=key(),projected={0};XL6_NullableSlotMapKeyV1 input={0},out={0};
    assert(xl6NullableSlotMapKeyWrapV1(source,&input));const xrttype* type=xl6NullableSlotMapKeyTypeV1();
    XL6_SlotMapKeyV1 prior={0};
    if(operation==5){prior=key();assert(xl6NullableSlotMapKeyWrapV1(prior,&out));}
    xvalue* boxed=NULL;if(operation==3){boxed=xl6NullableSlotMapKeyBoxV1(input);assert(boxed);}
    point=at;seen=0;if(oom)assert(xrtMemDebugFailAfter(point));else armed=true;
    bool ok=false;xvalue* result=NULL;
    if(operation==0)ok=xl6NullableSlotMapKeyCopyValueV1(input,&out);
    else if(operation==1)ok=xrtTypeCopyValue(type,&out,&input);
    else if(operation==2)ok=xl6NullableSlotMapKeyProjectV1(input,&projected);
    else if(operation==3)ok=xl6NullableSlotMapKeyUnboxV1(boxed,&out);
    else if(operation==4){result=xl6NullableSlotMapKeyBoxV1(input);ok=result!=NULL;}
    else ok=xrtTypeCopyValue(type,&out,&input);
    bool hit=oom?xrtMemDebugFailTriggered():seen>point;armed=false;xrtMemDebugFailClear();
    assert(ok==!hit);
    if(hit){
        assert(!projected.instance);
        if(operation==5)assert(out.hasValue && xl6SlotMapKeyEqualV1(out.value,prior));
        else assert(!out.hasValue && !out.value.instance);
    }
    xrtClearError();xl6NullableSlotMapKeyDropValueV1(out);xl6SlotMapKeyDropValueV1(projected);out=(XL6_NullableSlotMapKeyV1){0};
    assert(xl6NullableSlotMapKeyCopyValueV1(input,&out));xl6NullableSlotMapKeyDropValueV1(out);
    xrtValueRelease(boxed);xrtValueRelease(result);xl6NullableSlotMapKeyDropValueV1(input);xl6SlotMapKeyDropValueV1(source);
    xl6SlotMapKeyDropValueV1(prior);
    printf("lane=%s operation=%u point=%zu hit=%u\n",lane,operation,at,(unsigned)hit);
}
int main(int argc,char** argv)
{
    xl6ProductMemoryBegin(SIZE_MAX);
    if(argc==1)vectors();else{assert(argc==4);fault(argv[1],(unsigned)strtoul(argv[2],NULL,10),(size_t)strtoull(argv[3],NULL,10));}
    assert(!xl6ProductMemoryEnd() && !xrtGetError());xl6ProductMemoryVerify();puts("optional native records balanced");return 0;
}
