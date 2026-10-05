#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_iterator.h"
#include "src/runtime/xl6_value_admission.h"
#include "src/runtime/xl6_scalar_type_abi.h"
#include "src/runtime/xl6_sequence.h"
#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* One host per physical closure, so symbol checks can prove that unrelated
 * profile factories/codecs are absent, not merely unused at runtime. */
#ifndef XL6_TEST_PRIMITIVE_PROFILE
#define XL6_TEST_PRIMITIVE_PROFILE 0
#endif
#define PROFILE XL6_TEST_PRIMITIVE_PROFILE
typedef union Slot {
    int64_t integer; uint64_t unsignedInteger; double real; float single;
    bool boolean; uint32_t character; xstrview text;
    XL6_NullableI64V1 nullableInteger; XL6_NullableCharV1 nullableCharacter;
    XL6_NullableBoolV1 nullableBool; XL6_NullableF32V1 nullableF32;
    XL6_NullableU64V1 nullableU64; XL6_NullableF64V1 nullableF64;
    XL6_NullableTimeV1 nullableTime;
} Slot;
typedef struct Spec {
    unsigned int rule; bool nullable;
    const xrttype* type; const xvalueconverter* converter;
    uint64_t owner,element,identity; unsigned int transfer;
} Spec;
#define LAYOUT(s) (s)->owner,(s)->element,(s)->transfer,(s)->type->Size,(s)->type
static Spec spec(unsigned int rule)
{
    Spec s={.rule=rule,.nullable=PROFILE>=1 && PROFILE<=7,
        .owner=1001+rule,.element=2001+rule,.identity=3001+rule,.transfer=1};
#if PROFILE==1
    s.type=xl6NullableBoolTypeV1(); s.converter=xl6NullableBoolConverterV1();
#elif PROFILE==2
    s.type=xl6NullableI64TypeV1(); s.converter=xl6NullableI64ConverterV1();
#elif PROFILE==3
    s.type=xl6NullableU64TypeV1(); s.converter=xl6NullableU64ConverterV1();
#elif PROFILE==4
    s.type=xl6NullableF32TypeV1(); s.converter=xl6NullableF32ConverterV1();
#elif PROFILE==5
    s.type=xl6NullableF64TypeV1(); s.converter=xl6NullableF64ConverterV1();
#elif PROFILE==6
    s.type=xl6NullableTimeTypeV1(); s.converter=xl6NullableTimeConverterV1();
#elif PROFILE==7
    s.type=xl6NullableCharTypeV1(); s.converter=xl6NullableCharConverterV1();
#elif PROFILE==8
    s.type=xl6CharTypeV1(); s.converter=xl6CharConverterV1();
#elif PROFILE==9
    s.type=xl6StringTypeV1(); s.converter=xl6StringConverterV1(); s.transfer=2;
#elif PROFILE==10
    s.type=xl6NullableStringTypeV1(); s.converter=xl6NullableStringConverterV1(); s.transfer=2; s.nullable=true;
#else
    if (rule>=XL6_VALUE_ADMIT_I8 && rule<=XL6_VALUE_ADMIT_U32) s.type=xrtTypeInt64();
    else switch (rule) {
    case XL6_VALUE_ADMIT_BOOL: s.type=xrtTypeBool(); break;
    case XL6_VALUE_ADMIT_U64: s.type=xrtTypeUInt64(); break;
    case XL6_VALUE_ADMIT_F32: s.type=xrtTypeFloat32(); break;
    case XL6_VALUE_ADMIT_F64: s.type=xrtTypeFloat64(); break;
    case XL6_VALUE_ADMIT_TIME: s.type=xrtTypeTime(); break;
    default: abort();
    }
#endif
    assert(s.type); return s;
}
static const unsigned int rules[]={
#if PROFILE==0
    XL6_VALUE_ADMIT_BOOL,XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I16,XL6_VALUE_ADMIT_I32,
    XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_U8,XL6_VALUE_ADMIT_U16,XL6_VALUE_ADMIT_U32,
    XL6_VALUE_ADMIT_U64,XL6_VALUE_ADMIT_F32,XL6_VALUE_ADMIT_F64,XL6_VALUE_ADMIT_TIME
#elif PROFILE==1
    XL6_VALUE_ADMIT_BOOL
#elif PROFILE==2
    XL6_VALUE_ADMIT_I8,XL6_VALUE_ADMIT_I16,XL6_VALUE_ADMIT_I32,XL6_VALUE_ADMIT_I64,
    XL6_VALUE_ADMIT_U8,XL6_VALUE_ADMIT_U16,XL6_VALUE_ADMIT_U32
#elif PROFILE==3
    XL6_VALUE_ADMIT_U64
#elif PROFILE==4
    XL6_VALUE_ADMIT_F32
#elif PROFILE==5
    XL6_VALUE_ADMIT_F64
#elif PROFILE==6
    XL6_VALUE_ADMIT_TIME
#elif PROFILE==7 || PROFILE==8
    XL6_VALUE_ADMIT_CHAR
#else
    XL6_VALUE_ADMIT_STRING
#endif
};
static void error(xerrkind kind)
{
    if (!xrtGetError() || xrtErrorKind(xrtGetError())!=kind)
        fprintf(stderr,"profile %d expected %d actual %d\n",PROFILE,(int)kind,
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
static xvalue* create(const Spec* s)
{ return xl6ArrayPrimitiveCreateV2(LAYOUT(s),s->rule,s->nullable,s->identity); }
static xvalue* element(const Spec* s)
{
    if (s->rule>=XL6_VALUE_ADMIT_I8 && s->rule<=XL6_VALUE_ADMIT_U32) return xrtValueInt(65);
    switch (s->rule) {
    case XL6_VALUE_ADMIT_BOOL: return xrtValueBool(true);
    case XL6_VALUE_ADMIT_U64: return xrtValueUInt(UINT64_MAX);
    case XL6_VALUE_ADMIT_F32: return xrtValueFloat((double)(float)0.1);
    case XL6_VALUE_ADMIT_F64: return xrtValueFloat(0.1);
    case XL6_VALUE_ADMIT_TIME: return xrtValueTime(INT64_MIN);
    case XL6_VALUE_ADMIT_CHAR: return xrtValueChar(0x10FFFF);
    case XL6_VALUE_ADMIT_STRING: return xrtValueString((xstrview){"a\0\xe4\xbd\xa0",5});
    default: abort();
    }
}
static void toSlot(const Spec* s,const xvalue* value,Slot* result)
{ assert(xrtValueToTyped(value,s->type,result,s->converter)); }
static uint64_t projectionIdentity(const Spec* s,const xvalue* value)
{
    xvaluetype tag=xrtValueType(value);
    return tag==XVALUE_NULL || tag==XVALUE_BOOL ? 0 : s->identity;
}
static void observed(const Spec* s,const xvalue* value,int64_t index,const xvalue* expected)
{
    xvalue* boxed=xl6ArrayErasedReadV1(value,index);
    if (!boxed || xrtValueTypeId(boxed)!=projectionIdentity(s,expected))
        fprintf(stderr,"projection rule=%u nullable=%u expected=%llu actual=%llu error=%d\n",s->rule,
            (unsigned)s->nullable,(unsigned long long)s->identity,
            (unsigned long long)(boxed ? xrtValueTypeId(boxed) : 0),
            xrtGetError() ? (int)xrtErrorKind(xrtGetError()) : 0);
    assert(boxed && xrtValueTypeId(boxed)==projectionIdentity(s,expected));
    Slot read={0}; xl6ArrayReadV1(value,LAYOUT(s),index,&read); assert(!xrtGetError());
    xvalue* second=xrtValueFromTyped(s->type,&read,s->converter); assert(second && xrtValueEqual(second,expected));
    if (projectionIdentity(s,second)) assert(xrtValueTypeIdBind(second,s->identity));
    assert(xrtValueEqual(boxed,second));
    xrtValueRelease(second); xrtTypeDropValue(s->type,&read); xrtValueRelease(boxed);
}
static void refusal(const Spec* s,const xvalue* owner,const xvalue* bad,const Slot* badSlot)
{
    int64_t count=xl6ArraySizeV1(owner,LAYOUT(s),false),capacity=xl6ArraySizeV1(owner,LAYOUT(s),true);
    assert(xrtMemDebugFailAfter(0));
    if (badSlot) {
        assert(!xl6ArrayPushV1(owner,LAYOUT(s),badSlot)); error(XERR_TYPE);
        assert(!xl6ArraySetV1(owner,LAYOUT(s),0,badSlot)); error(XERR_TYPE);
        assert(!xl6ArrayInsertV1(owner,LAYOUT(s),0,badSlot)); error(XERR_TYPE);
    }
    assert(!xl6ArrayErasedPushV1(owner,bad)); error(XERR_TYPE);
    assert(!xl6ArrayErasedSetV1(owner,0,bad)); error(XERR_TYPE);
    assert(!xl6ArrayErasedInsertV1(owner,0,bad)); error(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==count && xl6ArraySizeV1(owner,LAYOUT(s),true)==capacity);
}
static int64_t outside(unsigned int rule)
{
    switch (rule) {
    case XL6_VALUE_ADMIT_I8: return INT8_MAX+INT64_C(1);
    case XL6_VALUE_ADMIT_I16: return INT16_MAX+INT64_C(1);
    case XL6_VALUE_ADMIT_I32: return INT32_MAX+INT64_C(1);
    case XL6_VALUE_ADMIT_U8: return UINT8_MAX+INT64_C(1);
    case XL6_VALUE_ADMIT_U16: return UINT16_MAX+INT64_C(1);
    case XL6_VALUE_ADMIT_U32: return UINT32_MAX+INT64_C(1);
    default: return 0;
    }
}
static void domainVectors(const Spec* s,xvalue* owner,const xvalue* good)
{
    xvalue* wrong=xrtValueInt(1); assert(wrong);
    if (s->rule>=XL6_VALUE_ADMIT_I8 && s->rule<=XL6_VALUE_ADMIT_U32) {
        xrtValueRelease(wrong); wrong=xrtValueUInt(1);
        if (outside(s->rule)) {
            int64_t number=outside(s->rule);
            xvalue* large=xrtValueInt(number); Slot invalid={0};
            if (s->nullable) invalid.nullableInteger=(XL6_NullableI64V1){true,number}; else invalid.integer=number;
            refusal(s,owner,large,&invalid); xrtValueRelease(large);
        }
        if (s->rule>=XL6_VALUE_ADMIT_U8) {
            xvalue* negative=xrtValueInt(-1); Slot invalid={0};
            if (s->nullable) invalid.nullableInteger=(XL6_NullableI64V1){true,-1}; else invalid.integer=-1;
            refusal(s,owner,negative,&invalid); xrtValueRelease(negative);
        }
    }
    refusal(s,owner,wrong,NULL); xrtValueRelease(wrong);
    if (s->rule==XL6_VALUE_ADMIT_CHAR) {
        Slot invalid={0}; wrong=xrtValueUInt(0xD800);
        if (s->nullable) invalid.nullableCharacter=(XL6_NullableCharV1){true,0xD800}; else invalid.character=0xD800;
        refusal(s,owner,wrong,&invalid); xrtValueRelease(wrong);
        if (s->nullable) invalid.nullableCharacter.value=0x110000; else invalid.character=0x110000;
        wrong=xrtValueUInt(0x110000); refusal(s,owner,wrong,&invalid); xrtValueRelease(wrong);
    }
    if (s->rule==XL6_VALUE_ADMIT_STRING) {
        Slot invalid={.text={NULL,s->nullable ? 1 : 0}};
        wrong=xrtValueInt(0); refusal(s,owner,wrong,&invalid); xrtValueRelease(wrong);
        invalid.text=(xstrview){"",SIZE_MAX}; wrong=xrtValueInt(0);
        refusal(s,owner,wrong,&invalid); xrtValueRelease(wrong);
    }
    if (s->rule==XL6_VALUE_ADMIT_F32) {
        const double invalid[]={0.1,DBL_MAX,-DBL_MAX,DBL_MIN};
        for (size_t i=0;i<sizeof(invalid)/sizeof(*invalid);i++) {
            wrong=xrtValueFloat(invalid[i]); refusal(s,owner,wrong,NULL); xrtValueRelease(wrong);
        }
        const double valid[]={0.0,-0.0,FLT_MAX,-FLT_MAX,(double)FLT_TRUE_MIN,INFINITY,-INFINITY,NAN};
        for (size_t i=0;i<sizeof(valid)/sizeof(*valid);i++) {
            wrong=xrtValueFloat(valid[i]); assert(xl6ArrayErasedPushV1(owner,wrong)); xrtValueRelease(wrong);
        }
    }
    /* An explicitly foreign nominal identity may share a primitive tag, but
     * it must not enter an Array<T> by pointer/ABI coincidence. */
    /* Boolean values are immortal shared XRT singletons, not bindable shells. */
    if (s->rule!=XL6_VALUE_ADMIT_BOOL) {
        xvalue* foreign=element(s); assert(foreign && xrtValueTypeIdBind(foreign,s->identity+1));
        refusal(s,owner,foreign,NULL); xrtValueRelease(foreign);
        assert(xrtValueTypeIdBind((xvalue*)good,s->identity));
    }
    assert(xl6ArrayErasedSetV1(owner,0,good)); observed(s,owner,0,good);
    if (s->nullable) {
        Slot absent={0};
        /* Even a nonscalar garbage payload is ignored when absent. */
        if (s->rule==XL6_VALUE_ADMIT_CHAR) absent.nullableCharacter.value=UINT32_MAX;
        if (s->rule>=XL6_VALUE_ADMIT_I8 && s->rule<=XL6_VALUE_ADMIT_U32) absent.nullableInteger.value=INT64_MIN;
        assert(xl6ArrayPushV1(owner,LAYOUT(s),&absent));
        assert(xl6ArrayErasedPushV1(owner,xrtValueNull()));
    } else refusal(s,owner,xrtValueNull(),NULL);
}
static void defaults(const Spec* s)
{
    xvalue* owner=create(s); assert(owner && xl6ArrayResizeV1(owner,LAYOUT(s),2));
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter); assert(alias==owner);
    xvalue* item=xl6ArrayErasedReadV1(alias,0); assert(item);
    if (s->nullable) assert(xrtValueType(item)==XVALUE_NULL);
    else {
        assert(xl6ValueAdmitsV1(item,s->rule,false,s->identity));
        if (s->rule==XL6_VALUE_ADMIT_STRING) { xstrview text; assert(xrtValueGetString(item,&text) && text.Data && !text.Size); }
        else {
            Slot zero={0}; xvalue* expected=xrtValueFromTyped(s->type,&zero,s->converter);
            assert(expected);
            if (projectionIdentity(s,expected)) assert(xrtValueTypeIdBind(expected,s->identity));
            assert(xrtValueEqual(item,expected)); xrtValueRelease(expected);
        }
    }
    assert(xrtValueTypeId(item)==projectionIdentity(s,item));
    xrtValueRelease(item); assert(xl6ArrayResizeV1(owner,LAYOUT(s),0)); xrtValueRelease(alias); xrtValueRelease(owner);
}
static void vectors(const Spec* s)
{
    xvalue* owner=create(s),*good=element(s); assert(owner && good);
    Slot typed={0}; toSlot(s,good,&typed); assert(xl6ArrayPushV1(owner,LAYOUT(s),&typed));
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter); assert(alias==owner);
    domainVectors(s,alias,good); observed(s,alias,0,good);
    xvalue* clone=xl6ArrayCloneV1(alias,LAYOUT(s)),*erased=xl6ArrayErasedCloneV1(alias);
    assert(clone && erased);
    xvalue* bound=xl6ArrayEraseV1(clone,LAYOUT(s),s->converter); assert(bound==clone); xrtValueRelease(bound);
    xvalue* cursor=xl6ArrayIteratorCreateV1(alias,LAYOUT(s),999); assert(cursor);
    /* No stack-local schema/converter/context can outlive this frame: policy
     * state is immutable resident data copied/refcounted by every owner. */
    assert(xl6ArrayAppendV1(clone,erased,LAYOUT(s)) && xl6ArrayAppendV1(clone,clone,LAYOUT(s)));
    xvalue* unprofiled=xl6ArrayCreateV1(LAYOUT(s)); assert(unprofiled);
    assert(!xl6ArrayAppendV1(clone,unprofiled,LAYOUT(s))); error(XERR_TYPE);
    assert(!xl6ArrayEqualsV1(clone,unprofiled,LAYOUT(s))); error(XERR_TYPE);
    xrtValueRelease(unprofiled);
    /* Same physical tuple, but a different base nominal identity. */
    Spec other=*s; other.identity++;
    unprofiled=create(&other); assert(unprofiled);
    assert(!xl6ArrayAppendV1(clone,unprofiled,LAYOUT(s))); error(XERR_TYPE);
    xrtValueRelease(unprofiled); xrtValueRelease(alias); xrtValueRelease(owner);
    observed(s,clone,0,good); observed(s,erased,0,good);
    xvalue* bad=xrtValueBytes((xbytesview){(const uint8_t*)"a",1});
    assert(bad); refusal(s,clone,bad,NULL); refusal(s,erased,bad,NULL); xrtValueRelease(bad);
    assert(xl6ArrayIteratorAdvanceV1(cursor,999,s->element,s->transfer,s->type->Size,s->type));
    Slot read={0}; assert(xl6ArrayIteratorReadV1(cursor,999,s->element,s->transfer,s->type->Size,s->type,&read));
    xvalue* item=xrtValueFromTyped(s->type,&read,s->converter); assert(item && xrtValueEqual(item,good));
    xrtValueRelease(item); xrtTypeDropValue(s->type,&read); xrtTypeDropValue(s->type,&typed);
    xrtValueRelease(cursor); xrtValueRelease(erased); xrtValueRelease(clone); xrtValueRelease(good);
    defaults(s);
}
static void policyVectors(const Spec* s)
{
    xvalue* owner=create(s),*good=element(s); assert(owner && good);
    Slot typed={0}; toSlot(s,good,&typed);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(owner,XL6_SEQUENCE_ARRAY_V1,LAYOUT(s)); assert(pin);
    /* Successful predicates are transparent to an already pending error;
     * the pure domain predicate's negative result is transparent too. */
    xrtSetErrorKind(XERR_TIMEOUT); const xerror* pending=xrtGetError(); assert(pending);
    assert(xl6SequenceAdmitSlotV1(pin,&typed) && xrtGetError()==pending);
    assert(xl6SequenceAdmitValueV1(pin,good) && xrtGetError()==pending);
    assert(!xl6ValueAdmitsV1(good,XL6_VALUE_ADMIT_BYTES,false,s->identity) && xrtGetError()==pending);
    xl6SequenceEndV1(pin); assert(xrtGetError()==pending);
    xrtValueRelease(owner); assert(xrtGetError()==pending); xrtClearError();
    xrtTypeDropValue(s->type,&typed); xrtValueRelease(good);
    if (s->rule==XL6_VALUE_ADMIT_I8) {
        /* Sharing the same int64 physical carrier/nominal owner tuple must
         * never equate the i8 and i16 admission policies. */
        owner=create(s); Spec wider=*s; wider.rule=XL6_VALUE_ADMIT_I16;
        xvalue* other=create(&wider); assert(owner && other);
        assert(!xl6ArrayAppendV1(owner,other,LAYOUT(s))); error(XERR_TYPE);
        assert(!xl6ArrayEqualsV1(owner,other,LAYOUT(s))); error(XERR_TYPE);
        xrtValueRelease(owner); xrtValueRelease(other);
    }
}
static void malformed(const Spec* s)
{
    assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayPrimitiveCreateV2(0,s->element,s->transfer,s->type->Size,s->type,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(s->owner,0,s->transfer,s->type->Size,s->type,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(LAYOUT(s),s->rule,s->nullable,0)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(LAYOUT(s),XL6_VALUE_ADMIT_NONE,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(LAYOUT(s),XL6_VALUE_ADMIT_ANY,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(LAYOUT(s),XL6_VALUE_ADMIT_BYTES,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(LAYOUT(s),s->rule,!s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(s->owner,s->element,3,s->type->Size,s->type,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayPrimitiveCreateV2(s->owner,s->element,s->transfer,s->type->Size+1,s->type,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    /* Type names/Ids alone are not permission to dispatch slot predicates on
     * an incompatible descriptor. Test size/alignment/ops/flags/ABI changes. */
    xrttype fake=*s->type;
    if (s->nullable || s->rule==XL6_VALUE_ADMIT_CHAR || s->rule==XL6_VALUE_ADMIT_STRING) {
        fake.Align++;
        assert(!xl6ArrayPrimitiveCreateV2(s->owner,s->element,s->transfer,fake.Size,&fake,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
        fake=*s->type; fake.AbiName=(xstrview){"wrong",5};
        assert(!xl6ArrayPrimitiveCreateV2(s->owner,s->element,s->transfer,fake.Size,&fake,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
        fake=*s->type; fake.Ops=NULL;
        assert(!xl6ArrayPrimitiveCreateV2(s->owner,s->element,s->transfer,fake.Size,&fake,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    } else {
        assert(!xl6ArrayPrimitiveCreateV2(s->owner,s->element,s->transfer,fake.Size,&fake,s->rule,s->nullable,s->identity)); error(XERR_ARGUMENT);
    }
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* good=element(s),*owner=NULL,*alias=NULL,*result=NULL; Slot typed={0};
    assert(good); toSlot(s,good,&typed);
    if (strcmp(operation,"create")) {
        owner=create(s); assert(owner && xl6ArrayPushV1(owner,LAYOUT(s),&typed));
        alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter); assert(alias);
    }
    int64_t count=owner ? xl6ArraySizeV1(owner,LAYOUT(s),false) : 0;
    int64_t capacity=owner ? xl6ArraySizeV1(owner,LAYOUT(s),true) : 0;
    xarray beforeStorage={0};
    if (owner) {
        XL6_Sequence* sequence=xl6SequenceBeginV1(owner,LAYOUT(s)); assert(sequence);
        beforeStorage=xl6SequenceStorageV1(sequence)->Storage; xl6SequenceEndV1(sequence);
    }
    assert(xrtMemDebugFailAfter(point)); bool ok=false;
    if (!strcmp(operation,"create")) { result=create(s); ok=result!=NULL; }
    else if (!strcmp(operation,"clone")) { result=xl6ArrayCloneV1(owner,LAYOUT(s)); ok=result!=NULL; }
    else if (!strcmp(operation,"erased-clone")) { result=xl6ArrayErasedCloneV1(alias); ok=result!=NULL; }
    else if (!strcmp(operation,"cursor")) { result=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); ok=result!=NULL; }
    else if (!strcmp(operation,"read")) { result=xl6ArrayErasedReadV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"take")) { result=xl6ArrayErasedTakeV1(alias,0); ok=result!=NULL; }
    else if (!strcmp(operation,"push")) ok=xl6ArrayErasedPushV1(alias,good);
    else if (!strcmp(operation,"set")) ok=xl6ArrayErasedSetV1(alias,0,good);
    else if (!strcmp(operation,"insert")) ok=xl6ArrayErasedInsertV1(alias,0,good);
    else { assert(!strcmp(operation,"resize")); ok=xl6ArrayResizeV1(owner,LAYOUT(s),70); }
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if (hit) {
        assert(!ok); error(XERR_MEMORY);
        if (owner) {
            assert(xl6ArraySizeV1(alias,LAYOUT(s),false)==count);
            assert(xl6ArraySizeV1(alias,LAYOUT(s),true)==capacity);
            XL6_Sequence* sequence=xl6SequenceBeginV1(owner,LAYOUT(s)); assert(sequence);
            xtypedarray* storage=xl6SequenceStorageV1(sequence);
            assert(storage->Storage.Data==beforeStorage.Data && storage->Storage.Allocation==beforeStorage.Allocation);
            xl6SequenceEndV1(sequence);
            observed(s,alias,0,good);
        }
    } else {
        assert(ok && !xrtGetError());
        if (owner) assert(xl6ArraySizeV1(owner,LAYOUT(s),false)==
            (!strcmp(operation,"take") ? 0 : !strcmp(operation,"push") || !strcmp(operation,"insert") ? 2 :
                !strcmp(operation,"resize") ? 70 : count));
    }
    xrtValueRelease(result); xrtValueRelease(alias); xrtValueRelease(owner); xrtValueRelease(good);
    xrtTypeDropValue(s->type,&typed); balanced(&before);
    printf("array-primitive profile=%d %s point=%zu hit=%d balanced\n",PROFILE,operation,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true));
    if (argc==3) { Spec s=spec(rules[0]); fault(&s,argv[1],(size_t)strtoull(argv[2],NULL,10)); return 0; }
    assert(argc==1); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    for (size_t i=0;i<sizeof(rules)/sizeof(*rules);i++) {
        Spec s=spec(rules[i]); vectors(&s); policyVectors(&s); malformed(&s);
    }
    balanced(&before); printf("array-primitive profile=%d vectors balanced\n",PROFILE); return 0;
}
