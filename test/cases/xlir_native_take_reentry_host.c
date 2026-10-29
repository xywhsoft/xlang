/* Exercise the helper selected by the verified TAKE target, not a guessed
 * destructor name. The gate writes this small derived symbol binding. */
#include "../../build/test/xlir_native_take_helper.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef XLIR_NATIVE_TAKE_INJECT
#define xrtValueHandleTake xlir_test_value_handle_take
#endif
#include "../../build/test/xlir_native_handle_bind_generated.c"
#ifdef XLIR_NATIVE_TAKE_INJECT
#undef xrtValueHandleTake
extern xvalue* xrtValueHandleTake(void**, const xvaluehandleops*, void*);
static int inject_failure;
xvalue* xlir_test_value_handle_take(void** handle, const xvaluehandleops* ops, void* user)
{
    if (inject_failure) { inject_failure = 0; return NULL; }
    return xrtValueHandleTake(handle, ops, user);
}
#endif

static void** reentry_slot;
static void* replacement;
static int opened, closed, detached, attached;
static int destroy_failure;

void* make_borrowed_box(int64_t value)
{
    int64_t* box = (int64_t*)malloc(sizeof(*box));
    assert(box != NULL);
    *box = value;
    ++opened;
    return box;
}

void borrowed_box_close(void* handle)
{
    assert(handle != NULL);
    if (reentry_slot != NULL) {
        if (*reentry_slot == NULL) ++detached; else ++attached;
        *reentry_slot = replacement;
        reentry_slot = NULL;
    }
    free(handle);
    ++closed;
    if (destroy_failure == 1) {
        xerror* error = xrtErrorCreate(XERR_STATE, "native-drop", 402, "destructor failed");
        assert(error); xrtSetErrorTake(error);
    } else if (destroy_failure == 2) {
        xl6ErrorScopePropagateFailure((xl6_error_result){false, NULL});
    }
}

int64_t borrowed_box_value(void* handle) { assert(handle != NULL); return *(int64_t*)handle; }
int64_t borrowed_close_count(void) { return closed; }
void expect_i64(int64_t actual, int64_t expected) { assert(actual == expected); }

void xlir_native_take_case(int failed, int replace)
{
    void* slot = make_borrowed_box(41);
    xvalue* boxed;
    int prior_closed = closed, prior_detached = detached;
    replacement = replace ? make_borrowed_box(42) : NULL;
    reentry_slot = &slot;
#ifdef XLIR_NATIVE_TAKE_INJECT
    inject_failure = failed;
#endif
    boxed = XLIR_NATIVE_TAKE_HELPER(&slot);
    if (failed) {
        assert(boxed == NULL);
    } else {
        assert(boxed != NULL && slot == NULL);
        xrtValueRelease(boxed);
    }
    assert(attached == 0 && detached == prior_detached + 1);
    assert(closed == prior_closed + 1 && reentry_slot == NULL);
    /* A destructor may install a distinct owner; the old TAKE must not erase
     * that write after destruction returns, even on allocation failure. */
    assert(slot == replacement);
    if (slot != NULL) {
        assert(*(int64_t*)slot == 42);
        borrowed_box_close(slot);
    }
    replacement = NULL;
    assert(opened == closed);
}

int xlir_native_take_live_handles(void) { return opened - closed; }

void xlir_native_drop_outcome_case(int failure, int primary)
{
    xl6_error_scope scope = {0}; xl6_error_result outcome;
    xerror* initial = primary ? xrtErrorCreate(XERR_STATE, "native-drop", 401, "primary") : NULL;
    int prior = closed;
    assert(!primary || initial);
    assert(!xrtGetError());
    if (initial) xrtSetErrorTake(initial);
    assert(xl6ErrorScopeBeginCleanup(&scope, primary != 0));
    destroy_failure = failure;
    XLIR_NATIVE_DROP_HELPER(make_borrowed_box(7), NULL);
    destroy_failure = 0;
    outcome = xl6ErrorScopeEnd(&scope);
    assert(closed == prior + 1 && opened == closed);
    assert(outcome.ok == (!primary && failure == 0));
    if (primary) assert(outcome.error == initial && xrtErrorCode(outcome.error) == 401);
    else if (failure == 1) assert(outcome.error && xrtErrorCode(outcome.error) == 402);
    else assert(outcome.error == NULL);
    /* Ending the outer cleanup scope restores its caller's diagnostic; the
     * outcome owns a separate reference to that same primary error. */
    assert(xrtGetError() == initial);
    xrtErrorFree(outcome.error);
    xrtClearError();
    assert(!xrtGetError());
}
