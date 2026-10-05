#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_array_value.h"
#include "src/runtime/xl6_array_iterator.h"
#include <assert.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
typedef union Slot {
    XL6_NullableBoolV1 b; XL6_NullableI64V1 i64; XL6_NullableU64V1 u64;
    XL6_NullableF32V1 f32; XL6_NullableF64V1 f64;
    XL6_NullableNativeI8V1 i8; XL6_NullableNativeU8V1 u8;
    XL6_NullableNativeI16V1 i16; XL6_NullableNativeU16V1 u16;
    XL6_NullableNativeI32V1 i32; XL6_NullableNativeU32V1 u32;
    unsigned char raw[32];
} Slot;
typedef struct Spec { const xrttype* base; const xrttype* type; const xvalueconverter* converter; uint64_t owner,element,identity; } Spec;
#define LAYOUT(s) (s)->owner,(s)->element,1,(s)->type->Size,(s)->type
static Spec spec(unsigned profile)
{
    assert(profile<12);
#define SPEC(BASE,NAME) {xrtType##BASE(),xl6Nullable##NAME##TypeV1(),xl6Nullable##NAME##ConverterV1(),0,0,0}
    Spec profiles[]={SPEC(Bool,Bool),SPEC(Int8,NativeI8),SPEC(UInt8,NativeU8),SPEC(Int16,NativeI16),
        SPEC(UInt16,NativeU16),SPEC(Int32,NativeI32),SPEC(UInt32,NativeU32),SPEC(Int64,I64),
        SPEC(UInt64,U64),SPEC(Float32,F32),SPEC(Float64,F64),SPEC(Type,U64)};
#undef SPEC
    Spec s=profiles[profile]; s.owner=1001+profile; s.element=2001+profile; s.identity=3001+profile; return s;
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
{ return xl6ArrayNullableCScalarCreateV1(s->owner,s->element,s->type,s->base,s->identity); }
static xvalue* encoded(const Spec* s,uint64_t identity)
{
    xvalue* value=NULL;
    switch (s->base->Kind) {
    case XRT_TYPE_BOOL: value=xrtValueBool(true); break;
    case XRT_TYPE_SIGNED_INT: value=xrtValueInt(-7); break;
    case XRT_TYPE_UNSIGNED_INT: value=xrtValueUInt(7); break;
    case XRT_TYPE_TYPE: value=xrtValueUInt(UINT64_MAX); break;
    case XRT_TYPE_FLOAT: value=xrtValueFloat(1.5); break;
    default: abort();
    }
    assert(value);
    if (s->base!=xrtTypeBool() && identity) assert(xrtValueTypeIdBind(value,identity));
    return value;
}
static void observed(const Spec* s,xvalue* owner,int64_t at,const xvalue* expected)
{
    xvalue* value=xl6ArrayErasedReadV1(owner,at); assert(value && xrtValueEqual(value,expected));
    assert(xrtValueTypeId(value)==(xrtValueType(expected)==XVALUE_NULL || s->base==xrtTypeBool() ? 0 : s->identity));
    xrtValueRelease(value);
}
static void refused(const Spec* s,xvalue* owner,const xvalue* wrong)
{
    int64_t count=xl6ArrayErasedSizeV1(owner); assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayErasedPushV1(owner,wrong)); error(XERR_TYPE);
    assert(!xl6ArrayErasedSetV1(owner,0,wrong)); error(XERR_TYPE);
    assert(!xl6ArrayErasedInsertV1(owner,0,wrong)); error(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    assert(xl6ArrayErasedSizeV1(owner)==count); (void)s;
}
static void vectors(const Spec* s)
{
    assert(!xl6ArrayNullableCScalarCreateV1(0,s->element,s->type,s->base,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,0,s->type,s->base,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,s->identity,s->type,s->base,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,s->element,s->type,s->base,0)); error(XERR_ARGUMENT);
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,s->element,s->type,NULL,s->identity)); error(XERR_ARGUMENT);
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,s->element,NULL,s->base,s->identity)); error(XERR_ARGUMENT);
    xrttype broken=*s->type; broken.Id^=1;
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,s->element,&broken,s->base,s->identity)); error(XERR_ARGUMENT);
    broken=*s->type; broken.Align+=1;
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,s->element,&broken,s->base,s->identity)); error(XERR_ARGUMENT);
    broken=*s->type; broken.Size+=1;
    assert(!xl6ArrayNullableCScalarCreateV1(s->owner,s->element,&broken,s->base,s->identity)); error(XERR_ARGUMENT);
    xvalue* owner=create(s),*good=encoded(s,s->identity); assert(owner);
    Slot present={0}; assert(xrtValueToTyped(good,s->type,&present,s->converter));
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&present));
    xvalue* alias=xl6ArrayEraseV1(owner,LAYOUT(s),s->converter); assert(alias==owner);
    observed(s,alias,0,good);
    Slot absent; memset(&absent,0xA5,sizeof(absent)); absent.raw[0]=0;
    assert(xl6ArrayPushV1(owner,LAYOUT(s),&absent)); observed(s,alias,1,xrtValueNull());
    Slot zero={0}; int equal=1; uint64_t ah,bh;
    assert(xrtTypeCompareValue(s->type,&absent,&zero,&equal) && !equal);
    assert(xrtTypeHashValue(s->type,&absent,&ah) && xrtTypeHashValue(s->type,&zero,&bh) && ah==bh);
    assert(xl6ArrayErasedSetV1(alias,1,good)); observed(s,alias,1,good);
    assert(xl6ArrayErasedSetV1(alias,1,xrtValueNull())); observed(s,alias,1,xrtValueNull());
    Slot invalid={0}; invalid.raw[0]=2; assert(xrtMemDebugFailAfter(0));
    assert(!xl6ArrayPushV1(owner,LAYOUT(s),&invalid)); error(XERR_TYPE);
    assert(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
    if (s->base==xrtTypeBool()) {
        invalid.raw[0]=1; invalid.raw[1]=2;
        assert(!xl6ArraySetV1(owner,LAYOUT(s),0,&invalid)); error(XERR_TYPE);
    }
    xvalue* wrong=xrtValueString((xstrview){"x",1}); assert(wrong); refused(s,alias,wrong); xrtValueRelease(wrong);
    if (s->base!=xrtTypeBool()) {
        wrong=encoded(s,s->identity+1); refused(s,alias,wrong); xrtValueRelease(wrong);
    }
    if (s->base->Kind==XRT_TYPE_SIGNED_INT && s->base->Size<8) {
        wrong=xrtValueInt(INT64_MAX); refused(s,alias,wrong); xrtValueRelease(wrong);
    }
    if (s->base->Kind==XRT_TYPE_UNSIGNED_INT && s->base->Size<8) {
        wrong=xrtValueUInt(UINT64_MAX); refused(s,alias,wrong); xrtValueRelease(wrong);
    }
    if (s->base==xrtTypeFloat32()) {
        wrong=xrtValueFloat(1.1); refused(s,alias,wrong); xrtValueRelease(wrong);
    }
    if (s->base==xrtTypeType()) {
        wrong=xrtValueUInt(7); refused(s,alias,wrong); xrtValueRelease(wrong);
    }
    xvalue* clone=xl6ArrayErasedCloneV1(alias),*snapshot=xl6ArrayToValueV1(owner,LAYOUT(s),s->converter);
    xvalue* cursor=xl6ArrayIteratorCreateV1(owner,LAYOUT(s),999); assert(clone && snapshot && cursor);
    assert(xrtValueCount(snapshot)==2 && xrtValueEqual(xrtValueArrayAt(snapshot,0),good));
    assert(xrtValueType(xrtValueArrayAt(snapshot,1))==XVALUE_NULL);
    xrtValueRelease(alias); xrtValueRelease(owner);
    observed(s,clone,0,good); assert(xl6ArrayResizeV1(clone,LAYOUT(s),7));
    for (int64_t i=2;i<7;++i) observed(s,clone,i,xrtValueNull());
    assert(xl6ArrayIteratorAdvanceV1(cursor,999,s->element,1,s->type->Size,s->type));
    Slot got={0}; assert(xl6ArrayIteratorReadV1(cursor,999,s->element,1,s->type->Size,s->type,&got));
    assert(xrtTypeCompareValue(s->type,&got,&present,&equal) && !equal);
    xvalue* taken=xl6ArrayErasedTakeV1(clone,0); assert(taken && xrtValueEqual(taken,good));
    xrtValueRelease(taken); xrtValueRelease(cursor); xrtValueRelease(snapshot); xrtValueRelease(clone); xrtValueRelease(good);
}
static void mappings(void)
{
#define MAP(TYPE,BASE,CARRIER) do { \
    struct NativeOptional { bool hasValue; TYPE value; }; \
    _Static_assert(sizeof(struct NativeOptional)==sizeof(CARRIER),"Native optional size"); \
    _Static_assert(_Alignof(struct NativeOptional)==_Alignof(CARRIER),"Native optional align"); \
    _Static_assert(offsetof(struct NativeOptional,value)==offsetof(CARRIER,value),"Native optional offset"); \
    assert(sizeof(TYPE)==xrtType##BASE()->Size); } while (0)
    MAP(signed char,Int8,XL6_NullableNativeI8V1); MAP(unsigned char,UInt8,XL6_NullableNativeU8V1);
    MAP(short,Int16,XL6_NullableNativeI16V1); MAP(unsigned short,UInt16,XL6_NullableNativeU16V1);
    MAP(int,Int32,XL6_NullableNativeI32V1); MAP(unsigned int,UInt32,XL6_NullableNativeU32V1);
    MAP(long long,Int64,XL6_NullableI64V1); MAP(unsigned long long,UInt64,XL6_NullableU64V1);
    MAP(bool,Bool,XL6_NullableBoolV1); MAP(float,Float32,XL6_NullableF32V1); MAP(double,Float64,XL6_NullableF64V1);
    MAP(uint64_t,Type,XL6_NullableU64V1);
#if CHAR_MIN<0
    MAP(char,Int8,XL6_NullableNativeI8V1);
#else
    MAP(char,UInt8,XL6_NullableNativeU8V1);
#endif
#if LONG_MAX==INT32_MAX
    MAP(long,Int32,XL6_NullableNativeI32V1); MAP(unsigned long,UInt32,XL6_NullableNativeU32V1);
#else
    MAP(long,Int64,XL6_NullableI64V1); MAP(unsigned long,UInt64,XL6_NullableU64V1);
#endif
#if SIZE_MAX==UINT32_MAX
    MAP(size_t,UInt32,XL6_NullableNativeU32V1); MAP(ptrdiff_t,Int32,XL6_NullableNativeI32V1);
    MAP(intptr_t,Int32,XL6_NullableNativeI32V1); MAP(uintptr_t,UInt32,XL6_NullableNativeU32V1);
    MAP(ptrdiff_t,Int32,XL6_NullableNativeI32V1); /* c_ssize */
#else
    MAP(size_t,UInt64,XL6_NullableU64V1); MAP(ptrdiff_t,Int64,XL6_NullableI64V1);
    MAP(intptr_t,Int64,XL6_NullableI64V1); MAP(uintptr_t,UInt64,XL6_NullableU64V1);
    MAP(ptrdiff_t,Int64,XL6_NullableI64V1); /* c_ssize */
#endif
#if WCHAR_MAX==UINT16_MAX
    MAP(wchar_t,UInt16,XL6_NullableNativeU16V1);
#else
    MAP(wchar_t,Int32,XL6_NullableNativeI32V1);
#endif
    /* c_ssize uses signed size's target ABI, already checked via ptrdiff_t. */
#undef MAP
}
static void fault(const Spec* s,const char* operation,size_t point)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* good=encoded(s,s->identity),*owner=NULL,*alias=NULL,*result=NULL;
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
    assert(argc==1); mappings();
    for (unsigned i=0;i<12;++i) { Spec s=spec(i); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before); vectors(&s); balanced(&before); }
    puts("21 native nullable C/type mappings and 12 exact domains, absent poison, malformed bool and owner lifetimes balanced"); return 0;
}
