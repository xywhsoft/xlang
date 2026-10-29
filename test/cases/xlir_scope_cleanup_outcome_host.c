#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../build/test/xlir_scope_cleanup_outcome.c"

void scope_cleanup_oom(void);
static int mode, firstDrop, liveMask, order, checks, marks, outerChecks;
void scope_cleanup_open(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    assert(id >= 1 && id <= 5 && !(liveMask & bit));
    liveMask |= bit;
}
void scope_cleanup_drop(int64_t id)
{
    int bit = 1 << (int)(id - 1);
    xl6_error_result outcome = {false, NULL};
    assert(id >= 1 && id <= 5 && (liveMask & bit));
    liveMask &= ~bit; order = order * 10 + (int)id;
    if (mode == 0 || id == 1 || (mode == 5 && id == firstDrop)) return;
    if ((mode == 2 || mode == 3) && id == firstDrop) {
        if (mode == 3) scope_cleanup_oom();
        xl6ErrorScopePropagateFailure(outcome);
    } else if ((mode == 4 && id != firstDrop) || (mode == 5 && id == firstDrop - 1)) {
        xl6ErrorScopePropagateFailure(outcome);
    } else {
        outcome.error = xrtErrorCreate(15, "scope-cleanup", 100 + (int)id, "cleanup failure");
        assert(outcome.error != NULL); xrtSetErrorTake(outcome.error);
    }
}
static void check_error(const xerror* value)
{
    assert(mode != 0);
    if (mode == 1 || mode == 4) assert(value != NULL && xrtErrorCode(value) == 100 + firstDrop);
    else assert(value == NULL);
}
void scope_cleanup_checked(int64_t code, int64_t live)
{
    assert(liveMask == live && live == 1);
    assert(code == (mode == 1 || mode == 4 ? 100 + firstDrop : 0)); ++checks;
}
void scope_cleanup_outer_error(void)
{
    ++outerChecks;
}
void scope_cleanup_mark(int64_t bit)
{
    assert((marks & (int)bit) == 0); marks |= (int)bit;
}
void scope_cleanup_case(int probe, int fault)
{
    xl6_error_scope boundary = {0};
    xl6_error_result result;
    int64_t value = -1;
    mode = fault; firstDrop = probe == 1 ? 5 : (probe == 4 || probe == 5) ? 3 : 4;
    liveMask = order = checks = marks = outerChecks = 0;
    assert(xrtGetError() == NULL && xl_script_module_init(NULL));
    assert(xl6ErrorScopeBegin(&boundary));
    switch (probe) {
    case 0: value = scope_cleanup_basic(); break;
    case 1: value = scope_cleanup_deep(); break;
    case 2: value = scope_cleanup_nested_catch(); break;
    case 3: value = scope_cleanup_finally(); break;
    case 4: value = scope_cleanup_unhandled(); break;
    case 5: value = scope_cleanup_cell(); break;
    case 6: value = scope_cleanup_outer_cell(); break;
    default: assert(0);
    }
    result = xl6ErrorScopeEnd(&boundary);
    if (probe == 4 && fault != 0) {
        assert(value == 0 && !result.ok); check_error(result.error);
    } else assert(value == 1 && result.ok && result.error == NULL);
    xrtErrorFree(result.error);
    if (liveMask != 0 || order != (firstDrop == 5 ? 54321 : firstDrop == 3 ? 321 : 4321))
        fprintf(stderr, "scope probe=%d fault=%d live=%d order=%d\n", probe, fault, liveMask, order);
    assert(liveMask == 0 && order == (firstDrop == 5 ? 54321 : firstDrop == 3 ? 321 : 4321));
    assert(checks == (probe != 4 && fault != 0));
    assert(marks == ((fault == 0 ? (probe == 1 ? 5 : 1) : 0) | (probe == 3 ? 2 : 0)));
    assert(outerChecks == (probe == 2 ? 2 : 0));
    assert(xl_script_module_unit_checked() && xrtGetError() == NULL);
}
