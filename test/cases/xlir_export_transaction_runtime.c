#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
void tx_direct_case(int, int);
void tx_callback_case(int, int);
void tx_pending_case(int, int);
void tx_bridge_case(int, int);
void tx_entry_failure_case(void);
void tx_previous_diagnostic_case(void);
void tx_finish(void);
void tx_header_check(void);
static int real_faults;

void tx_error_oom(void)
{
    xerror* error;
    assert(xrtMemDebugFailAfter(0));
    error = xrtErrorCreate(XERR_STATE, "export", 101, "diagnostic allocation failed");
    assert(error == NULL && xrtMemDebugFailTriggered());
    xrtMemDebugFailClear(); ++real_faults;
}

int main(int argc, char** argv)
{
    xmemdebugsnapshot before, after;
    int iteration, mode, kind, fail_after, cases = 0;
    int smoke = argc == 2 && strcmp(argv[1], "--smoke") == 0;
    assert(smoke || (argc == 1 && xrtMemDebugEnable(true)));
    for (kind = 0; kind < 5; ++kind) tx_direct_case(0, kind);
    for (kind = 0; kind < 4; ++kind) tx_callback_case(1, kind);
    tx_previous_diagnostic_case();
    tx_header_check();
    if (!smoke) xrtMemDebugSnapshot(&before);
    for (iteration = 0; iteration < 100; ++iteration) {
        for (mode = 0; mode <= (smoke ? 2 : 3); ++mode) {
            for (kind = 0; kind < 5; ++kind) { tx_direct_case(mode, kind); ++cases; }
            if (mode != 0)
                for (kind = 0; kind < 4; ++kind) { tx_callback_case(mode, kind); ++cases; }
        }
        for (mode = 0; mode < 2; ++mode)
            for (kind = 0; kind < 3; ++kind) { tx_pending_case(mode, kind); ++cases; }
        for (fail_after = 0; fail_after < 2; ++fail_after) {
            if (!smoke) assert(xrtMemDebugFailAfter((size_t)fail_after));
            tx_bridge_case(fail_after, smoke); ++cases;
            if (!smoke) { assert(xrtMemDebugFailTriggered()); xrtMemDebugFailClear(); ++real_faults; }
        }
        tx_entry_failure_case(); ++cases;
        tx_previous_diagnostic_case(); ++cases;
    }
    if (!smoke) {
        xrtMemDebugSnapshot(&after);
        assert(before.Enabled && after.Enabled);
        assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
        assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
        assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
        assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
        assert(real_faults == 1100);
        printf("export transaction: %d cases, %d real allocation failures, %llu balanced allocations\n",
            cases, real_faults, (unsigned long long)(after.AllocCount - before.AllocCount));
    } else printf("export transaction smoke: %d cases, first-error priority and pending ownership passed\n", cases);
    tx_finish();
    return 0;
}
