/* Generated process module preincluded. Keep failure category and ownership
 * diagnostics visible; a triggered failpoint is not a successful test. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static bool reportProcessLive(const xmemdebugallocation* allocation, void* user)
{
    (void)user;
    fprintf(stderr, "live %p size=%zu at %s:%u\n", allocation->Address,
        allocation->Size, allocation->File ? allocation->File : "unknown", allocation->Line);
    return true;
}
int main(int argc, char** argv)
{
    size_t point = argc == 2 ? (size_t)strtoull(argv[1], NULL, 10) : SIZE_MAX;
    xmemdebugsnapshot before, after;
    assert(argc == 1 || argc == 2);
    assert(xrtMemDebugEnable(true));
    xrtMemDebugSnapshot(&before);
    if (point != SIZE_MAX) assert(xrtMemDebugFailAfter(point));
    int initialized = xlir_module_init();
    bool hit = xrtMemDebugFailTriggered();
    xrtMemDebugFailClear();
    const xerror* error = xrtGetError();
    if (hit && (initialized || xrtErrorKind(error) != XERR_MEMORY))
        fprintf(stderr, "point=%zu initialized=%d kind=%d message=%s\n", point,
            initialized, (int)xrtErrorKind(error), error ? xrtErrorMessage(error) : "none");
    assert(hit ? !initialized && xrtErrorKind(error) == XERR_MEMORY : initialized && !error);
    xlir_module_fini();
    xrtClearError();
    xrtMemDebugSnapshot(&after);
    if (before.LiveCount != after.LiveCount || before.LiveBytes != after.LiveBytes)
        xrtMemDebugVisitLive(reportProcessLive, NULL);
    assert(before.LiveCount == after.LiveCount && before.LiveBytes == after.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(before.DoubleFreeCount == after.DoubleFreeCount && before.InvalidFreeCount == after.InvalidFreeCount);
    assert(before.UseAfterFreeCount == after.UseAfterFreeCount);
    printf("process logical allocation point=%zu hit=%d balanced\n", point, hit);
    return 0;
}
