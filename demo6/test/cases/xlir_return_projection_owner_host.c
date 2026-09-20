#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_return_projection_owner.c"
void return_projection_oom(void);
static int lane, mode, liveMask, order, observations, calls, finalChecks;
static void publish(int kind, int code)
{
    xl6_error_result failure = {false, NULL};
    if (kind == 1) {
        failure.error = xrtErrorCreate(15, "return-projection", code, "return ownership failure");
        assert(failure.error != NULL);
    } else if (kind == 3) return_projection_oom();
    xl6ErrorScopePropagateFailure(failure);
}
void return_projection_open(int64_t id)
{
#ifdef RETURN_PROJECTION_TRACE
    fprintf(stderr, "open %lld\n", (long long)id);
#endif
    int bit = 1 << ((int)id - 1);
    assert(id >= 1 && id <= 2 && !(liveMask & bit)); liveMask |= bit;
}
void return_projection_drop(int64_t id)
{
#ifdef RETURN_PROJECTION_TRACE
    fprintf(stderr, "drop %lld\n", (long long)id);
#endif
    int bit = 1 << ((int)id - 1);
    assert(id >= 1 && id <= 2 && (liveMask & bit));
    liveMask &= ~bit; order = order * 10 + (int)id;
    if (id == 2 && mode >= 1 && mode <= 3) publish(mode, 202);
}
int64_t return_projection_later(void)
{
#ifdef RETURN_PROJECTION_TRACE
    fprintf(stderr, "later\n");
#endif
    assert(liveMask == 3 && order == 0); ++calls;
    if (mode >= 4) publish(mode - 3, 777);
    return 7;
}
void return_projection_finally(int64_t id)
{
    assert(lane == 7 && id == 1 && liveMask == 3 && order == 0); ++finalChecks;
}
void return_projection_observe(void)
{
#ifdef RETURN_PROJECTION_TRACE
    fprintf(stderr, "observe\n");
#endif
    int scalar = lane == 4 || lane == 5 || lane == 13 || lane == 14;
    assert(mode == 0 && liveMask == (scalar ? 0 : 1) && order == (scalar ? 12 : 2));
    ++observations;
}
void return_projection_case(int index, int fault)
{
    xl6_error_scope boundary = {0};
    xl6_error_result result;
    int64_t value;
    int expectedOrder, expectedCode;
    lane = index; mode = fault; liveMask = order = observations = calls = finalChecks = 0;
#ifdef RETURN_PROJECTION_TRACE
    fprintf(stderr, "return projection start lane=%d mode=%d\n", lane, mode);
#endif
    assert(xrtGetError() == NULL && xlir_module_init() && xl6ErrorScopeBegin(&boundary));
    value = return_projection_probe(lane);
    result = xl6ErrorScopeEnd(&boundary);
    expectedOrder = mode >= 4 || lane == 4 || lane == 5 || lane == 13 || lane == 14 ? 12 : 21;
    expectedCode = mode == 1 ? 202 : mode == 4 ? 777 : 0;
    if (result.ok != (mode == 0) || xrtErrorCode(result.error) != expectedCode ||
        liveMask != 0 || order != expectedOrder || observations != (mode == 0) || calls != 1 ||
        finalChecks != (lane == 7) || (mode == 0 && value != 41))
        fprintf(stderr, "return projection lane=%d mode=%d value=%lld ok=%d code=%d live=%d order=%d expected=%d observations=%d calls=%d finally=%d\n",
            lane, mode, (long long)value, result.ok, xrtErrorCode(result.error), liveMask, order,
            expectedOrder, observations, calls, finalChecks);
    assert(result.ok == (mode == 0) && xrtErrorCode(result.error) == expectedCode && liveMask == 0 && order == expectedOrder);
    assert(observations == (mode == 0) && calls == 1 && finalChecks == (lane == 7));
    assert(mode != 0 || (value == 41 && result.error == NULL));
    xrtErrorFree(result.error); xlir_module_fini(); assert(xrtGetError() == NULL);
}
