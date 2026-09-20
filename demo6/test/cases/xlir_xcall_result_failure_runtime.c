#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void xlir_xcall_result_prepare(void);
int xlir_xcall_result_case(int producer, int site, int index);
void xlir_xcall_result_finish(void);

static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;
    xrtMemDebugSnapshot(&after);
    if (after.LiveCount != before->LiveCount || after.LiveBytes != before->LiveBytes)
        fprintf(stderr, "XCall result leak: objects %+lld, bytes %+lld\n",
            (long long)after.LiveCount - (long long)before->LiveCount,
            (long long)after.LiveBytes - (long long)before->LiveBytes);
    assert(after.LiveCount == before->LiveCount && after.LiveBytes == before->LiveBytes);
    assert(after.AllocCount - before->AllocCount == after.FreeCount - before->FreeCount);
    assert(after.DoubleFreeCount == before->DoubleFreeCount);
    assert(after.InvalidFreeCount == before->InvalidFreeCount);
    assert(after.UseAfterFreeCount == before->UseAfterFreeCount);
}

int main(int argc, char** argv)
{
    xmemdebugsnapshot before, prepared;
    int cycle, index, producer;
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) {
        /* The compiler's JIT runtime does not enable memory-debug accounting.
         * Keep all nine deterministic transport failure sites in this lane;
         * real allocator faults and accounting run in the strict DLL lane. */
        xlir_xcall_result_prepare();
        for (cycle = 0; cycle < 100; ++cycle) {
            for (index = 0; index < 2; ++index)
                assert(xlir_xcall_result_case(0, 1, index));
            for (index = 0; index < 7; ++index)
                assert(xlir_xcall_result_case(0, 2, index));
            assert(xlir_xcall_result_case(0, 0, 0));
            for (producer = 1; producer < 4; ++producer) {
                assert(xlir_xcall_result_case(producer, 2, 0));
                assert(xlir_xcall_result_case(producer, 0, 0));
            }
        }
        xlir_xcall_result_finish();
        puts("XCall result smoke: 1200 injected failures, 400 successes, product/single carriers, preserved errors and live closure captures");
        return 0;
    }
    assert(argc == 1);
    assert(xrtMemDebugEnable(true));
    assert(xrtMemDebugFailAfter(1000000));
    xrtMemDebugFailClear();
    /* Warm lazy error/scope and module infrastructure before accounting. */
    xlir_xcall_result_prepare();
    assert(xlir_xcall_result_case(0, 0, 0));
    xlir_xcall_result_finish();
    xrtMemDebugSnapshot(&before);
    xlir_xcall_result_prepare();
    xrtMemDebugSnapshot(&prepared);
    for (cycle = 0; cycle < 100; ++cycle) {
        for (index = 0; index < 2; ++index) {
            int preserved = xlir_xcall_result_case(0, 1, index);
            balanced(&prepared);
            assert(preserved);
        }
        for (index = 0; index < 7; ++index) {
            int preserved = xlir_xcall_result_case(0, 2, index);
            balanced(&prepared);
            assert(preserved);
        }
        for (index = 0; index < 2; ++index) {
            int preserved = xlir_xcall_result_case(0, 3, index);
            balanced(&prepared);
            assert(preserved);
        }
        {
            int preserved = xlir_xcall_result_case(0, 4, 4);
            balanced(&prepared);
            assert(preserved);
        }
        assert(xlir_xcall_result_case(0, 0, 0));
        balanced(&prepared);
        for (producer = 1; producer < 4; ++producer) {
            int preserved = xlir_xcall_result_case(producer, 2, 0);
            balanced(&prepared);
            assert(preserved);
            assert(xlir_xcall_result_case(producer, 0, 0));
            balanced(&prepared);
        }
    }
    xlir_xcall_result_finish();
    balanced(&before);
    puts("XCall result transaction: 1500 partial failures (300 real allocation faults), 400 successes, product/single carriers, original error identity and balanced managed owners");
    return 0;
}
