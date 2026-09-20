#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int boxing_outcome_init(void);
extern void boxing_outcome_finish(void);
extern int64_t boxing_outcome_invoke(int kind, int caught);
extern int64_t boxing_outcome_callback(void);
extern int64_t boxing_outcome_failed_insertion(void);
#include "xlir_owner_fault_runtime.h"

static int parse_int_arg(const char* text)
{
    char* end = NULL;
    long value;
    errno = 0;
    value = strtol(text, &end, 10);
    assert(errno == 0 && end != text && *end == '\0' &&
        value >= INT_MIN && value <= INT_MAX);
    return (int)value;
}

void boxing_arm_insertion_fault(void) { assert(xrtMemDebugFailAfter(0)); }
static xerror* insertionCallbackError;
static unsigned insertionCallbackCalls;
void boxing_raise_insertion_error(void)
{
    assert(insertionCallbackError);
    ++insertionCallbackCalls;
    xrtSetErrorTake(xrtErrorRef(insertionCallbackError));
}

static void prepare_callback_error(void)
{
    insertionCallbackError = xrtErrorCreate(XERR_ARGUMENT, "boxing-test", 91, "preallocated insertion cleanup failure");
    assert(insertionCallbackError);
}

static unsigned run(int kind, int caught, int fault)
{
    xmemdebugsnapshot before; int64_t value; xerror* error; unsigned allocations;
    xrtMemDebugSnapshot(&before); continuation_owner_fault_arm(fault);
    value = boxing_outcome_invoke(kind, caught); error = xrtTakeError();
    allocations = continuation_owner_fault_end(fault);
    if ((fault < 0 && (value != 9 || error)) ||
        (fault >= 0 && caught && (value != 77 || error)) ||
        (fault >= 0 && !caught && (value != 0 || !error || xrtErrorKind(error) != XERR_MEMORY))) {
        fprintf(stderr, "boxing outcome: kind=%d caught=%d fault=%d value=%lld error=%d\n",
            kind, caught, fault, (long long)value, error ? (int)xrtErrorKind(error) : -1);
        abort();
    }
    xrtErrorFree(error); continuation_owner_fault_balanced(&before);
    return allocations;
}

int main(int argc, char** argv)
{
    int smoke = argc == 2 && !strcmp(argv[1], "--smoke");
    int round, kind, caught; unsigned faults = 0, executions = 0; xmemdebugsnapshot before, after;
    if (smoke) {
        assert(boxing_outcome_init());
        prepare_callback_error();
        for (kind = 0; kind < 32; ++kind) for (caught = 0; caught < 2; ++caught) {
            assert(boxing_outcome_invoke(kind, caught) == 9 && !xrtGetError());
        }
        assert(boxing_outcome_callback() == 77 && !xrtGetError());
        xrtErrorFree(insertionCallbackError); insertionCallbackError = NULL;
        boxing_outcome_finish(); puts("boxing allocation outcome C-host smoke: 64 executions plus true-result callback failure"); return 0;
    }
    assert(xrtMemDebugEnable(true) && boxing_outcome_init()); prepare_callback_error(); xrtMemDebugSnapshot(&before);
    if (argc == 4) {
        unsigned count = run(parse_int_arg(argv[1]), parse_int_arg(argv[2]),
            parse_int_arg(argv[3]));
        printf("boxing allocation directed: kind=%s caught=%s fault=%s allocations=%u\n", argv[1], argv[2], argv[3], count);
        xrtErrorFree(insertionCallbackError); insertionCallbackError = NULL;
        boxing_outcome_finish(); return 0;
    }
    for (round = 0; round < 20; ++round) for (kind = 0; kind < 32; ++kind) for (caught = 0; caught < 2; ++caught) {
        unsigned count = run(kind, caught, -1), fault; ++executions;
        if (kind != 9 && kind != 10) assert(count > 0);
        for (fault = 0; fault < count; ++fault) { run(kind, caught, (int)fault); ++faults; }
    }
    for (round = 0; round < 20; ++round) {
        xmemdebugsnapshot callbackBefore;
        xerror* original;
        unsigned callbackCount = insertionCallbackCalls;
        xrtMemDebugSnapshot(&callbackBefore);
        assert(boxing_outcome_callback() == 77 && !xrtGetError());
        assert(insertionCallbackCalls == callbackCount + 1);
        continuation_owner_fault_balanced(&callbackBefore);
        assert(boxing_outcome_failed_insertion() == 0);
        assert(insertionCallbackCalls == callbackCount + 2);
        original = xrtTakeError();
        assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
        if (!original || xrtErrorKind(original) != XERR_MEMORY) {
            fprintf(stderr, "insertion original failure replaced: error=%d\n", original ? (int)xrtErrorKind(original) : -1);
            abort();
        }
        xrtErrorFree(original); continuation_owner_fault_balanced(&callbackBefore);
    }
    xrtMemDebugSnapshot(&after); continuation_owner_fault_balanced(&before);
    printf("boxing allocation outcome: 32 shapes, %u baseline executions, %u real allocation failures, %llu balanced allocations, 20 true-result callback failures, 20 insertion-failure/raising-destructor pairs\n",
        executions, faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    xrtErrorFree(insertionCallbackError); insertionCallbackError = NULL;
    boxing_outcome_finish(); return 0;
}
