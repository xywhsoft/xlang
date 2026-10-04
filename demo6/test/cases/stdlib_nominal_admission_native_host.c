#include "src/runtime/xl6_nominal_admission.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const uint64_t classes[]={10,11};
static const uint64_t protocol[]={11};
static const uint64_t exact[]={101};
static const XL6_NominalAdmissionV1 base={XL6_NOMINAL_ADMIT_CLASS_TAG,101,classes,2};
static const XL6_NominalAdmissionV1 named={XL6_NOMINAL_ADMIT_PROTOCOL_TAG,0,protocol,1};
static const XL6_NominalAdmissionV1 native={XL6_NOMINAL_ADMIT_EXACT_ID,0,exact,1};
static xvalue* object(int64_t tag)
{
    xvalue* value=xrtValueObject(); assert(value);
    const xstrview key={"$xlir.class.runtime-type",sizeof("$xlir.class.runtime-type")-1};
    assert(xrtValueObjectSetNew(value,key,xrtValueInt(tag)));
    return value;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount && before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount && before->DoubleFreeCount==after.DoubleFreeCount &&
        before->UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static void probe(xvalue* value,const XL6_NominalAdmissionV1* schema,bool nullable,bool expected)
{
    assert(value); xrtSetErrorKind(XERR_RANGE); const xerror* prior=xrtGetError();
    assert(xrtMemDebugFailAfter(0));
    assert(xl6ValueAdmitsNominalV1(value,schema,nullable)==expected && xrtGetError()==prior);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    xrtValueRelease(value); assert(xrtGetError()==prior); xrtClearError();
}
static void drop(void* handle,void* context)
{ (void)handle; ++*(unsigned int*)context; xrtSetErrorKind(XERR_RANGE); }
static void vectors(void)
{
    probe(object(10),&base,false,true); probe(object(11),&base,false,true);
    probe(object(12),&base,false,false); probe(object(-1),&base,false,false);
    probe(object(10),&named,false,false); probe(object(11),&named,false,true);
    probe(xrtValueObject(),&base,false,false);
    probe(xrtValueInt(11),&base,false,false);
    probe(xrtValueNull(),&base,false,false); probe(xrtValueNull(),&base,true,true);
    probe(xrtValueNull(),&named,true,true);
    xvalue* bytes=xrtValueBytes((xbytesview){(const uint8_t*)"a\0b",3});
    assert(bytes && xrtValueTypeIdBind(bytes,101));
    probe(xrtValueRetain(bytes),&base,false,true);
    probe(xrtValueRetain(bytes),&native,false,true);
    probe(bytes,&named,false,false);
    probe(xrtValueBytes((xbytesview){NULL,0}),&native,false,false);
    XL6_NominalAdmissionV1 empty={XL6_NOMINAL_ADMIT_PROTOCOL_TAG,0,NULL,0};
    probe(object(11),&empty,false,false); probe(xrtValueNull(),&empty,true,true);
    XL6_NominalAdmissionV1 invalid=base; invalid.kind=0;
    probe(object(11),&invalid,false,false);
    invalid=base; invalid.identities=NULL; probe(object(11),&invalid,false,false);
    invalid=named; invalid.nonObjectIdentity=101; probe(object(11),&invalid,false,false);
    assert(!xl6ValueAdmitsNominalV1(NULL,&base,true) && !xl6ValueAdmitsNominalV1(xrtValueNull(),NULL,true));
    for (int tag=XVALUE_ARRAY;tag<=XVALUE_OBJECT;++tag) {
        xvalue* container=tag==XVALUE_ARRAY ? xrtValueArray() : tag==XVALUE_INT_MAP ? xrtValueIntMap() :
            tag==XVALUE_SET ? xrtValueSet() : xrtValueObject();
        assert(container);
        assert(xl6CollectionCheckedNominalTakeV1(container,tag,&empty,false)==container);
        xrtValueRelease(container);
    }
    xvalue* array=xrtValueArray(); assert(array && xrtValueArrayAppendNew(array,object(10)));
    xvalue* alias=xrtValueRetain(array); assert(alias);
    assert(xl6CollectionCheckedNominalTakeV1(array,XVALUE_ARRAY,&base,false)==alias);
    xrtValueRelease(array);
    assert(!xl6CollectionCheckedNominalTakeV1(xrtValueRetain(alias),XVALUE_ARRAY,&named,false));
    assert(xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    assert(xrtValueCount(alias)==1); xrtValueRelease(alias);
    unsigned int drops=0; void* handle=&drops; const xvaluehandleops ops={.Drop=drop};
    xvalue* wrong=xrtValueHandleTake(&handle,&ops,&drops); assert(wrong);
    assert(!xl6CollectionCheckedNominalTakeV1(wrong,XVALUE_ARRAY,&base,false));
    assert(drops==1 && xrtErrorKind(xrtGetError())==XERR_TYPE); xrtClearError();
    array=xrtValueArray(); xvalue* weak=xrtValueWeakRef(array); assert(weak);
    assert(!xl6CollectionCheckedNominalTakeV1(array,XVALUE_ARRAY,NULL,false));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT && xrtValueWeakRefExpired(weak));
    xrtClearError(); xrtValueRelease(weak);
}
static void fault(const char* mode,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* array=xrtValueArray(); assert(array && xrtValueArrayAppendNew(array,object(11)));
    if (!strcmp(mode,"mismatch")) assert(xrtValueArrayAppendNew(array,object(12)));
    else assert(!strcmp(mode,"validate"));
    xvalue* alias=xrtValueRetain(array); assert(alias && xrtMemDebugFailAfter(point));
    xvalue* result=xl6CollectionCheckedNominalTakeV1(array,XVALUE_ARRAY,&named,false);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) assert(!result && xrtErrorKind(xrtGetError())==XERR_MEMORY);
    else if (!strcmp(mode,"mismatch")) assert(!result && xrtErrorKind(xrtGetError())==XERR_TYPE);
    else assert(result==alias && !xrtGetError());
    xrtClearError(); assert(xrtValueCount(alias)==(!strcmp(mode,"mismatch") ? 2u : 1u));
    xrtValueRelease(result); xrtValueRelease(alias); balanced(&before);
    printf("nominal-admission %s point=%zu hit=%d balanced\n",mode,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) { fault(argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    vectors(); balanced(&before); puts("nominal-admission vectors balanced"); return 0;
}
