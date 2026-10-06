#include "src/runtime/xl6_array_extract.h"
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array_erased.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
static const xrttype* descriptor(unsigned profile)
{
    switch (profile) {
    case 0:return xrtTypeBool(); case 1:return xrtTypeInt64(); case 2:return xrtTypeUInt64();
    case 3:return xrtTypeFloat64(); case 4:return xrtTypeTime(); case 5:return xrtTypeFloat32();
    case 6:return xl6CharTypeV1(); case 7:return xl6StringTypeV1();
    case 8:case 10:return xl6NullableStringTypeV1(); default:return xrtTypeValue();
    }
}
static unsigned rule(unsigned profile)
{
    const unsigned rules[]={XL6_VALUE_ADMIT_BOOL,XL6_VALUE_ADMIT_I64,XL6_VALUE_ADMIT_U64,
        XL6_VALUE_ADMIT_F64,XL6_VALUE_ADMIT_TIME,XL6_VALUE_ADMIT_F32,XL6_VALUE_ADMIT_CHAR,
        XL6_VALUE_ADMIT_STRING,XL6_VALUE_ADMIT_STRING,XL6_VALUE_ADMIT_BYTES,XL6_VALUE_ADMIT_STRING,XL6_VALUE_ADMIT_ANY};
    assert(profile<12); return rules[profile];
}
static unsigned transfer(unsigned profile)
{ return profile==9 || profile==11 ? 3 : profile>=7 ? 2 : 1; }
static xvalue* item(unsigned profile)
{
    switch (profile) {
    case 0:return xrtValueBool(false); case 1:return xrtValueInt(INT64_MIN);
    case 2:return xrtValueUInt(UINT64_MAX); case 3:return xrtValueFloat(-0.0);
    case 4:return xrtValueTime(0); case 5:return xrtValueFloat(1.25);
    case 6:return xrtValueChar(0x4F60); case 7:return xrtValueString((xstrview){"a\0你",5});
    case 8:case 11:return xrtValueRetain(xrtValueNull());
    case 9:return xrtValueBytes((xbytesview){(const unsigned char*)"a\0b",3});
    default:return xrtValueString((xstrview){"",0});
    }
}
static xvalue* prepared(unsigned profile,bool native)
{
    xvalue* good=item(profile); assert(good); xvalue* result;
    if (!native) { result=xrtValueArray(); assert(result && xrtValueArrayAppend(result,good)); }
    else {
        const xrttype* type=descriptor(profile); unsigned mode=transfer(profile);
        result=mode==3 ? xl6SequenceReferenceCreateV1(XL6_SEQUENCE_ARRAY_V1,1001,3001,rule(profile),
            profile==11,3001) : xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,1001,3001,mode,
            type->Size,type,rule(profile),profile==8 || profile==10,3001); assert(result);
        const xvalueconverter* converter=profile==6 ? xl6CharConverterV1() : profile==7 ? xl6StringConverterV1() :
            profile==8 || profile==10 ? xl6NullableStringConverterV1() : NULL;
        xvalue* alias=xl6ArrayEraseV1(result,1001,3001,mode,type->Size,type,converter);
        assert(alias==result); xrtValueRelease(alias); assert(xl6ArrayErasedPushV1(result,good));
    }
    xrtValueRelease(good); return result;
}
static size_t count(const xvalue* value,bool native)
{ return native ? (size_t)xl6ArrayErasedSizeV1(value) : xrtValueCount(value); }
static bool consume(xvalue* value,unsigned profile,bool pop,bool native)
{
    switch (profile) {
    case 0: { bool result=xl6ArrayExtractBoolV1(value,1001,3001,0,pop); if(!xrtGetError()) assert(!result); break; }
    case 1: { int64_t result=xl6ArrayExtractI64V1(value,1001,3001,0,pop); if(!xrtGetError()) assert(result==INT64_MIN); break; }
    case 2: { uint64_t result=xl6ArrayExtractU64V1(value,1001,3001,0,pop); if(!xrtGetError()) assert(result==UINT64_MAX); break; }
    case 3: { double result=xl6ArrayExtractF64V1(value,1001,3001,0,pop); if(!xrtGetError()) assert(result==0 && signbit(result)); break; }
    case 4: { int64_t result=xl6ArrayExtractTimeV1(value,1001,3001,0,pop); if(!xrtGetError()) assert(result==0); break; }
    case 5: { float result=xl6ArrayExtractF32V1(value,1001,3001,0,pop); if(!xrtGetError()) assert(result==1.25f); break; }
    case 6: { uint32_t result=xl6ArrayExtractCharV1(value,1001,3001,0,pop); if(!xrtGetError()) assert(result==0x4F60); break; }
    case 7:case 8:case 10: {
        const char* owned=NULL;
        if(native) {
            const xrttype* type=descriptor(profile);
            XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,1001,3001,2,type->Size,type);
            assert(pin); owned=((const xstrview*)xrtTypedArrayGet(xl6SequenceStorageV1(pin),0))->Data; xl6SequenceEndV1(pin);
        }
        xstrview result=xl6ArrayExtractStringV1(value,1001,3001,0,pop,profile!=7);
        if(!xrtGetError()) {
            if(profile==8) assert(!result.Data && !result.Size);
            else assert(result.Data && result.Size==(profile==7 ? 5u : 0u) &&
                (!result.Size || !memcmp(result.Data,"a\0你",5)));
            if(native) assert(result.Data==owned); /* exact slot move, not another copy */
        }
        xrtFree((void*)result.Data); break;
    }
    default: {
        xvalue* result=xl6ArrayExtractValueV1(value,1001,3001,0,pop);
        if(!xrtGetError()) {
            assert(result);
            if(profile==11) assert(xrtValueType(result)==XVALUE_NULL);
            else { xbytesview raw={0}; assert(xrtValueGetBytes(result,&raw) && raw.Size==3 && !memcmp(raw.Data,"a\0b",3)); }
        }
        xrtValueRelease(result); break;
    }
    }
    return !xrtGetError();
}
static void collection(void)
{
    xvalue* good=xrtValueString((xstrview){"a\0你",5}),*list=xrtValueIntMap(),*dict=xrtValueObject(); assert(good && list && dict);
    assert(xrtValueIntMapSet(list,INT64_MIN,good) && xrtValueObjectSet(dict,(xstrview){"a\0b",3},good));
    xvalue* result=xl6CollectionExtractValueV1(list,XL6_COLLECTION_EXTRACT_LIST_V1,INT64_MIN,(xstrview){0});
    assert(result && xrtValueEqual(result,good) && !xrtValueCount(list)); xrtValueRelease(result);
    result=xl6CollectionExtractValueV1(list,XL6_COLLECTION_EXTRACT_LIST_V1,INT64_MIN,(xstrview){0});
    assert(result && xrtValueType(result)==XVALUE_NULL && !xrtGetError()); xrtValueRelease(result);
    result=xl6CollectionExtractValueV1(dict,XL6_COLLECTION_EXTRACT_DICT_V1,0,(xstrview){"a",1});
    assert(result && xrtValueType(result)==XVALUE_NULL && xrtValueCount(dict)==1 && !xrtGetError()); xrtValueRelease(result);
    result=xl6CollectionExtractValueV1(dict,XL6_COLLECTION_EXTRACT_DICT_V1,0,(xstrview){"a\0b",3});
    assert(result && xrtValueEqual(result,good) && !xrtValueCount(dict)); xrtValueRelease(result);
    assert(!xl6CollectionExtractValueV1(dict,XL6_COLLECTION_EXTRACT_DICT_V1,0,(xstrview){NULL,1})); error(XERR_ARGUMENT);
    assert(!xl6CollectionExtractValueV1(list,XL6_COLLECTION_EXTRACT_DICT_V1,0,(xstrview){0})); error(XERR_TYPE);
    xrtValueRelease(dict); xrtValueRelease(list); xrtValueRelease(good);
}
static void invalidSlot(void)
{
    const xrttype* type=xrtTypeInt64();
    xvalue* value=xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1,1001,3001,1,
        type->Size,type,XL6_VALUE_ADMIT_I8,false,3001); assert(value);
    xvalue* alias=xl6ArrayEraseV1(value,1001,3001,1,type->Size,type,NULL); assert(alias==value); xrtValueRelease(alias);
    XL6_Sequence* pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,1001,3001,1,type->Size,type);
    int64_t invalid=128; assert(pin && xrtTypedArrayPush(xl6SequenceStorageV1(pin),&invalid)); xl6SequenceEndV1(pin);
    assert(!xl6ArrayExtractI64V1(value,1001,3001,0,false)); error(XERR_TYPE); assert(count(value,true)==1); xrtValueRelease(value);
    value=prepared(6,true); type=xl6CharTypeV1();
    pin=xl6SequenceBeginRoleV1(value,XL6_SEQUENCE_ARRAY_V1,1001,3001,1,type->Size,type);
    uint32_t invalidChar=0xD800; assert(pin && xrtTypedArraySet(xl6SequenceStorageV1(pin),0,&invalidChar)); xl6SequenceEndV1(pin);
    assert(!xl6ArrayExtractCharV1(value,1001,3001,0,true)); error(XERR_TYPE); assert(count(value,true)==1); xrtValueRelease(value);
}
static void collectionFault(unsigned profile,const char* point,const xmemdebugsnapshot* before)
{
    bool dict=profile==13; xvalue* good=xrtValueString((xstrview){"a\0你",5});
    xvalue* value=dict ? xrtValueObject() : xrtValueIntMap(); assert(good && value);
    assert(dict ? xrtValueObjectSet(value,(xstrview){"a\0b",3},good) : xrtValueIntMapSet(value,INT64_MIN,good));
    xvalue* snapshot=xrtValueClone(value); assert(snapshot);
    assert(xrtMemDebugFailAfter((size_t)strtoull(point,NULL,10)));
    xvalue* result=xl6CollectionExtractValueV1(value,dict ? XL6_COLLECTION_EXTRACT_DICT_V1 : XL6_COLLECTION_EXTRACT_LIST_V1,
        INT64_MIN,(xstrview){"a\0b",3});
    bool hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
    if(hit) { assert(!result && xrtValueCount(value)==1); error(XERR_MEMORY); }
    else { assert(result && xrtValueEqual(result,good) && !xrtValueCount(value) && !xrtGetError()); xrtValueRelease(result); }
    assert(xrtValueCount(snapshot)==1);
    xrtValueRelease(snapshot); xrtValueRelease(value); xrtValueRelease(good); balanced(before);
    printf("core-extract profile=%u wire take point=%s hit=%u balanced\n",profile,point,hit);
}
int main(int argc,char** argv)
{
    assert(xrtMemDebugEnable(true)); xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    if(argc==5) {
        unsigned profile=(unsigned)strtoul(argv[1],NULL,10); assert(profile<14);
        if(profile>=12) { assert(!strcmp(argv[2],"wire") && !strcmp(argv[3],"take")); collectionFault(profile,argv[4],&before); return 0; }
        bool native=!strcmp(argv[2],"native"),pop=!strcmp(argv[3],"pop");
        assert(pop || !strcmp(argv[3],"take")); xvalue* value=prepared(profile,native);
        xvalue* snapshot=native ? NULL : xrtValueClone(value); assert(native || snapshot);
        assert(xrtMemDebugFailAfter((size_t)strtoull(argv[4],NULL,10)));
        bool ok=consume(value,profile,pop,native),hit=xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        if(hit) { assert(!ok && count(value,native)==1); error(XERR_MEMORY); }
        else assert(ok && !count(value,native));
        if(snapshot) assert(xrtValueCount(snapshot)==1);
        xrtValueRelease(snapshot); xrtValueRelease(value); balanced(&before);
        printf("core-extract profile=%u %s %s point=%s hit=%u balanced\n",profile,argv[2],argv[3],argv[4],hit); return 0;
    }
    assert(argc==1);
    for(unsigned cycle=0;cycle<100;++cycle) {
        for(unsigned profile=0;profile<12;++profile) for(unsigned mode=0;mode<2;++mode) for(unsigned pop=0;pop<2;++pop) {
            xvalue* value=prepared(profile,mode!=0),*alias=xrtValueRetain(value); assert(alias);
            assert(consume(value,profile,pop!=0,mode!=0) && !count(alias,mode!=0));
            xrtValueRelease(alias); xrtValueRelease(value);
        }
        collection();
    }
    invalidSlot();
    xvalue* wrong=xrtValueArray(),*text=xrtValueString((xstrview){"not-an-int",10});
    assert(wrong && text && xrtValueArrayAppend(wrong,text));
    assert(!xl6ArrayExtractI64V1(wrong,1001,3001,0,false)); error(XERR_TYPE); assert(xrtValueCount(wrong)==1);
    xrtSetErrorKind(XERR_ARGUMENT); const xerror* prior=xrtGetError();
    assert(!xl6ArrayExtractI64V1(wrong,1001,3001,0,false) && xrtGetError()==prior); xrtClearError();
    xrtValueRelease(text); xrtValueRelease(wrong);
    balanced(&before); puts("core extraction: 100 native/wire cycles; 12 basic/presence profiles, zero-copy native text, List/Dict misses balanced"); return 0;
}
