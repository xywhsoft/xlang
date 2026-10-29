#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Faults are injected at the generated adapter's ABI boundary, after the
 * language producer has constructed all seven owned results. Runtime calls
 * inside the DLL are deliberately not intercepted. */
#define xrtValueString result_box_string
#define xrtCallResultSetTake result_set_take
#define xrtValueRelease result_release
#include "../../build/test/xlir_xcall_result_failure.c"
#undef xrtValueString
#undef xrtCallResultSetTake
#undef xrtValueRelease
extern xvalue* xrtValueString(xlir_string_view);
extern bool xrtCallResultSetTake(xrtcallresult*, size_t, xvalue**);
extern void xrtValueRelease(xvalue*);
extern size_t xrtCallResultCount(const xrtcallresult*);
extern xvalue* xrtCallResultGet(const xrtcallresult*, size_t);
extern bool xrtMemDebugFailAfter(uint64_t);
extern bool xrtMemDebugFailTriggered(void);
extern void xrtMemDebugFailClear(void);

static xvalue* saved[4];
static xerror* first_error;
static xerror* cleanup_error;
static xerror* allocation_error;
static int failure_site, failure_index, attempts, armed, failed;

void xcall_result_keep(xvalue* callback, int64_t slot)
{
    assert(slot >= 0 && slot < 4 && saved[slot] == NULL);
    saved[slot] = xrtValueRetain(callback);
    assert(saved[slot] != NULL);
}

void xcall_result_arm(void) { armed = 1; }

static int should_fail(int site, int index)
{
    if (!armed || failed || failure_site != site) return 0;
    ++attempts;
    if (index != failure_index) return 0;
    failed = 1;
    xrtSetError(first_error);
    return 1;
}

xvalue* result_box_string(xlir_string_view text)
{
    if (armed && !failed && failure_site == 3 && attempts++ == failure_index) {
        xvalue* result;
        assert(xrtMemDebugFailAfter(0));
        result = xrtValueString(text);
        assert(result == NULL && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        allocation_error = xrtErrorRef((xerror*)xrtGetError());
        assert(allocation_error != NULL);
        failed = 1;
        return result;
    }
    if (should_fail(1, attempts)) return NULL;
    return xrtValueString(text);
}

bool result_set_take(xrtcallresult* result, size_t index, xvalue** value)
{
    if (armed && !failed && failure_site == 4 && index == 4) {
        bool ok;
        assert(xrtMemDebugFailAfter(0));
        ok = xrtCallResultSetTake(result, index, value);
        assert(!ok && *value != NULL && xrtMemDebugFailTriggered());
        xrtMemDebugFailClear();
        allocation_error = xrtErrorRef((xerror*)xrtGetError());
        assert(allocation_error != NULL);
        failed = 1;
        return ok;
    }
    if (should_fail(2, (int)index)) return false;
    return xrtCallResultSetTake(result, index, value);
}

void result_release(xvalue* value)
{
    if (failed && value != NULL) {
        /* Model a native destructor publishing another callback failure.
         * It must not replace the earlier result-transport failure. */
        xrtSetError(cleanup_error);
        xl6ForeignCallbackFail();
    }
    xrtValueRelease(value);
}

void xlir_xcall_result_prepare(void)
{
    assert(xlir_module_init());
    assert(saved[0] != NULL && saved[1] != NULL && saved[2] != NULL && saved[3] != NULL);
    first_error = xlir_error_create("RuntimeError", "result transport failure");
    cleanup_error = xlir_error_create("RuntimeError", "later cleanup failure");
    assert(first_error != NULL && cleanup_error != NULL);
}

int xlir_xcall_result_case(int producer, int site, int index)
{
    xl6_foreign_error_scope scope;
    xl6_foreign_error_result outcome;
    xrtcallframe frame = {0};
    xrtcallresult result = {0};
    bool ok;
    int preserved = 1;
    assert(producer >= 0 && producer < 4);
    failure_site = site;
    failure_index = index;
    attempts = armed = failed = 0;
    assert(xl6ForeignCallBegin(&scope));
    ok = xrtValueInvoke(saved[producer], &frame, &result);
    if (site != 0) {
        assert(failed && !ok);
        assert(xrtCallResultCount(&result) == 0);
        assert(xrtGetError() != NULL);
    } else if (producer == 0) {
        int64_t number = 0;
        xlir_string_view text = {0};
        xrtcallresult closure_result = {0};
        assert(ok && !failed);
        assert(xrtCallResultCount(&result) == 7);
        assert(xrtValueGetString(xrtCallResultGet(&result, 1), &text));
        assert(text.Size == 11 && memcmp(text.Data, "owned first", 11) == 0);
        assert(xrtValueGetInt(xrtCallResultGet(&result, 6), &number) && number == 42);
        assert(xrtValueInvoke(xrtCallResultGet(&result, 3), &frame, &closure_result));
        assert(xrtValueGetInt(xrtCallResultGet(&closure_result, 0), &number) && number == 15);
        xrtCallResultUnit(&closure_result);
    } else {
        assert(ok && !failed && xrtCallResultCount(&result) == 1);
        assert(xrtCallResultGet(&result, 0) != NULL);
    }
    xrtCallResultUnit(&result);
    xrtClearError(); /* Native callers may consume the diagnostic. */
    outcome = xl6ForeignCallEnd(&scope);
    if (site != 0) {
        const xerror* expected = site >= 3 ? allocation_error : first_error;
        if (outcome.ok || outcome.error != expected) {
            fprintf(stderr, "result failure not preserved: site=%d slot=%d ok=%d identity=%d\n",
                site, index, (int)outcome.ok, outcome.error == expected);
            preserved = 0;
        }
    } else {
        assert(outcome.ok && outcome.error == NULL);
    }
    xrtErrorFree(outcome.error);
    xrtErrorFree(allocation_error);
    allocation_error = NULL;
    assert(xrtGetError() == NULL);
    failed = armed = 0;
    return preserved;
}

void xlir_xcall_result_finish(void)
{
    int i;
    for (i = 0; i < 4; ++i) {
        xrtValueRelease(saved[i]);
        saved[i] = NULL;
    }
    xlir_module_fini();
    xrtErrorFree(first_error);
    xrtErrorFree(cleanup_error);
    first_error = cleanup_error = NULL;
    assert(xrtGetError() == NULL);
}
