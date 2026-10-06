/* The functions and TypeIds are extracted from the real verified language IR.
 * This host supplies native owners, not copied algorithm bodies. */
#include "src/runtime/xl6_array_admission.h"
#include "src/runtime/xl6_array.h"
#include "src/runtime/xl6_array_erased.h"
#include "src/runtime/xl6_string_type.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define INTS XL6_METHOD_I64_OWNER,XL6_METHOD_I64_ELEMENT,1,sizeof(int64_t),xrtTypeInt64()
#define TEXTS XL6_METHOD_STRING_OWNER,XL6_METHOD_STRING_ELEMENT,2,sizeof(xstrview),xl6StringTypeV1()

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after; xrtMemDebugSnapshot(&after);
    assert(before->LiveCount == after.LiveCount && before->LiveBytes == after.LiveBytes &&
        after.AllocCount-before->AllocCount == after.FreeCount-before->FreeCount &&
        after.InvalidFreeCount == before->InvalidFreeCount && after.DoubleFreeCount == before->DoubleFreeCount &&
        after.UseAfterFreeCount == before->UseAfterFreeCount && !xrtGetError());
}

int main(int argc, char** argv)
{
    assert(argc == 1 || argc == 3);
    assert(xrtMemDebugEnable(true)); assert(xlir_module_init());
    xmemdebugsnapshot before; xrtMemDebugSnapshot(&before);
    if (argc == 3) {
        bool text = !strcmp(argv[1], "text");
        assert(text || !strcmp(argv[1], "int"));
        xvalue* source = text ? xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1, TEXTS,
            XL6_VALUE_ADMIT_STRING, false, XL6_METHOD_STRING_ELEMENT) :
            xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1, INTS,
                XL6_VALUE_ADMIT_I64, false, XL6_METHOD_I64_ELEMENT); assert(source);
        int64_t one = 1, two = 2;
        xstrview nul = {"a\0你", 5}, last = {"z", 1};
        if (text) assert(xl6ArrayPushV1(source, TEXTS, &nul) && xl6ArrayPushV1(source, TEXTS, &last));
        else assert(xl6ArrayPushV1(source, INTS, &one) && xl6ArrayPushV1(source, INTS, &two));
        xvalue* alias = text ? xl6ArrayEraseV1(source, TEXTS, xl6StringConverterV1()) :
            xl6ArrayEraseV1(source, INTS, NULL); assert(alias == source);
        int64_t cleanup = XL6_METHOD_CLEANUP;
        assert(xrtMemDebugFailAfter((size_t)strtoull(argv[2], NULL, 10)));
        XL6_METHOD_CORE_RESULT result = text ? XL6_METHOD_TEXT(source) : XL6_METHOD_CORE(source);
        bool hit = xrtMemDebugFailTriggered(); xrtMemDebugFailClear();
        assert(hit ? !result.ok && xrtGetError() : result.ok && !xrtGetError());
        if (hit) {
            const xerror* failure=xrtGetError();
            while (failure && xrtErrorKind(failure)!=XERR_MEMORY) failure=xrtErrorCause(failure);
            assert(failure);
        }
        if (!text) assert(XL6_METHOD_CLEANUP == cleanup + 1);
        xerror* failure = xrtTakeError();
        /* Individual successful mutations may already have committed. The
         * whole function is not transactional; its owner must still be valid. */
        int64_t size = text ? xl6ArraySizeV1(source, TEXTS, false) : xl6ArraySizeV1(source, INTS, false);
#ifdef XL6_METHOD_EXTRACT
        assert(size >= 0 && size <= 2);
#else
        assert(size == 2 || size == 3);
#endif
        assert(!xrtGetError());
        xrtValueRelease(alias); xrtValueRelease(source); xrtErrorFree(failure); balanced(&before);
        xlir_module_fini(); xlir_module_fini(); assert(!xrtGetError());
        printf("generated/native Array methods %s point=%s hit=%u balanced\n", argv[1], argv[2], hit); return 0;
    }
    for (unsigned cycle = 0; cycle < 100; ++cycle) {
        xvalue* values = xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1, INTS,
            XL6_VALUE_ADMIT_I64, false, XL6_METHOD_I64_ELEMENT); assert(values);
        int64_t one = 1, two = 2;
        assert(xl6ArrayPushV1(values, INTS, &one) && xl6ArrayPushV1(values, INTS, &two));
        xvalue* alias = xl6ArrayEraseV1(values, INTS, NULL); assert(alias == values);
        int64_t cleanup = XL6_METHOD_CLEANUP;
        XL6_METHOD_CORE_RESULT core = XL6_METHOD_CORE(values);
        assert(core.ok && core.value == 3 && XL6_METHOD_CLEANUP == cleanup + 1);
        int64_t a = 0, b = 0;
        assert(xl6ArrayReadV1(values, INTS, 0, &a) && xl6ArrayReadV1(values, INTS, 1, &b));
        assert(a == 1 && b == 2);
        XL6_METHOD_CLEAR_RESULT cleared = XL6_METHOD_CLEAR(values);
        assert(cleared.ok && cleared.value == 0 && xl6ArraySizeV1(alias, INTS, false) == 0);
        xrtValueRelease(alias); xrtValueRelease(values);
        values = xl6SequencePrimitiveCreateV1(XL6_SEQUENCE_ARRAY_V1, TEXTS,
            XL6_VALUE_ADMIT_STRING, false, XL6_METHOD_STRING_ELEMENT); assert(values);
        xstrview nul = {"a\0你", 5}, last = {"z", 1};
        assert(xl6ArrayPushV1(values, TEXTS, &nul) && xl6ArrayPushV1(values, TEXTS, &last));
        alias = xl6ArrayEraseV1(values, TEXTS, xl6StringConverterV1()); assert(alias == values);
        XL6_METHOD_TEXT_RESULT text = XL6_METHOD_TEXT(values); assert(text.ok && text.value == 5);
        xstrview copy = {0}; assert(xl6ArrayReadV1(alias, TEXTS, 0, &copy));
        assert(copy.Size == 5 && !memcmp(copy.Data, nul.Data, 5)); xrtFree((void*)copy.Data);
        xrtValueRelease(alias); xrtValueRelease(values);
    }
    balanced(&before);
    xlir_module_fini(); xlir_module_fini(); assert(!xrtGetError());
    puts("generated/native Array methods: 100 physical cycles balanced; exact NUL bytes and order"); return 0;
}
