#define XRT_MODULE_ERROR
#include "lib/xrt/xrt.h"
#include "src/api/xl_foreign_error.h"
#include <assert.h>
#include <string.h>

#if defined(_WIN32)
__declspec(dllexport)
#endif
void xlir_foreign_error_peer_fail(const xerror* error)
{
    xrtSetError(error);
    xl6ForeignCallbackFail();
    assert(xrtGetError() != NULL);
    assert(strcmp(xrtErrorOperation(xrtGetError()), "callback") == 0);
    assert(strcmp(xrtErrorMessage(xrtGetError()), xrtErrorMessage(error)) == 0);
    assert(xrtErrorLine(xrtGetError()) == xrtErrorLine(error));
    assert(xrtErrorCause(xrtGetError()) == xrtErrorCause(error));
    /* This consumption is deliberately in a separately linked module. */
    xrtClearError();
}

#if defined(_WIN32)
__declspec(dllexport)
#endif
void xlir_cleanup_error_peer_finalize(const xerror* first, const xerror* second, int* steps)
{
    xl6_error_scope scope;
    xl6_error_result result;
    assert(xl6ErrorScopeBegin(&scope));
    assert(xrtGetError() == NULL);
    ++*steps;
    if (first != NULL) {
        xrtSetError(first);
        xl6ErrorScopeCaptureFailure(&scope);
    }
    /* A previous failure must not poison the next destructor's entry. */
    assert(xrtGetError() == NULL);
    ++*steps;
    if (second != NULL) {
        xrtSetError(second);
        xl6ErrorScopeCaptureFailure(&scope);
    }
    result = xl6ErrorScopeEnd(&scope);
    if (!result.ok) {
        const xerror* expected = first != NULL ? first : second;
        assert(result.error == expected);
        xl6ErrorScopePropagateFailure(result);
        assert(xrtGetError() == expected);
        /* The void native host may consume its diagnostic after publication. */
        xrtClearError();
    }
}
