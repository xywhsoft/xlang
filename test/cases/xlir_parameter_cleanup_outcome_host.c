#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_parameter_cleanup_outcome.c"

void parameter_cleanup_oom(void);
static int fault, probe, liveMask, order, checks, finalChecks;
void parameter_open(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 5 && !(liveMask & bit)); liveMask |= bit;
}
void parameter_drop(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    xl6_error_result failure = {false, NULL};
    assert(id >= 1 && id <= 5 && (liveMask & bit));
    liveMask &= ~bit; order = order * 10 + (int)id;
    if (fault == 0 || id == 1 || (fault == 5 && (id == 3 || (probe == 10 && id == 4)))) return;
    if ((fault == 2 || fault == 3) && id == 3) {
        if (fault == 3) parameter_cleanup_oom();
        xl6ErrorScopePropagateFailure(failure);
    } else if ((fault == 4 && id != 3) || (fault == 5 && id == 2)) {
        xl6ErrorScopePropagateFailure(failure);
    } else {
        failure.error = xrtErrorCreate(15, "parameter-cleanup", 100 + (int)id, "parameter drop failed");
        assert(failure.error != NULL); xrtSetErrorTake(failure.error);
    }
}
void parameter_primary(void)
{
    xl6_error_result failure = {false, NULL};
    if (fault == 0) return;
    if (fault != 2) {
        failure.error = xrtErrorCreate(15, "parameter-primary", 777, "original failure");
        assert(failure.error != NULL);
    }
    xl6ErrorScopePropagateFailure(failure);
}
void parameter_checked(int64_t code, int64_t live)
{
    int expected = probe == 7 ? (fault != 2 ? 777 : 0) : (fault == 1 || fault == 4 ? 103 : 0);
    if (liveMask != 1 || code != expected)
        fprintf(stderr, "parameter catch: probe=%d fault=%d live=%d code=%lld expected=%d\n",
            probe, fault, liveMask, (long long)code, expected);
    assert(live == 1 && liveMask == 1 && code == expected); ++checks;
}
void parameter_finally(int64_t first, int64_t last)
{
    assert(first == 2 && last == 3 && liveMask == 15 && order == 0); ++finalChecks;
}
void parameter_cleanup_case(int index, int mode)
{
    xl6_error_scope boundary = {0};
    xl6_error_result outcome;
    int64_t value = -1;
    int expectedOrder = index == 10 ? 3421 : index == 4 ? 32541 :
        (index >= 1 && index <= 3) || (index == 7 && mode != 0) ? 321 : 3241;
    probe = index; fault = mode; liveMask = order = checks = finalChecks = 0;
    assert(xrtGetError() == NULL && xl_script_module_init(NULL) && xl6ErrorScopeBegin(&boundary));
    switch (probe) {
    case 0: value = parameter_cleanup_owned(); break;
    case 1: value = parameter_cleanup_scalar(); break;
    case 2: value = parameter_cleanup_void(); break;
    case 3: value = parameter_cleanup_implicit(); break;
    case 4: value = parameter_cleanup_product(); break;
    case 5: value = parameter_cleanup_cell(); break;
    case 6: value = parameter_cleanup_finally(); break;
    case 7: value = parameter_cleanup_throw(); break;
    case 8: value = parameter_cleanup_out(); break;
    case 9: value = parameter_cleanup_byref(); break;
    case 10: value = parameter_cleanup_alias(); break;
    default: assert(0);
    }
    outcome = xl6ErrorScopeEnd(&boundary);
    if (!outcome.ok || value != 1 || liveMask != 0 || order != expectedOrder || checks != (fault != 0))
        fprintf(stderr, "parameter probe=%d fault=%d result=%lld ok=%d live=%d order=%d expected=%d checks=%d finally=%d\n",
            probe, fault, (long long)value, (int)outcome.ok, liveMask, order, expectedOrder, checks, finalChecks);
    assert(outcome.ok && outcome.error == NULL && value == 1 && liveMask == 0 && order == expectedOrder);
    assert(checks == (fault != 0) && finalChecks == (probe == 6));
    assert(xl_script_module_unit_checked() && xrtGetError() == NULL);
}
