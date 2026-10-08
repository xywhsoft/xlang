#include "src/runtime/xl6_array_producer.h"
#include "src/runtime/xl6_array_erased.h"
#include "test/support/array_producer_test.h"
#include "test/support/module_product_memory.h"
#include <stdio.h>
#include <stdlib.h>

static void* makeSet(void)
{
    xvalue* patterns=xrtValueArray(); assert(patterns);
    assert(xrtValueArrayAppendNew(patterns,xrtValueString(XRT_STR_LITERAL("你"))));
    assert(xrtValueArrayAppendNew(patterns,xrtValueString(XRT_STR_LITERAL("a\0b"))));
    void* set=xl6RegexSetCreateV1(patterns,0,1048576,4096,4096,-1);
    xrtValueRelease(patterns); assert(set); return set;
}
static xvalue* produce(unsigned mode, void* set)
{
    switch (mode) {
    case 0: return xl6TextSplitV2(XL6_TEST_TEXT_ARRAY_IDS,XRT_STR_LITERAL("你\0|🙂||last|"),XRT_STR_LITERAL("|"),-1,-1);
    case 1: return xl6TextLinesV2(XL6_TEST_TEXT_ARRAY_IDS,XRT_STR_LITERAL("a\0\r\n\r你\n"),-1);
    case 2: return xl6TextFieldsV2(XL6_TEST_TEXT_ARRAY_IDS,XRT_STR_LITERAL(" \t你\0🙂\v\fx "),-1);
    case 3: return xl6TextCharsV2(XL6_TEST_CHAR_ARRAY_IDS,XRT_STR_LITERAL("A\0你🙂"),-1);
    case 4: return xl6RegexSetHitsV2(XL6_TEST_INT_ARRAY_IDS,set,XRT_STR_LITERAL("你a\0b"),0,-1);
    default: abort();
    }
}
static void verify(xvalue* result, unsigned mode)
{
    static const size_t counts[]={5,3,2,4,2}; assert(result && xl6TestArrayCount(result)==counts[mode]);
    XL6_Sequence* pin=NULL; XL6_SequenceLayoutV1 layout; const xvalueconverter* converter=NULL;
    assert(xl6SequenceBeginErasedArrayV1(result,&pin,&layout,&converter)==1);
    assert(layout.identity==UINT64_C(0x1001)+(mode<3 ? 0 : mode-2));
    assert(layout.elementIdentity==UINT64_C(0x2001)+(mode<3 ? 0 : mode-2));
    assert(layout.type==(mode<3 ? xl6StringTypeV1() : mode==3 ? xl6CharTypeV1() : xrtTypeInt64()));
    assert(layout.transfer==(mode<3 ? 2u : 1u));
    XL6_SequenceAdmissionV3 policy={0}; assert(xl6SequenceAdmissionCopyV3(pin,&policy) && policy.ops);
    xl6SequenceAdmissionDropV3(&policy); xl6SequenceEndV1(pin);
    if (mode==0) { xl6TestArrayTextEquals(result,0,XRT_STR_LITERAL("你\0")); xl6TestArrayTextEquals(result,1,XRT_STR_LITERAL("🙂")); }
    if (mode==1) { xl6TestArrayTextEquals(result,0,XRT_STR_LITERAL("a\0")); xl6TestArrayTextEquals(result,1,XRT_STR_LITERAL("")); }
    if (mode==2) xl6TestArrayTextEquals(result,0,XRT_STR_LITERAL("你\0🙂"));
    if (mode==3) assert(xl6TestArrayCharAt(result,1)==0 && xl6TestArrayCharAt(result,2)==0x4f60 && xl6TestArrayCharAt(result,3)==0x1f642);
    if (mode==4) {
        assert(xl6SequenceBeginErasedArrayV1(result,&pin,&layout,&converter)==1);
        assert(*(const int64_t*)xrtTypedArrayGet(xl6SequenceStorageV1(pin),0)==0);
        assert(*(const int64_t*)xrtTypedArrayGet(xl6SequenceStorageV1(pin),1)==1);
        xl6SequenceEndV1(pin);
    }
    assert(!xrtGetError());
}
int main(int argc, char** argv)
{
    assert(argc==1 || argc==3); xl6ProductMemoryBegin(SIZE_MAX);
    void* set=makeSet();
    if (argc==3) {
        unsigned mode=(unsigned)strtoul(argv[1],NULL,10); assert(mode<5);
        assert(xrtMemDebugFailAfter((size_t)strtoull(argv[2],NULL,10)));
        xvalue* result=produce(mode,set); bool hit=xl6ProductMemoryEnd();
        if (hit) assert(!result); else verify(result,mode);
        xrtValueRelease(result); xrtClearError(); xl6RegexSetDestroyV1(set);
        xl6ProductMemoryVerify(); printf("mode=%u hit=%d complete-prefix\n",mode,hit); return 0;
    }
    for (unsigned mode=0;mode<5;++mode) { xvalue* result=produce(mode,set); verify(result,mode); xrtValueRelease(result); }
    XL6_ArrayProducerV1 state; assert(xl6ArrayProducerBeginV1(&state,XL6_TEST_CHAR_ARRAY_IDS,XL6_VALUE_ADMIT_CHAR,xl6CharTypeV1(),xl6CharConverterV1()));
    uint32_t scalar=0xd800; assert(!xl6ArrayProducerPushV1(&state,&scalar));
    assert(!xl6ArrayProducerFinishV1(&state,false)); xrtClearError();
    assert(!xl6ArrayProducerBeginV1(&state,0,UINT64_C(0x2001),XL6_VALUE_ADMIT_STRING,xl6StringTypeV1(),xl6StringConverterV1()));
    assert(!xl6ArrayProducerFinishV1(&state,false)); xrtClearError();
    assert(!xl6ArrayProducerBeginV1(&state,XL6_TEST_CHAR_ARRAY_IDS,XL6_VALUE_ADMIT_ANY,xl6CharTypeV1(),xl6CharConverterV1()));
    assert(!xl6ArrayProducerFinishV1(&state,false)); xrtClearError();
    xl6RegexSetDestroyV1(set); assert(!xl6ProductMemoryEnd()); xl6ProductMemoryVerify();
    puts("native Array producer: exact primitive slots, admission, projection, NUL/Unicode and rollback balanced"); return 0;
}
