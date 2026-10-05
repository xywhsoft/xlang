#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_iterator.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef union Slot {
    int64_t i64; uint64_t u64;
    XL6_NullableI64V1 ni64; XL6_NullableU64V1 nu64;
    unsigned char raw[32];
} Slot;
typedef struct Spec {
    unsigned rule; bool nullable;
    const xrttype* type; const xvalueconverter* converter;
    uint64_t owner,element,identity;
    int64_t low,high;
} Spec;
#define LAYOUT(s) (s)->owner,(s)->element,1,(s)->type->Size,(s)->type
static Spec spec(unsigned profile)
{
    assert(profile<16);
    Spec s={.rule=XL6_VALUE_ADMIT_I8+profile%8,.nullable=profile>=8,
        .owner=1001+profile,.identity=3001+profile,
        .low=0,.high=INT64_MAX};
    s.element=s.nullable ? 2001+profile : s.identity;
    if (s.rule==XL6_VALUE_ADMIT_U64) {
        s.type=s.nullable ? xl6NullableU64TypeV1() : xrtTypeUInt64();
        s.converter=s.nullable ? xl6NullableU64ConverterV1() : NULL;
    } else {
        s.type=s.nullable ? xl6NullableI64TypeV1() : xrtTypeInt64();
        s.converter=s.nullable ? xl6NullableI64ConverterV1() : NULL;
    }
    switch (s.rule) {
    case XL6_VALUE_ADMIT_I8: s.low=INT8_MIN; s.high=INT8_MAX; break;
    case XL6_VALUE_ADMIT_I16: s.low=INT16_MIN; s.high=INT16_MAX; break;
    case XL6_VALUE_ADMIT_I32: s.low=INT32_MIN; s.high=INT32_MAX; break;
    case XL6_VALUE_ADMIT_I64: s.low=INT64_MIN; break;
    case XL6_VALUE_ADMIT_U8: s.high=UINT8_MAX; break;
    case XL6_VALUE_ADMIT_U16: s.high=UINT16_MAX; break;
    case XL6_VALUE_ADMIT_U32: s.high=UINT32_MAX; break;
    default: assert(s.rule==XL6_VALUE_ADMIT_U64); break;
    }
    return s;
}
static void error(xerrkind kind)
{ assert(xrtGetError() && xrtErrorKind(xrtGetError())==kind); xrtClearError(); }
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(after.LiveCount==before->LiveCount && after.LiveBytes==before->LiveBytes &&
        after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount==before->InvalidFreeCount && after.DoubleFreeCount==before->DoubleFreeCount &&
        after.UseAfterFreeCount==before->UseAfterFreeCount && !xrtGetError());
}
static xvalue* create(const Spec* s)
{ return xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,s->type,s->rule,s->nullable,s->identity); }
static xvalue* encoded(const Spec* s,uint64_t identity,bool high)
{
    xvalue* item=s->rule==XL6_VALUE_ADMIT_U64 ? xrtValueUInt(high ? UINT64_MAX : 0) :
        xrtValueInt(high ? s->high : s->low);
    assert(item);
    if (identity) assert(xrtValueTypeIdBind(item,identity));
    return item;
}
static void observed(const Spec* s,xvalue* owner,int64_t at,const xvalue* expected)
{
    xvalue* item=xl6ArrayErasedReadV1(owner,at); assert(item && xrtValueEqual(item,expected));
    assert(xrtValueTypeId(item)==(xrtValueType(expected)==XVALUE_NULL ? 0 : s->identity));
    xrtValueRelease(item);
}
static void refused(xvalue* owner,const xvalue* wrong)
{
    int64_t count=xl6ArrayErasedSizeV1(owner); assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayErasedPushV1(owner,wrong)); error(XERR_TYPE);
    assert(!xl6ArrayErasedSetV1(owner,0,wrong)); error(XERR_TYPE);
    assert(!xl6ArrayErasedInsertV1(owner,0,wrong)); error(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6ArrayErasedSizeV1(owner)==count);
}
static void vectors(const Spec* s)
{
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,0,s->element,s->type,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,0,s->type,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,NULL,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,s->type,s->rule,s->nullable,0)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,s->type,XL6_VALUE_ADMIT_BOOL,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,s->type,UINT_MAX,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,s->type,s->rule,!s->nullable,s->identity)); error(XERR_ARGUMENT);
    xrttype broken=*s->type; broken.Size+=1;
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,&broken,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    broken=*s->type; broken.Id^=1;
    assert(!xl6SequenceNumericEnumCreateV1(XL6_SEQUENCE_ARRAY_V1,s->owner,s->element,&broken,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    xvalue* owner=create(s),*good=encoded(s,s->identity,false),*high=encoded(s,s->identity,true); assert(owner);
    Slot present={0}; assert(xrtValueToTyped(good,s->type,&present,s->converter));
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&present));
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter); assert(alias==owner);
    observed(s,alias,0,good);
    assert(xl6ArrayErasedPushV1(alias,high)); observed(s,alias,1,high);
    assert(xl6ArrayErasedInsertV1(alias,1,good)); observed(s,alias,1,good);
    assert(xl6ArrayErasedSetV1(alias,1,high)); observed(s,alias,1,high);
    if (s->nullable) {
        Slot absent; memset(&absent,0xA5,sizeof(absent)); absent.raw[0]=0;
        assert(xl6ArrayPushV1(owner,LAYOUT(s),&absent)); observed(s,alias,3,xrtValueNull());
        Slot zero={0}; int equal=1; uint64_t ah,bh;
        assert(xrtTypeCompareValue(s->type,&absent,&zero,&equal) && !equal);
        assert(xrtTypeHashValue(s->type,&absent,&ah) && xrtTypeHashValue(s->type,&zero,&bh) && ah==bh);
        Slot invalid={0}; invalid.raw[0]=2; assert(xrtMemDebugFailAfter(0));
        assert(!xl6ArrayPushV1(owner,LAYOUT(s),&invalid)); error(XERR_TYPE);
        assert(!xl6ArraySetV1(owner,LAYOUT(s),0,&invalid)); error(XERR_TYPE);
        assert(!xl6ArrayInsertV1(owner,LAYOUT(s),0,&invalid)); error(XERR_TYPE);
        assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    } else refused(alias,xrtValueNull());
    for (unsigned i=0;i<2;++i) {
        xvalue* wrong=encoded(s,i ? s->identity+1 : 0,false); refused(alias,wrong); xrtValueRelease(wrong);
    }
    xvalue* wrong=s->rule==XL6_VALUE_ADMIT_U64 ? xrtValueInt(0) : xrtValueUInt(0);
    assert(wrong && xrtValueTypeIdBind(wrong,s->identity)); refused(alias,wrong); xrtValueRelease(wrong);
    if (s->rule!=XL6_VALUE_ADMIT_I64 && s->rule!=XL6_VALUE_ADMIT_U64) {
        int64_t bad[]={s->low-1,s->high+1};
        for (unsigned i=0;i<2;++i) {
            wrong=xrtValueInt(bad[i]); assert(wrong && xrtValueTypeIdBind(wrong,s->identity));
            refused(alias,wrong); xrtValueRelease(wrong);
            Slot invalid={0}; if (s->nullable) invalid.ni64=(XL6_NullableI64V1){true,bad[i]}; else invalid.i64=bad[i];
            assert(xrtMemDebugFailAfter(0));
            assert(!xl6ArrayPushV1(owner,LAYOUT(s),&invalid)); error(XERR_TYPE);
            assert(!xl6ArraySetV1(owner,LAYOUT(s),0,&invalid)); error(XERR_TYPE);
            assert(!xl6ArrayInsertV1(owner,LAYOUT(s),0,&invalid)); error(XERR_TYPE);
            assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        }
    }
    xvalue* clone=xl6ArrayErasedCloneV1(alias),*snapshot=xl6ArrayToValueV1(owner,LAYOUT(s),s->converter);
    xvalue* cursor=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); assert(clone && snapshot && cursor);
    assert(xrtValueTypeId(xrtValueArrayAt(snapshot,0))==s->identity);
    xrtValueRelease(alias); xrtValueRelease(owner);
    observed(s,clone,0,good); assert(xl6ArrayResizeV1(clone,LAYOUT(s),7));
    xvalue* zero=s->nullable ? xrtValueNull() : (s->rule==XL6_VALUE_ADMIT_U64 ? xrtValueUInt(0) : xrtValueInt(0));
    assert(zero); if (!s->nullable) assert(xrtValueTypeIdBind(zero,s->identity));
    for (int64_t i=4;i<7;++i) observed(s,clone,i,zero);
    xrtValueRelease(zero);
    assert(xl6ArrayIteratorAdvanceV1(cursor,999,s->element,1,s->type->Size,s->type));
    Slot got={0}; int equal=1;
    assert(xl6ArrayIteratorReadV1(cursor,999,s->element,1,s->type->Size,s->type,&got));
    assert(xrtTypeCompareValue(s->type,&got,&present,&equal) && !equal);
    xvalue* taken=xl6ArrayErasedTakeV1(clone,0); assert(taken && xrtValueEqual(taken,good) && xrtValueTypeId(taken)==s->identity);
    xrtValueRelease(taken); xrtValueRelease(cursor); xrtValueRelease(snapshot); xrtValueRelease(clone);
    xrtValueRelease(high); xrtValueRelease(good);
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* good=encoded(s,s->identity,false),*owner=NULL,*alias=NULL,*result=NULL;
    if (strcmp(operation,"create")) {
        owner=create(s); assert(owner); alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter);
        assert(alias && xl6ArrayErasedPushV1(alias,good));
    }
    assert(xrtMemDebugFailAfter(point)); bool ok;
    if (!strcmp(operation,"create")) { result=create(s); ok=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6ArrayCloneV1(owner,LAYOUT(s)); ok=result!=NULL; }
    else if (!strcmp(operation,"erased-clone")) { result=xl6ArrayErasedCloneV1(alias); ok=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); ok=result!=NULL; }
    else if (!strcmp(operation,"snapshot")) { result=xl6ArrayToValueV1(owner,LAYOUT(s),s->converter); ok=result!=NULL; }
    else if (!strcmp(operation,"read")) { result=xl6ArrayErasedReadV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"take")) { result=xl6ArrayErasedTakeV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"push")) ok=xl6ArrayErasedPushV1(alias,good);
    else if (!strcmp(operation,"set")) ok=xl6ArrayErasedSetV1(alias,0,good);
    else if (!strcmp(operation,"insert")) ok=xl6ArrayErasedInsertV1(alias,0,good);
    else if (!strcmp(operation,"reserve")) ok=xl6ArrayReserveV1(owner,LAYOUT(s),257);
    else { assert(!strcmp(operation,"resize")); ok=xl6ArrayResizeV1(owner,LAYOUT(s),257); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(!ok); error(XERR_MEMORY);
        if (owner) { assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==1); observed(s,alias,0,good); }
    } else assert(ok && !xrtGetError());
    xrtValueRelease(result); xrtValueRelease(alias); xrtValueRelease(owner); xrtValueRelease(good);
    balanced(&before); printf("operation=%s point=%zu hit=%d balanced\n",operation,point,(int)hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==4) { Spec s=spec((unsigned)strtoul(argv[1],NULL,10)); fault(&s,argv[2],(size_t)strtoull(argv[3],NULL,10)); return 0; }
    assert(argc==1);
    for (unsigned i=0;i<16;++i) { Spec s=spec(i); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); vectors(&s); balanced(&before); }
    puts("16 numeric enum domains, exact identities, integer limits, absent poison, malformed bool and owner lifetimes balanced"); return 0;
}
