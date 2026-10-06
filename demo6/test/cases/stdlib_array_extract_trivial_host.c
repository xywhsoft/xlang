#include "src/runtime/xl6_array_extract.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef union Slots {
    bool b; int8_t i8; uint8_t u8; int16_t i16; uint16_t u16;
    int32_t i32; uint32_t u32; int64_t i64; uint64_t u64; float f; double d;
    XL6_NullableBoolV1 nb; XL6_NullableI64V1 ni; XL6_NullableU64V1 nu;
    XL6_NullableF32V1 nf; XL6_NullableF64V1 nd; XL6_NullableTimeV1 nt;
    XL6_NullableCharV1 nc;
    XL6_NullableNativeI8V1 ni8; XL6_NullableNativeU8V1 nu8;
    XL6_NullableNativeI16V1 ni16; XL6_NullableNativeU16V1 nu16;
    XL6_NullableNativeI32V1 ni32; XL6_NullableNativeU32V1 nu32;
} Slots;
typedef struct Profile {
    const xrttype* base; const xrttype* type; const xvalueconverter* codec;
    unsigned rule; uint64_t identity,element; bool nullable;
} Profile;
static Profile profiles[50]; static unsigned count;
static void pair(const xrttype* base,const xrttype* optional,const xvalueconverter* codec,unsigned rule)
{
    for (unsigned nullable=0;nullable<2;++nullable) {
        assert(count<50); unsigned index=count++;
        profiles[index]=(Profile){base,nullable ? optional : base,nullable ? codec : NULL,
            rule,1000+index,nullable ? 2000+index : 1000+index,nullable!=0};
    }
}
static void init(void)
{
    pair(xrtTypeBool(),xl6NullableBoolTypeV1(),xl6NullableBoolConverterV1(),XL6_VALUE_ADMIT_BOOL);
    for (unsigned rule=XL6_VALUE_ADMIT_I8;rule<=XL6_VALUE_ADMIT_U32;++rule)
        pair(xrtTypeInt64(),xl6NullableI64TypeV1(),xl6NullableI64ConverterV1(),rule);
    pair(xrtTypeUInt64(),xl6NullableU64TypeV1(),xl6NullableU64ConverterV1(),XL6_VALUE_ADMIT_U64);
    pair(xrtTypeFloat32(),xl6NullableF32TypeV1(),xl6NullableF32ConverterV1(),XL6_VALUE_ADMIT_F32);
    pair(xrtTypeFloat64(),xl6NullableF64TypeV1(),xl6NullableF64ConverterV1(),XL6_VALUE_ADMIT_F64);
    pair(xrtTypeTime(),xl6NullableTimeTypeV1(),xl6NullableTimeConverterV1(),XL6_VALUE_ADMIT_TIME);
    pair(xl6CharTypeV1(),xl6NullableCharTypeV1(),xl6NullableCharConverterV1(),XL6_VALUE_ADMIT_CHAR);
    profiles[count-2].codec=xl6CharConverterV1();
#define C_PAIR(Base,Name) pair(xrtType##Base(),xl6Nullable##Name##TypeV1(),xl6Nullable##Name##ConverterV1(),0)
    C_PAIR(Bool,Bool); C_PAIR(Int8,NativeI8); C_PAIR(UInt8,NativeU8);
    C_PAIR(Int16,NativeI16); C_PAIR(UInt16,NativeU16); C_PAIR(Int32,NativeI32);
    C_PAIR(UInt32,NativeU32); C_PAIR(Int64,I64); C_PAIR(UInt64,U64);
    C_PAIR(Float32,F32); C_PAIR(Float64,F64); C_PAIR(Type,U64);
#undef C_PAIR
    assert(count==50);
}
static xvalue* item(Profile p,bool absent)
{
    if (absent) return xrtValueRetain(xrtValueNull());
    xvalue* value=NULL;
    switch (p.base->Kind) {
    case XRT_TYPE_BOOL:value=xrtValueBool(false);break;
    case XRT_TYPE_SIGNED_INT:value=xrtValueInt(7);break;
    case XRT_TYPE_UNSIGNED_INT:case XRT_TYPE_TYPE:value=xrtValueUInt(7);break;
    case XRT_TYPE_FLOAT:value=xrtValueFloat(0.5);break;
    case XRT_TYPE_TIME:value=xrtValueTime(0);break;
    default:assert(p.rule==XL6_VALUE_ADMIT_CHAR);value=xrtValueChar(0x4F60);break;
    }
    assert(value);
    if (p.base->Kind==XRT_TYPE_TYPE) assert(xrtValueTypeIdBind(value,p.identity));
    return value;
}
static void balance(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount && !xrtGetError());
}
static bool attempt(unsigned profile,bool native,bool pop,bool absent,size_t fail)
{
    Profile p=profiles[profile]; xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* good=item(p,absent); xvalue* source; xvalue* snapshot=NULL;
    if (native) {
        source=p.rule ? xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,500,p.element,1,
            p.type->Size,p.type,p.rule,p.nullable,p.identity) : p.nullable ?
            xl6SequenceNullableCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,500,p.element,p.type,p.base,p.identity) :
            xl6SequenceCScalarCreateV1(XL6_SEQUENCE_ARRAY_V1,500,p.element,p.type,p.identity);
        assert(source);
        XL6_Sequence* pin=xl6SequenceBeginRoleV1(source,XL6_SEQUENCE_ARRAY_V1,500,p.element,1,p.type->Size,p.type);
        assert(pin && xrtTypedArrayPushValue(xl6SequenceStorageV1(pin),good,p.codec)); xl6SequenceEndV1(pin);
        xvalue* alias=xl6ArrayEraseV1(source,500,p.element,1,p.type->Size,p.type,p.codec);
        assert(alias==source); xrtValueRelease(alias);
    } else {
        source=xrtValueArray(); assert(source && xrtValueArrayAppend(source,good));
        snapshot=xrtValueClone(source); assert(snapshot && xrtValueCount(snapshot)==1);
    }
    unsigned char output[sizeof(Slots)+2]; memset(output,0xA5,sizeof(output));
    xmemdebugsnapshot pre; xrtMemDebugSnapshot(&pre);
    if (fail!=SIZE_MAX) assert(xrtMemDebugFailAfter(fail));
    bool ok=xl6ArrayExtractTrivialV1(source,500,p.element,0,pop,p.type,p.base,p.codec,p.rule,p.identity,output+1);
    bool hit=fail!=SIZE_MAX && xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    assert(ok ? !xrtGetError() : xrtGetError()!=NULL);
    if (hit) {
        assert(!ok && xrtErrorKind(xrtGetError())==XERR_MEMORY);
        for(size_t i=0;i<sizeof(output);++i) assert(output[i]==0xA5);
        xrtClearError(); assert(!native && xrtValueCount(source)==1);
    } else {
        assert(ok); xmemdebugsnapshot post; xrtMemDebugSnapshot(&post);
        if (native) assert(pre.AllocCount==post.AllocCount);
        Slots result={0}; memcpy(&result,output+1,p.type->Size);
        xvalue* observed=xrtValueFromTyped(p.type,&result,p.codec);
        assert(observed && xrtValueEqual(observed,good)); xrtValueRelease(observed);
        assert(output[0]==0xA5 && output[p.type->Size+1]==0xA5);
        assert(!xl6ArrayExtractTrivialV1(source,500,p.element,0,pop,p.type,p.base,p.codec,p.rule,p.identity,&result));
        assert(xrtErrorKind(xrtGetError())==XERR_RANGE); xrtClearError();
    }
    if (snapshot) { assert(xrtValueCount(snapshot)==1 && xrtValueEqual(xrtValueArrayGet(snapshot,0),good)); xrtValueRelease(snapshot); }
    xrtValueRelease(source); xrtValueRelease(good); balance(before); return hit;
}
static void boundaries(void)
{
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    xvalue* source=xrtValueArray(); xvalue* good=xrtValueInt(128);
    assert(source && good && xrtValueArrayAppend(source,good));
    unsigned char output[sizeof(Slots)+2];
    const xrttype* type=xrtTypeInt64();
    /* Domain, descriptor, argument and range rejection must not alter either
     * source or output. Test a live wire array so errors cannot be hidden by
     * an earlier empty-array check. */
    for (unsigned mode=0;mode<9;++mode) {
        memset(output,0xA5,sizeof(output));
        bool ok=xl6ArrayExtractTrivialV1(source,mode==0 ? 0 : 500,2000,
            mode==1 ? -1 : (mode==2 || mode==3) ? 1 : 0,mode==3,
            mode==4 ? NULL : type,mode==5 ? xrtTypeFloat64() : type,
            NULL,mode==6 ? XL6_VALUE_ADMIT_I8 : mode==7 ? UINT32_MAX : XL6_VALUE_ADMIT_I64,
            mode==8 ? 0 : 1000,output+1);
        assert(!ok && xrtGetError()); xrtClearError();
        for(size_t i=0;i<sizeof(output);++i) assert(output[i]==0xA5);
        assert(xrtValueCount(source)==1 && xrtValueEqual(xrtValueArrayGet(source,0),good));
    }
    memset(output,0xA5,sizeof(output));
    assert(!xl6ArrayExtractTrivialV1(source,500,2000,1,true,type,type,NULL,
        XL6_VALUE_ADMIT_I64,1000,output+1));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    assert(!xl6ArrayExtractTrivialV1(source,500,2000,0,false,type,type,NULL,
        XL6_VALUE_ADMIT_I64,1000,NULL));
    assert(xrtErrorKind(xrtGetError())==XERR_ARGUMENT); xrtClearError();
    /* A preexisting error owns the thread slot, even if new admission fails. */
    xrtSetErrorKind(XERR_STATE); const xerror* prior=xrtGetError();
    assert(!xl6ArrayExtractTrivialV1(source,500,2000,0,false,type,type,NULL,
        XL6_VALUE_ADMIT_I8,1000,output+1));
    assert(xrtGetError()==prior && xrtValueCount(source)==1); xrtClearError();
    for(size_t i=0;i<sizeof(output);++i) assert(output[i]==0xA5);
    xrtValueRelease(good); xrtValueRelease(source); balance(before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true)); init(); boundaries(); unsigned prefixes=0;
    for(unsigned profile=0;profile<count;++profile)
        for(unsigned native=0;native<2;++native)
            for(unsigned pop=0;pop<2;++pop)
                for(unsigned absent=0;absent<=(unsigned)profiles[profile].nullable;++absent) {
                    for(unsigned cycle=0;cycle<100;++cycle) assert(!attempt(profile,native,pop,absent,SIZE_MAX));
                    size_t point=0;
                    while(attempt(profile,native,pop,absent,point)){assert(++point<128);}
                    ++prefixes;
                }
    printf("50 closed trivial profiles, %u complete prefixes, 100 cycles balanced\n",prefixes);
    return 0;
}
