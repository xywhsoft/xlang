#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#define XRT_MODULE_STRING
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
extern void string_outcome_prepare(void), string_outcome_finish(void);
extern int string_outcome_invoke(int, int, char**, char*, char**);
static xerror* diagnostic;
static unsigned faults;
xerror* string_outcome_diagnostic(void) { return xrtErrorRef(diagnostic); }

static unsigned run_case(int shape, int mode, int fault, int memory)
{
    const char* expected[] = {"firstsuffix", "firstsuffixsuffixfirst", "firstsuffix", "firstsuffixsuffix", "firstsuffix", NULL,
        "literal", "41", "prefixfirst", "", "true", "41", "41.5", NULL, "int", "41", NULL, "suffix", "first", "custom41"};
    xmemdebugsnapshot before = {0}, armed = {0}, after = {0};
    char *left, *right, *output = NULL; xerror* error; int ok; unsigned count = 0;
    if (memory) xrtMemDebugSnapshot(&before);
    left = xrtStrDup("first"); right = shape == 4 || shape == 18 ? NULL : xrtStrDup("suffix");
    assert(left && (shape == 4 || shape == 18 || right));
    if (memory) xrtMemDebugSnapshot(&armed);
    if (fault >= 0) assert(xrtMemDebugFailAfter((uint64)fault));
    ok = string_outcome_invoke(shape, mode, &left, right, &output); error = xrtTakeError();
    if (memory) { xrtMemDebugSnapshot(&after); count = (unsigned)(after.AllocCount - armed.AllocCount); }
    if (fault >= 0) { assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++faults; }
    if (fault >= 0 && (ok || !error || xrtErrorKind(error) != XERR_MEMORY || output)) {
        fprintf(stderr, "string allocation outcome: shape=%d fault=%d ok=%d error=%d output=%p\n",
            shape, fault, ok, error ? (int)xrtErrorKind(error) : -1, (void*)output); assert(0);
    }
    if (fault < 0 && mode) {
        assert(!ok && !output);
        if (mode == 1) assert(error && xrtErrorCode(error) == 93); else assert(!error);
    } else if (fault < 0) {
        assert(ok && !error);
        if (shape != 5) {
            assert(output);
            if (expected[shape]) assert(!strcmp(output, expected[shape]));
            else if (shape == 13) assert(strlen(output) == 19);
            else assert(shape == 16 && strstr(output, "OutcomeRecord{") && strstr(output, "value=41"));
        }
    }
    if (shape == 5) assert(left && !strcmp(left, fault >= 0 ? "first" : "firstsuffixsuffix"));
    else assert(!left);
    xrtFree(left); xrtFree(output); xrtErrorFree(error);
    if (memory) {
        xrtMemDebugSnapshot(&after);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes &&
            after.AllocCount-before.AllocCount == after.FreeCount-before.FreeCount &&
            before.InvalidFreeCount == after.InvalidFreeCount && before.DoubleFreeCount == after.DoubleFreeCount &&
            before.UseAfterFreeCount == after.UseAfterFreeCount);
    }
    return count;
}
int main(int argc, char** argv)
{
    unsigned count[20] = {0}, shape, cycle, index;
    int memory = !(argc == 2 && !strcmp(argv[1], "--smoke"));
    if (memory) assert(xrtMemDebugEnable(true));
    diagnostic = xrtErrorCreate(XERR_STATE, "string-outcome", 93, "default failed"); assert(diagnostic);
    string_outcome_prepare();
    for (shape = 0; shape < 20; ++shape) {
        count[shape] = run_case((int)shape, 0, -1, memory);
        if (memory) { assert(count[shape]); printf("string allocation baseline: shape=%u allocations=%u\n", shape, count[shape]); }
    }
    for (cycle = 0; cycle < 20; ++cycle) {
        run_case(4, 1, -1, memory); run_case(4, 2, -1, memory);
        for (shape = 0; shape < 20; ++shape) {
            run_case((int)shape, 0, -1, memory);
            if (memory) for (index = 0; index < count[shape]; ++index)
                run_case((int)shape, 0, (int)index, memory);
        }
    }
    string_outcome_finish(); xrtErrorFree(diagnostic);
    printf("string allocation outcome: 20 shapes, %u real allocation failures; BYVAL rollback, inout atomic commit, nullable success, literals, conversions and default failures\n", faults);
    return 0;
}
