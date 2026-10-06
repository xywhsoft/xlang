/* The same resident scalar/text profiles used by exact admission. */
#define main xl6PrimitiveReadFixtureMain
#include "test/cases/stdlib_array_primitive_admission_native_host.c"
#undef main
#include "src/runtime/xl6_array_value_read.h"
static xvalue* populated(const Spec* s,const xvalueconverter* converter)
{
    xvalue* owner=create(s),*good=element(s); assert(owner && good);
    Slot slot={0}; toSlot(s,good,&slot);
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&slot));
    if (s->nullable) {
        Slot absent; memset(&absent,0,sizeof(absent)); toSlot(s,xrtValueNull(),&absent);
        assert(xl6ArrayPushV1(owner,LAYOUT(s),&absent)); xrtTypeDropValue(s->type,&absent);
    }
    xrtTypeDropValue(s->type,&slot); xrtValueRelease(good);
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),converter);
    assert(alias==owner); xrtValueRelease(alias); return owner;
}
static void projection(const Spec* s,const xvalue* result)
{
    assert(result && xrtValueTypeId(result)==projectionIdentity(s,result) &&
        xl6ValueAdmitsV1(result,s->rule,s->nullable,s->identity));
    xvalue* expected=element(s); assert(expected);
    if (projectionIdentity(s,expected)) assert(xrtValueTypeIdBind(expected,s->identity));
    assert(xrtValueEqual(result,expected)); xrtValueRelease(expected);
}
#if PROFILE==9
static xvalue* activeOwner;
static const Spec* activeSpec;
static unsigned hookMode,hookCalls;
static xvalue* hookFrom(const xrttype* type,const void* slot,ptr context)
{
    (void)context; ++hookCalls;
    if (hookMode==1) { xrtSetErrorKind(XERR_TYPE); return NULL; }
    if (hookMode==3) return NULL;
    if (hookMode==4) return xrtValueInt(65);
    if (hookMode==2) {
        assert(!xl6ArrayValueReadOrNullCopyV1(activeOwner,99)); error(XERR_STATE);
        assert(!xl6ArrayClearV1(activeOwner,LAYOUT(activeSpec))); error(XERR_STATE);
    }
    return xrtValueFromTyped(type,slot,xl6StringConverterV1());
}
static bool hookTo(const xvalue* value,const xrttype* type,ptr slot,ptr context)
{ (void)context; return xrtValueToTyped(value,type,slot,xl6StringConverterV1()); }
static const xvalueconverter hookConverter={.ToTyped=hookTo,.FromTyped=hookFrom};
#endif
static void readVectors(const Spec* s)
{
    xvalue* owner=populated(s,s->converter),*key=xrtValueInt(0); assert(key);
    xvalue* first=xl6ArrayValueReadCopyV1(owner,0); projection(s,first);
    xvalue* second=xl6ValueIndexReadCopyV1(owner,key); projection(s,second);
    XL6_Sequence* pin=xl6SequenceBeginV1(owner,LAYOUT(s)); assert(pin);
    assert(!xl6ArrayValueReadCopyV1(owner,0)); error(XERR_STATE);
    assert(!xl6ArrayValueReadOrNullCopyV1(owner,99)); error(XERR_STATE);
    assert(!xl6ValueIndexReadCopyV1(owner,key)); error(XERR_STATE); xl6SequenceEndV1(pin);
    assert(!xl6ArrayValueReadCopyV1(owner,INT64_MIN)); error(XERR_RANGE);
    xrtSetErrorKind(XERR_IO); const xerror* ambient=xrtGetError();
    assert(xl6ArrayValueReadOrNullCopyV1(owner,INT64_MIN)==xrtValueNull() && xrtGetError()==ambient);
    assert(xl6ArrayValueReadOrNullCopyV1(NULL,0)==xrtValueNull() && xrtGetError()==ambient);
    xrtClearError();
    if (s->nullable) assert(xl6ArrayValueReadCopyV1(owner,-1)==xrtValueNull());
    assert(xl6ArrayClearV1(owner,LAYOUT(s))); xrtValueRelease(owner);
    projection(s,first); projection(s,second);
    xrtValueRelease(first); xrtValueRelease(second); xrtValueRelease(key);
    owner=create(s); assert(owner);
    assert(!xl6ArrayValueReadOrNullCopyV1(owner,0)); error(XERR_STATE); xrtValueRelease(owner);
#if PROFILE==9
    for(unsigned mode=1;mode<5;++mode) {
        owner=populated(s,&hookConverter); activeOwner=owner; activeSpec=s; hookMode=mode;
        unsigned before=hookCalls; first=xl6ArrayValueReadOrNullCopyV1(owner,0);
        assert(hookCalls==before+1);
        /* XRT normalizes a converter's silent refusal to XERR_TYPE. */
        if (mode!=2) { assert(!first); error(XERR_TYPE); }
        else { projection(s,first); xrtValueRelease(first); }
        hookMode=0; activeOwner=NULL; activeSpec=NULL; xrtValueRelease(owner);
    }
#endif
}
static void wire(void)
{
    xvalue* array=xrtValueArray(),*list=xrtValueIntMap(),*object=xrtValueObject();
    xvalue* item=xrtValueString((xstrview){"a\0b",3}),*key=xrtValueInt(-7);
    xvalue* text=xrtValueString((xstrview){"a\0b",3}); assert(array && list && object && item && key && text);
    assert(xrtValueArrayAppend(array,item) && xrtValueIntMapSet(list,-7,item) &&
        xrtValueObjectSet(object,(xstrview){"a\0b",3},item));
    assert(xrtMemDebugFailAfter(0));
    xvalue* a=xl6ArrayValueReadCopyV1(array,-1),*b=xl6ValueIndexReadCopyV1(list,key),*c=xl6ValueIndexReadCopyV1(object,text);
    assert(a==item && b==item && c==item && !xrtMemDebugFailTriggered());
    assert(xl6ArrayValueReadOrNullCopyV1(array,INT64_MIN)==xrtValueNull() && !xrtGetError());
    assert(xl6ValueIndexReadCopyV1(array,text)==xrtValueNull() && !xrtGetError()); xrtMemDebugFailClear();
    assert(!xl6ArrayValueReadCopyV1(object,0)); error(XERR_TYPE);
    xrtValueRelease(array); xrtValueRelease(list); xrtValueRelease(object); xrtValueRelease(item);
    assert(xrtValueEqual(a,text) && xrtValueEqual(b,text) && xrtValueEqual(c,text));
    xrtValueRelease(a); xrtValueRelease(b); xrtValueRelease(c); xrtValueRelease(text); xrtValueRelease(key);
}
static void readFault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* owner=populated(s,s->converter),*key=xrtValueInt(0); assert(key);
    assert(xrtMemDebugFailAfter(point));
    xvalue* result=!strcmp(operation,"read") ? xl6ArrayValueReadCopyV1(owner,0) :
        !strcmp(operation,"safe") ? xl6ArrayValueReadOrNullCopyV1(owner,0) : xl6ValueIndexReadCopyV1(owner,key);
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) { assert(!result); error(XERR_MEMORY); }
    else { assert(result && !xrtGetError()); projection(s,result); }
    assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==(s->nullable ? 2u : 1u));
    xrtValueRelease(result); xrtValueRelease(key); xrtValueRelease(owner); balanced(&before);
    printf("profile=%d operation=%s point=%zu hit=%d balanced\n",PROFILE,operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xrtSetErrorKind(XERR_MEMORY); xrtClearError();
    if (argc==3) {
        assert(!strcmp(argv[1],"read") || !strcmp(argv[1],"safe") || !strcmp(argv[1],"keyed"));
        for(size_t i=0;i<sizeof(rules)/sizeof(*rules);++i) { Spec s=spec(rules[i]); readFault(&s,argv[1],(size_t)strtoull(argv[2],NULL,10)); }
    } else {
        assert(argc==1);
        for(unsigned cycle=0;cycle<100;++cycle) {
            xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
            for(size_t i=0;i<sizeof(rules)/sizeof(*rules);++i) { Spec s=spec(rules[i]); readVectors(&s); }
            wire(); balanced(&before);
        }
        printf("Owned index read profile=%d: 100 physical cycles balanced\n",PROFILE);
    }
    return 0;
}
