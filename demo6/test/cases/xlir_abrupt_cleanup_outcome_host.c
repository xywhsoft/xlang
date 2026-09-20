#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "../../build/test/xlir_abrupt_cleanup_outcome.c"

void abrupt_cleanup_oom(void);
static int fault, liveMask, order, checks, finalChecks;
void abrupt_cleanup_open(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 4 && !(liveMask & bit)); liveMask |= bit;
}
void abrupt_cleanup_drop(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    xl6_error_result failure = {false, NULL};
    assert(id >= 1 && id <= 4 && (liveMask & bit));
    liveMask &= ~bit; order = order * 10 + (int)id;
    if (fault == 0 || id == 1 || (fault == 5 && id == 4)) return;
    if ((fault == 2 || fault == 3) && id == 4) {
        if (fault == 3) abrupt_cleanup_oom();
        xl6ErrorScopePropagateFailure(failure);
    } else if ((fault == 4 && id != 4) || (fault == 5 && id == 3)) {
        xl6ErrorScopePropagateFailure(failure);
    } else {
        failure.error = xrtErrorCreate(15, "abrupt-cleanup", 100 + (int)id, "cleanup failed");
        assert(failure.error != NULL); xrtSetErrorTake(failure.error);
    }
}
void abrupt_cleanup_checked(int64_t code, int64_t live)
{
    assert(liveMask == live && (live == 1 || live == 3));
    assert(code == (fault == 1 || fault == 4 ? 104 : 0)); ++checks;
}
void abrupt_cleanup_finally(int64_t id, int64_t live)
{
    assert(liveMask == live && id == (live == 1 ? 1 : 2)); ++finalChecks;
}
void abrupt_cleanup_case(int probe, int mode)
{
    xl6_error_scope boundary = {0};
    xl6_error_result outcome;
    int64_t value = -1;
    int escaped = probe == 4 && mode != 0;
    fault = mode; liveMask = order = checks = finalChecks = 0;
    assert(xrtGetError() == NULL && xl_script_module_init(NULL) && xl6ErrorScopeBegin(&boundary));
    switch (probe) {
    case 0: value = abrupt_cleanup_return(); break;
    case 1: value = abrupt_cleanup_return_finally(); break;
    case 2: value = abrupt_cleanup_break(); break;
    case 3: value = abrupt_cleanup_continue(); break;
    case 4: value = abrupt_cleanup_break_finally(); break;
    case 5: value = abrupt_cleanup_pending(); break;
    case 6: value = abrupt_cleanup_copy(); break;
    case 7: value = abrupt_cleanup_copy_temporary(); break;
    case 8: value = abrupt_cleanup_clone_temporary(); break;
    case 9: value = abrupt_cleanup_pending_override(); break;
    default: assert(0);
    }
    outcome = xl6ErrorScopeEnd(&boundary);
    if (outcome.ok == escaped || value != (escaped ? 0 : probe < 2 && fault == 0 ? 17 : 1) ||
        liveMask != 0 || order != 4321 || checks != (fault != 0))
        fprintf(stderr, "abrupt probe=%d fault=%d result=%lld ok=%d live=%d order=%d checks=%d finally=%d\n",
            probe, fault, (long long)value, (int)outcome.ok, liveMask, order, checks, finalChecks);
    assert(outcome.ok != escaped && value == (escaped ? 0 : probe < 2 && fault == 0 ? 17 : 1));
    /* In breakFinally, Guard 2 belongs outside the protected body. Its later
     * failure must escape after finally, not re-enter the already left catch. */
    if (escaped && fault != 4) assert(outcome.error != NULL && xrtErrorCode(outcome.error) == 102);
    else assert(outcome.error == NULL);
    xrtErrorFree(outcome.error);
    assert(liveMask == 0 && order == 4321 && checks == (fault != 0));
    assert(finalChecks == (probe == 1 || probe == 4));
    assert(xl_script_module_unit_checked() && xrtGetError() == NULL);
}
