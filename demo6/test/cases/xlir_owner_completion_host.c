#include <assert.h>
#include "../../build/test/xlir_owner_completion.c"
static int withoutDiagnostic;
static unsigned dropped, caught, finalized;
static int64_t observed;
void owner_completion_fail(void)
{
    xl6_error_result failure = {false, NULL};
    if (!withoutDiagnostic) {
        failure.error = xrtErrorCreate(XERR_STATE, "owner-completion", 77, "primary failure");
        assert(failure.error);
    }
    xl6ErrorScopePropagateFailure(failure);
}
void owner_completion_drop(void)
{
    xl6_error_result secondary = {false, xrtErrorCreate(XERR_STATE, "owner-completion", 99, "secondary destructor failure")};
    assert(secondary.error); ++dropped; xl6ErrorScopePropagateFailure(secondary);
}
void owner_completion_caught(int64_t code) { ++caught; observed = code; }
void owner_completion_finally(void) { ++finalized; }
const char* owner_completion_string(void)
{
    /* The native ABI borrows this string; CALL's explicit result conversion
     * creates the language owner that must be rolled back on failure. */
    owner_completion_fail(); return "owning language copy despite failure";
}
xvalue* owner_completion_value(void)
{
    xvalue* result = xrtValueInt(123);
    assert(result); owner_completion_fail(); return result;
}
int owner_completion_init(void) { return xlir_module_init(); }
void owner_completion_fini(void) { xlir_module_fini(); }
void owner_completion_cycle(int mode, int kind)
{
    int64_t result;
    withoutDiagnostic = mode;
    dropped = caught = finalized = 0; observed = -1;
    result = xl6_export_owner_completion_case(kind);
    assert(result == 23 && dropped == 1 && caught == 1 && finalized == 1);
    assert(observed == (mode ? 0 : 77) && !xrtGetError());
}
