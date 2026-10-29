#define XRT_MODULE_ERROR
#define XRT_MODULE_MEMORY_DEBUG
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#if defined(_WIN32)
#include <windows.h>
#endif

void xlir_foreign_error_peer_fail(const xerror* error);

static void expectFailure(xl6_foreign_error_result result, const xerror* expected)
{
    assert(!result.ok && result.error == expected);
    xrtErrorFree(result.error);
}

static void expectSuccess(xl6_foreign_error_result result)
{
    assert(result.ok && result.error == NULL);
}

#if defined(_WIN32)
typedef struct { const xerror* error; void* main_fiber; } worker_input;

static void runIsolated(const xerror* error)
{
    xl6_foreign_error_scope scope;
    assert(xrtGetError() == NULL);
    assert(xl6ForeignCallBegin(&scope));
    assert(scope.previous == NULL);
    xlir_foreign_error_peer_fail(error);
    expectFailure(xl6ForeignCallEnd(&scope), error);
    assert(xrtGetError() == NULL);
}

static DWORD WINAPI threadBody(void* data)
{
    runIsolated((const xerror*)data);
    return 0;
}

static VOID WINAPI fiberBody(void* data)
{
    worker_input* input = (worker_input*)data;
    runIsolated(input->error);
    SwitchToFiber(input->main_fiber);
    abort();
}
#endif

int main(void)
{
    xl6_foreign_error_scope outer, inner;
    xerrordesc description = { XERR_STATE, 19, 23, "xlang", "throw",
        "original callback error", "RuntimeError", NULL };
    xerrorlocation location = { "fixture.xl", 17, 9 };
    xerror* cause = xrtErrorCreate(XERR_IO, "native", 31, "root cause");
    xerror* original;
    xerror* other = xrtErrorCreate(XERR_STATE, "xlang", 41, "other error");
    int iteration;
    description.Cause = cause;
    original = xrtErrorBuildAt(&description, &location);
    assert(original != NULL && other != NULL && cause != NULL);
    for (iteration = 0; iteration < 1000; ++iteration) {
        xrtSetError(other);
        assert(xl6ForeignCallBegin(&outer));
        assert(xrtGetError() == NULL);
        xlir_foreign_error_peer_fail(original);
        xlir_foreign_error_peer_fail(other);
        xrtSetError(other);
        expectFailure(xl6ForeignCallEnd(&outer), original);
        assert(xrtGetError() == other);
        assert(outer.previous == NULL && outer.pending == NULL &&
            outer.previous_diagnostic == NULL);
        xrtClearError();
    }

    assert(xl6ForeignCallBegin(&outer));
    assert(xl6ForeignCallBegin(&inner));
    xlir_foreign_error_peer_fail(other);
    expectFailure(xl6ForeignCallEnd(&inner), other);
    /* Language catch handled the inner error, so the callback succeeds. */
    expectSuccess(xl6ForeignCallEnd(&outer));

    assert(xl6ForeignCallBegin(&outer));
    xlir_foreign_error_peer_fail(original);
    assert(xl6ForeignCallBegin(&inner));
    xlir_foreign_error_peer_fail(other);
    expectFailure(xl6ForeignCallEnd(&inner), other);
    expectFailure(xl6ForeignCallEnd(&outer), original);

    assert(xl6ForeignCallBegin(&outer));
    xrtSetError(original);
    expectFailure(xl6ForeignCallEnd(&outer), original);
    xlir_foreign_error_peer_fail(original);
    assert(xl6ForeignCallBegin(&outer));
    expectSuccess(xl6ForeignCallEnd(&outer));

    assert(xl6ForeignCallBegin(&outer));
    xrtSetError(original);
    assert(xrtMemDebugFailAfter(0));
    xl6ForeignCallbackFail();
    assert(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    assert(xrtGetError() == original);
    xrtClearError();
    expectFailure(xl6ForeignCallEnd(&outer), original);

    /* The failure bit is independent of error allocation and native TLS. */
    assert(xl6ForeignCallBegin(&outer));
    assert(xrtGetError() == NULL);
    assert(xrtMemDebugFailAfter(0));
    xl6ForeignCallbackFail();
    assert(xrtMemDebugFailTriggered());
    xrtMemDebugFailClear();
    xrtClearError();
    {
        xl6_foreign_error_result failed = xl6ForeignCallEnd(&outer);
        assert(!failed.ok && failed.error == NULL);
    }

    /* An out-of-order End must not detach live scopes or consume their errors. */
    assert(xl6ForeignCallBegin(&outer));
    xlir_foreign_error_peer_fail(original);
    assert(xl6ForeignCallBegin(&inner));
    {
        xl6_foreign_error_result invalid = xl6ForeignCallEnd(&outer);
        assert(!invalid.ok && invalid.error != NULL);
        assert(xrtErrorKind(invalid.error) == XERR_STATE);
        xrtErrorFree(invalid.error);
    }
    expectSuccess(xl6ForeignCallEnd(&inner));
    expectFailure(xl6ForeignCallEnd(&outer), original);

#if defined(_WIN32)
    {
        HANDLE worker;
        void* fiber;
        worker_input input;
        input.error = other;
        input.main_fiber = ConvertThreadToFiber(NULL);
        assert(input.main_fiber != NULL);
        assert(xl6ForeignCallBegin(&outer));
        xlir_foreign_error_peer_fail(original);
        worker = CreateThread(NULL, 0, threadBody, other, 0, NULL);
        assert(worker != NULL);
        assert(WaitForSingleObject(worker, 10000) == WAIT_OBJECT_0);
        CloseHandle(worker);
        fiber = CreateFiber(0, fiberBody, &input);
        assert(fiber != NULL);
        SwitchToFiber(fiber);
        DeleteFiber(fiber);
        expectFailure(xl6ForeignCallEnd(&outer), original);
        assert(ConvertFiberToThread());
    }
#endif
    assert(xrtGetError() == NULL);
    assert(strcmp(xrtErrorOperation(original), "throw") == 0);
    assert(xrtErrorCode(original) == 19 && xrtErrorSystemCode(original) == 23);
    xrtErrorFree(original);
    xrtErrorFree(other);
    xrtErrorFree(cause);
    puts("foreign scope runtime: 1000 first-error transactions, nested catches, native diagnostics, missing-error allocation failure, LIFO rejection, threads/fibers and separate DLL callbacks ok");
    return 0;
}
