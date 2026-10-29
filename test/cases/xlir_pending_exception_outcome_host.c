#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_pending_exception_outcome.c"
void pending_exception_oom(void);
static int fault, probe, liveMask, checks, finals, raised;
static int64_t order;
void pending_open(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 6 && !(liveMask & bit)); liveMask |= bit;
}
void pending_drop(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 6 && (liveMask & bit));
    liveMask &= ~bit; order = order * 10 + id;
    if (fault >= 4 && raised && id != 1) {
        xerror* error = xrtErrorCreate(15, "pending-drop", 100 + (int)id, "secondary cleanup failure");
        assert(error != NULL); xrtSetErrorTake(error);
    }
}
static void pending_raise(int code)
{
    xl6_error_result outcome = {false, NULL};
    if (fault == 0) return;
    raised = 1;
    if (fault == 3) pending_exception_oom();
    else if (fault == 1 || fault == 4) {
        outcome.error = xrtErrorCreate(15, "pending-primary", code, "pending exception");
        assert(outcome.error != NULL);
    }
    xl6ErrorScopePropagateFailure(outcome);
}
void pending_primary(void) { pending_raise(777); }
void pending_raise_override(void) { pending_raise(888); }
void pending_observe_finally(int64_t stage)
{
    int expected = probe == 5 ? 47 : probe == 7 ? (stage == 1 ? 23 : 35) : 11;
    if (liveMask != expected) fprintf(stderr, "finally probe=%d fault=%d stage=%lld live=%d expected=%d order=%lld\n",
        probe, fault, (long long)stage, liveMask, expected, (long long)order);
    assert(liveMask == expected); ++finals;
}
void pending_checked(int64_t code)
{
    int expected = fault == 1 || fault == 4 ? (probe == 8 ? 888 : 777) : 0;
    if (liveMask != 1 || code != expected) fprintf(stderr,
        "pending catch probe=%d fault=%d live=%d code=%lld expected=%d order=%lld\n",
        probe, fault, liveMask, (long long)code, expected, (long long)order);
    assert(liveMask == 1 && code == expected); ++checks;
}
void pending_exception_case(int index, int mode)
{
    static const int64_t orders[] = {3421,321,321,3421,3421,564321,54321,453621,3421,321,321,321};
    xl6_error_scope boundary = {0};
    xl6_error_result outcome;
    int64_t result = -1;
    probe = index; fault = mode; liveMask = checks = finals = raised = 0; order = 0;
    assert(xrtGetError() == NULL && xl_script_module_init(NULL) && xl6ErrorScopeBegin(&boundary));
    switch (probe) {
    case 0: result = pending_finally(); break;
    case 1: result = pending_typed(); break;
    case 2: result = pending_multi(); break;
    case 3: result = pending_typed_finally(); break;
    case 4: result = pending_multi_finally(); break;
    case 5: result = pending_loop_finally(); break;
    case 6: result = pending_loop_typed(); break;
    case 7: result = pending_nested_finally(); break;
    case 8: result = pending_override(); break;
    case 9: result = pending_route_finally(); break;
    case 10: result = pending_route_typed_finally(); break;
    case 11: result = pending_route_multi_finally(); break;
    default: assert(0);
    }
    outcome = xl6ErrorScopeEnd(&boundary);
    if (!outcome.ok || result != 1 || liveMask != 0 || order != orders[probe] || checks != (mode != 0))
        fprintf(stderr, "pending probe=%d fault=%d result=%lld ok=%d live=%d order=%lld expected=%lld checks=%d finals=%d\n",
            probe, fault, (long long)result, (int)outcome.ok, liveMask, (long long)order, (long long)orders[probe], checks, finals);
    assert(outcome.ok && outcome.error == NULL && result == 1 && liveMask == 0 && order == orders[probe]);
    assert(checks == (mode != 0) && finals == (probe == 7 ? 2 : probe == 1 || probe == 2 || probe == 6 || probe >= 9 ? 0 : 1));
    assert(xl_script_module_unit_checked() && xrtGetError() == NULL);
}
