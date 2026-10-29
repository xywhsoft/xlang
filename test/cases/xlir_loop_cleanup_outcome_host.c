#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_loop_cleanup_outcome.c"

void loop_cleanup_oom(void);
static int fault, probe, liveMask, checks, innerChecks, triggered, recovered;
static int64_t order;
static const int failIds[] = {3,3,3,3,4,4,4,4,5,2,7,4,5,3,2,6,3,3,3,3};
void loop_open(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 9 && !(liveMask & bit)); liveMask |= bit;
}
void loop_drop(int64_t id)
{
    int first;
    xl6_error_result failure = {false, NULL};
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 9 && (liveMask & bit));
    liveMask &= ~bit; order = order * 10 + id;
    if (fault == 0 || id == 1 || recovered || (!triggered && id != failIds[probe])) return;
    first = !triggered; triggered = 1;
    if ((first && (fault == 2 || fault == 3 || fault == 5)) ||
        (!first && fault == 4)) {
        if (first && fault == 3) loop_cleanup_oom();
        xl6ErrorScopePropagateFailure(failure);
    } else {
        failure.error = xrtErrorCreate(15, "loop-cleanup", 100 + (int)id, "loop owner drop failed");
        assert(failure.error != NULL); xrtSetErrorTake(failure.error);
    }
}
static void loop_raise(int code)
{
    xl6_error_result failure = {false, NULL};
    if (fault == 0) return;
    if (fault != 2) {
        failure.error = xrtErrorCreate(15, "loop-primary", code, "original failure");
        assert(failure.error != NULL);
    }
    xl6ErrorScopePropagateFailure(failure);
}
void loop_primary(void) { if (probe == 11) loop_raise(777); }
void loop_next_fault(void) { if (probe == 13) loop_raise(778); }
void loop_iter_fault(void) { if (probe == 14) loop_raise(779); }
void loop_checked(int64_t code, int64_t inner)
{
    int expected = probe == 11 || probe == 13 || probe == 14
        ? (fault == 2 ? 0 : probe == 11 ? 777 : probe == 13 ? 778 : 779)
        : fault == 1 || fault == 4 ? 100 + failIds[probe] : 0;
    int expectedLive = inner ? 15 : 1;
    if (liveMask != expectedLive || code != expected)
        fprintf(stderr, "loop catch: probe=%d fault=%d inner=%lld live=%d code=%lld expected=%d order=%lld\n",
            probe, fault, (long long)inner, liveMask, (long long)code, expected, (long long)order);
    assert(liveMask == expectedLive && code == expected);
    if (inner) { assert(probe == 12); ++innerChecks; recovered = 1; }
    else ++checks;
}
void loop_cleanup_case(int index, int mode)
{
    static const int64_t success[] = {
        45321,4321,45321,43261,45321,4321,45321,43261,45321,43261,
        76543291,54321,54321,4321,4321,76543291,4321,4321,43261,4321};
    static const int64_t failure[] = {
        45321,4321,45321,43621,4321,4321,4321,46321,45321,43261,
        79654321,54321,54321,321,21,76954321,4321,4321,43621,4321};
    xl6_error_scope boundary = {0};
    xl6_error_result outcome;
    int64_t value = -1;
    int64_t expectedOrder = mode == 0 ? success[index] : failure[index];
    probe = index; fault = mode; liveMask = checks = innerChecks = triggered = recovered = 0; order = 0;
    assert(xrtGetError() == NULL && xl_script_module_init(NULL) && xl6ErrorScopeBegin(&boundary));
    switch (probe) {
    case 0: value = loop_cleanup_normal(); break;
    case 1: value = loop_cleanup_break(); break;
    case 2: value = loop_cleanup_continue(); break;
    case 3: value = loop_cleanup_owned(); break;
    case 4: value = loop_cleanup_item_normal(); break;
    case 5: value = loop_cleanup_item_break(); break;
    case 6: value = loop_cleanup_item_continue(); break;
    case 7: value = loop_cleanup_item_owned(); break;
    case 8: value = loop_cleanup_false_item(); break;
    case 9: value = loop_cleanup_outer_owned(); break;
    case 10: value = loop_cleanup_nested_owned(); break;
    case 11: value = loop_cleanup_primary(); break;
    case 12: value = loop_cleanup_inner_handler(); break;
    case 13: value = loop_cleanup_next(); break;
    case 14: value = loop_cleanup_iter(); break;
    case 15: value = loop_cleanup_nested_resource(); break;
    case 16: value = loop_cleanup_generator_normal(); break;
    case 17: value = loop_cleanup_generator_break(); break;
    case 18: value = loop_cleanup_generator_owned(); break;
    case 19: value = loop_cleanup_generator_continue(); break;
    default: assert(0);
    }
    outcome = xl6ErrorScopeEnd(&boundary);
    if (!outcome.ok || value != 1 || liveMask != 0 || order != expectedOrder ||
        checks != (mode != 0 && probe != 12) || innerChecks != (mode != 0 && probe == 12))
        fprintf(stderr, "loop probe=%d fault=%d result=%lld ok=%d live=%d order=%lld expected=%lld checks=%d inner=%d\n",
            probe, fault, (long long)value, (int)outcome.ok, liveMask, (long long)order,
            (long long)expectedOrder, checks, innerChecks);
    assert(outcome.ok && outcome.error == NULL && value == 1 && liveMask == 0 && order == expectedOrder);
    assert(checks == (mode != 0 && probe != 12) && innerChecks == (mode != 0 && probe == 12));
    assert(xl_script_module_unit_checked() && xrtGetError() == NULL);
}
