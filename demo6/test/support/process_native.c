#define XRT_MODULE_PROCESS_RUN
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE_TEXT
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#define XRUNTIME_MODULE_TYPED_ARRAY_VALUE
#define XRUNTIME_MODULE_RUNTIME_VALUE_TYPE
#define XRUNTIME_MODULE_RUNTIME_TYPE_STRING_VALUE
#define XRT_MODULE_ATOMIC
#define XRUNTIME_IMPLEMENTATION
#define XRT_MODULE_ONCE
#include "lib/xruntime/xruntime.h"
#include "src/api/xl_foreign_error.c"
#include "src/runtime/xl6_native_handle.c"
#include <assert.h>
#include <stdio.h>

void testProcessPlanOom(void)
{
#if !defined(_WIN32) && !defined(_WIN64)
    const cstr args[] = {"first", "", "last"};
    unsigned failures = 0, completions = 0;
    for (unsigned shell = 0; shell < 2; ++shell) {
        bool complete = false;
        for (size_t point = 0; point < 64; ++point) {
            xprocessconfig config;
            xprocessposixplan plan;
            xmemdebugsnapshot before, after;
            assert(xrtProcessConfigInit(&config));
            config.InheritEnv = false;
            config.Target = shell ? XPROCESS_SHELL : XPROCESS_EXEC;
            config.Program = "/bin/true";
            config.Command = "exit 0";
            config.Args = args;
            config.ArgCount = 3;
            xrtMemDebugSnapshot(&before);
            assert(xrtMemDebugFailAfter(point));
            bool built = __xrtProcessPlanPrepare(&config, &plan);
            bool hit = xrtMemDebugFailTriggered();
            xrtMemDebugFailClear();
            if (hit) {
                assert(!built && xrtErrorKind(xrtGetError()) == XERR_MEMORY);
                ++failures;
            } else {
                assert(built && !xrtGetError());
                ++completions;
            }
            __xrtProcessPlanUnit(&plan);
            xrtClearError();
            xrtMemDebugSnapshot(&after);
            assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
            assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
            assert(before.InvalidFreeCount == after.InvalidFreeCount);
            assert(before.DoubleFreeCount == after.DoubleFreeCount);
            if (!hit) { complete = true; break; }
        }
        assert(complete);
    }
    printf("POSIX launch plan: %u allocation failures and %u completions, balanced\n", failures, completions);
#endif
}
