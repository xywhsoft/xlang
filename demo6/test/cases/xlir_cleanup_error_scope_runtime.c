#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void xlir_foreign_error_peer_fail(const xerror* error);
void xlir_cleanup_error_peer_finalize(const xerror* first, const xerror* second, int* steps);

static void consume_failure(xl6_error_result result, const xerror* expected)
{
    assert(!result.ok && result.error == expected);
    xrtErrorFree(result.error);
}

static void expect_success(xl6_error_result result)
{
    assert(result.ok && result.error == NULL);
}

/* Failure identity is latched independently of diagnostic allocation. Exercise
 * every notification entry against every later source, including a separately
 * linked native callback that consumes its diagnostic before returning. */
static void first_missing_matrix(const xerror* later, const xerror* active, int allocation_faults)
{
    int first_kind, later_kind, iteration;
    for (first_kind = 0; first_kind < (allocation_faults ? 3 : 2); ++first_kind) {
        for (later_kind = 0; later_kind < 4; ++later_kind) {
            for (iteration = 0; iteration < 100; ++iteration) {
                xl6_error_scope scope;
                xl6_foreign_error_scope outer;
                xrtSetError(active);
                assert(xl6ForeignCallBegin(&outer));
                assert(xl6ErrorScopeBegin(&scope));
                assert(xrtGetError() == NULL);
                if (first_kind == 0) {
                    xl6ErrorScopeCaptureFailure(&scope);
                } else if (first_kind == 1) {
                    xl6_error_result missing = {false, NULL};
                    xl6ErrorScopePropagateFailure(missing);
                } else {
                    assert(xrtMemDebugFailAfter(0));
                    xl6ForeignCallbackFail();
                    assert(xrtMemDebugFailTriggered());
                    xrtMemDebugFailClear();
                    xrtClearError();
                }
                assert(scope.failed && scope.pending == NULL);
                if (later_kind == 0) {
                    xrtSetError(later);
                    xl6ErrorScopeCaptureFailure(&scope);
                } else if (later_kind == 1) {
                    xl6_error_result failed = {false, xrtErrorRef(later)};
                    xl6ErrorScopePropagateFailure(failed);
                    xrtClearError();
                } else if (later_kind == 2) {
                    xlir_foreign_error_peer_fail(later);
                } else {
                    xrtSetError(later);
                }
                consume_failure(xl6ErrorScopeEnd(&scope), NULL);
                assert(xrtGetError() == NULL);
                /* The inner catch consumes this failure. It must not poison
                 * the foreign caller or replace its pre-existing diagnostic. */
                expect_success(xl6ForeignCallEnd(&outer));
                assert(xrtGetError() == active);
                xrtClearError();
            }
        }
    }
}

int main(int argc, char** argv)
{
    xl6_error_scope cleanup, nested;
    xl6_foreign_error_scope foreign;
    xmemdebugsnapshot before, after;
    xerrordesc description = {XERR_STATE, 17, 23, "xlang", "throw", "cleanup first", "RuntimeError", NULL};
    xerrorlocation location = {"destructor.xl", 41, 7};
    xerror* first;
    xerror* second;
    xerror* active;
    int steps = 0;
    int i;
    if (argc == 2 && strcmp(argv[1], "--smoke") == 0) {
        /* The JIT/static compiler runtime does not enable memory-debug fault
         * injection. Exercise explicit missing-error results there; the DLL
         * lane above also tests actual callback diagnostic allocation faults. */
        second = xrtErrorCreate(XERR_IO, "xlang", 29, "cleanup second");
        active = xrtErrorCreate(XERR_ARGUMENT, "xlang", 31, "already unwinding");
        assert(second != NULL && active != NULL);
        first_missing_matrix(second, active, 0);
        xrtErrorFree(second);
        xrtErrorFree(active);
        assert(xrtGetError() == NULL);
        puts("cleanup first-failure smoke: 800 explicit missing-error transactions (8 paths), native consumption and nested catch isolation ok");
        return 0;
    }
    assert(argc == 1);
    assert(xrtMemDebugEnable(true));
    assert(xl6ErrorScopeBegin(&cleanup));
    expect_success(xl6ErrorScopeEnd(&cleanup));
    xrtSetErrorTake(xrtErrorCreate(XERR_STATE, "warmup", 0, "warmup"));
    xrtClearError();
    xrtMemDebugSnapshot(&before);
    first = xrtErrorBuildAt(&description, &location);
    second = xrtErrorCreate(XERR_IO, "xlang", 29, "cleanup second");
    active = xrtErrorCreate(XERR_ARGUMENT, "xlang", 31, "already unwinding");
    assert(first != NULL && second != NULL && active != NULL);

    for (i = 0; i < 1000; ++i) {
        xrtSetError(active);
        assert(xl6ErrorScopeBegin(&cleanup));
        xlir_cleanup_error_peer_finalize(first, second, &steps);
        assert(xrtGetError() == NULL);
        consume_failure(xl6ErrorScopeEnd(&cleanup), first);
        /* End keeps both outcomes separate; the IR cleanup route, not this
         * runtime helper, chooses whether the active error or new one wins. */
        assert(xrtGetError() == active);
        xrtClearError();
    }
    assert(steps == 2000);

    /* Foreign callback failure handled inside cleanup does not escape to an
     * outer foreign transaction. Separate per-domain TLS would get this wrong. */
    assert(xl6ForeignCallBegin(&foreign));
    assert(xl6ErrorScopeBegin(&cleanup));
    xlir_foreign_error_peer_fail(first);
    consume_failure(xl6ErrorScopeEnd(&cleanup), first);
    expect_success(xl6ForeignCallEnd(&foreign));

    /* The inverse nesting also respects the inner catch boundary. */
    assert(xl6ErrorScopeBegin(&cleanup));
    assert(xl6ForeignCallBegin(&foreign));
    xlir_foreign_error_peer_fail(second);
    consume_failure(xl6ForeignCallEnd(&foreign), second);
    expect_success(xl6ErrorScopeEnd(&cleanup));

    assert(xl6ForeignCallBegin(&foreign));
    assert(xl6ErrorScopeBegin(&cleanup));
    xlir_cleanup_error_peer_finalize(first, second, &steps);
    xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&cleanup));
    assert(xrtGetError() == first);
    xrtClearError();
    consume_failure(xl6ForeignCallEnd(&foreign), first);

    /* A new inner failure cannot overwrite an already captured outer one. */
    assert(xl6ErrorScopeBegin(&cleanup));
    xrtSetError(first);
    xl6ErrorScopeCaptureFailure(&cleanup);
    assert(xl6ErrorScopeBegin(&nested));
    xlir_foreign_error_peer_fail(second);
    xl6ErrorScopePropagateFailure(xl6ErrorScopeEnd(&nested));
    consume_failure(xl6ErrorScopeEnd(&cleanup), first);
    assert(xrtGetError() == NULL);

    assert(xl6ErrorScopeBegin(&cleanup));
    xlir_cleanup_error_peer_finalize(NULL, NULL, &steps);
    expect_success(xl6ErrorScopeEnd(&cleanup));
    assert(steps == 2004);

    /* Propagate failure even when allocation cannot produce an error object. */
    assert(xl6ForeignCallBegin(&foreign));
    assert(xl6ErrorScopeBegin(&cleanup));
    assert(xrtMemDebugFailAfter(0));
    assert(xrtErrorCreate(XERR_STATE, "xlang", 0, "unallocated") == NULL);
    assert(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    /* The allocator can publish its own emergency diagnostic. Model a native
     * consumer clearing it before the explicit failed-step notification. */
    xrtClearError();
    xl6ErrorScopeCaptureFailure(&cleanup);
    {
        xl6_error_result missing = xl6ErrorScopeEnd(&cleanup);
        assert(!missing.ok && missing.error == NULL);
        xl6ErrorScopePropagateFailure(missing);
    }
    xrtClearError();
    consume_failure(xl6ForeignCallEnd(&foreign), NULL);

    first_missing_matrix(second, active, 1);

    assert(strcmp(xrtErrorOperation(first), "throw") == 0);
    assert(xrtErrorLine(first) == 41 && xrtErrorColumn(first) == 7);
    xrtErrorFree(first);
    xrtErrorFree(second);
    xrtErrorFree(active);
    assert(xrtGetError() == NULL);
    xrtMemDebugSnapshot(&after);
    assert(before.Enabled && after.Enabled);
    assert(after.LiveCount == before.LiveCount && after.LiveBytes == before.LiveBytes);
    assert(after.AllocCount - before.AllocCount == after.FreeCount - before.FreeCount);
    assert(after.DoubleFreeCount == before.DoubleFreeCount);
    assert(after.InvalidFreeCount == before.InvalidFreeCount);
    assert(after.UseAfterFreeCount == before.UseAfterFreeCount);
    printf("cleanup error scope: 2004 steps, 1200 first-missing precedence transactions (12 paths), nested foreign catches, original identity and %llu balanced allocations ok\n",
        (unsigned long long)(after.AllocCount - before.AllocCount));
    return 0;
}
