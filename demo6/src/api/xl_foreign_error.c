#define XRT_MODULE_ONCE
#include "lib/xrt/xrt.h"
#include "xl_foreign_error.h"
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
static DWORD xl6_foreign_error_key = FLS_OUT_OF_INDEXES;
#else
#include <pthread.h>
static pthread_key_t xl6_foreign_error_key;
#endif
static xonce xl6_foreign_error_once = XRT_ONCE_INIT;

static bool xl6ForeignErrorKeyInit(void* data)
{
    (void)data;
#if defined(_WIN32)
    /* Match XRT's default error isolation: different Windows fibers may have
     * live native stacks on one OS thread. A plain TLS pointer aliases them. */
    xl6_foreign_error_key = FlsAlloc(NULL);
    return xl6_foreign_error_key != FLS_OUT_OF_INDEXES;
#else
    return pthread_key_create(&xl6_foreign_error_key, NULL) == 0;
#endif
}

static xl6_foreign_error_scope* xl6ForeignErrorTop(void)
{
#if defined(_WIN32)
    return (xl6_foreign_error_scope*)FlsGetValue(xl6_foreign_error_key);
#else
    return (xl6_foreign_error_scope*)pthread_getspecific(xl6_foreign_error_key);
#endif
}

static bool xl6ForeignErrorSetTop(xl6_foreign_error_scope* scope)
{
#if defined(_WIN32)
    return FlsSetValue(xl6_foreign_error_key, scope) != 0;
#else
    return pthread_setspecific(xl6_foreign_error_key, scope) == 0;
#endif
}

static void xl6ForeignErrorSetFailure(xerrkind kind, const char* message)
{
    xrtSetErrorTake(xrtErrorCreate(kind, "xlang", 0, message));
}

XL6_FOREIGN_API bool xl6ErrorScopeBegin(xl6_error_scope* scope)
{
    if (scope == NULL) {
        xl6ForeignErrorSetFailure(XERR_ARGUMENT, "error scope is null");
        return false;
    }
    scope->previous = NULL;
    scope->pending = NULL;
    scope->previous_diagnostic = NULL;
    scope->failed = false;
    if (!xrtOnce(&xl6_foreign_error_once, xl6ForeignErrorKeyInit, NULL)) {
        xl6ForeignErrorSetFailure(XERR_MEMORY, "error scope context initialization failed");
        return false;
    }
    scope->previous = xl6ForeignErrorTop();
    if (!xl6ForeignErrorSetTop(scope)) {
        xl6ForeignErrorSetFailure(XERR_MEMORY, "error scope context binding failed");
        return false;
    }
    scope->previous_diagnostic = xrtTakeError();
    return true;
}

XL6_FOREIGN_API bool xl6ErrorScopeBeginCleanup(xl6_error_scope* scope, bool knownFailure)
{
    if (!xl6ErrorScopeBegin(scope)) return false;
    if (scope->previous != NULL && scope->previous->failed) {
        scope->failed = true;
        scope->pending = xrtErrorRef(scope->previous->pending);
    } else if (knownFailure || scope->previous_diagnostic != NULL) {
        scope->failed = true;
        scope->pending = xrtErrorRef(scope->previous_diagnostic);
    }
    return true;
}

XL6_FOREIGN_API xl6_error_result xl6ErrorScopeEnd(xl6_error_scope* scope)
{
    xerror* diagnostic;
    xerror* failure;
    xl6_foreign_error_result result = { false, NULL };
    if (scope == NULL || xl6ForeignErrorTop() != scope) {
        result.error = xrtErrorCreate(XERR_STATE, "xlang", 0,
            "foreign call error scopes must end in stack order");
        return result;
    }
    /* Binding a previously installed scope cannot require a new heap object.
     * An OS binding failure is a runtime invariant failure: never continue
     * with a pointer to a stack frame that is about to disappear. */
    if (!xl6ForeignErrorSetTop(scope->previous)) abort();
    diagnostic = xrtTakeError();
    /* A failed step may have no diagnostic (e.g. allocation failure). The
     * failure bit, not a non-NULL pointer, latches the first outcome. Never
     * replace that outcome with a diagnostic from a later cleanup step. */
    failure = scope->failed ? scope->pending : diagnostic;
    result.ok = !scope->failed && failure == NULL;
    result.error = failure;
    if (scope->failed) xrtErrorFree(diagnostic);
    scope->pending = NULL;
    scope->previous = NULL;
    xrtSetErrorTake(scope->previous_diagnostic);
    scope->previous_diagnostic = NULL;
    return result;
}

XL6_FOREIGN_API bool xl6ForeignCallBegin(xl6_foreign_error_scope* scope)
{
    return xl6ErrorScopeBegin(scope);
}

XL6_FOREIGN_API xl6_foreign_error_result xl6ForeignCallEnd(xl6_foreign_error_scope* scope)
{
    return xl6ErrorScopeEnd(scope);
}

XL6_FOREIGN_API void xl6ErrorScopeCaptureFailure(xl6_error_scope* scope)
{
    xerror* error;
    /* This is a generated-program invariant. Continuing after a mismatched
     * scope would consume another transaction's diagnostic and lose errors. */
    if (scope == NULL || xl6ForeignErrorTop() != scope) abort();
    error = xrtTakeError();
    if (!scope->failed)
        scope->pending = error;
    else
        xrtErrorFree(error);
    scope->failed = true;
}

static void xl6ErrorScopeObserveFailure(xerror* original)
{
    xl6_error_scope* scope = NULL;
    if (xrtOnce(&xl6_foreign_error_once, xl6ForeignErrorKeyInit, NULL))
        scope = xl6ForeignErrorTop();
    if (scope != NULL && !scope->failed) {
        scope->failed = true;
        if (original != NULL)
            scope->pending = xrtErrorRef(original);
    }
}

XL6_FOREIGN_API void xl6ErrorScopePropagateFailure(xl6_error_result failure)
{
    if (failure.ok) abort();
    xl6ErrorScopeObserveFailure(failure.error);
    xrtSetErrorTake(failure.error);
}

XL6_FOREIGN_API void xl6ForeignCallbackFail(void)
{
    xerror* original = xrtTakeError();
    xerror* diagnostic;
    xerrordescview description;
    xerrorlocationview location;
    if (original == NULL)
        original = xrtErrorCreate(XERR_STATE, "xlang", 0,
            "language callback failed without an error");
    xl6ErrorScopeObserveFailure(original);
    if (original == NULL) return;
    description.Kind = xrtErrorKind(original);
    description.Code = xrtErrorCode(original);
    description.SystemCode = xrtErrorSystemCode(original);
    description.Domain = xrtErrorDomainView(original);
    description.Operation = (xstrview){"callback", 8};
    description.Message = xrtErrorMessageView(original);
    description.Data = xrtErrorDataView(original);
    description.Cause = xrtErrorCause(original);
    location.File = xrtErrorFileView(original);
    location.Line = xrtErrorLine(original);
    location.Column = xrtErrorColumn(original);
    diagnostic = xrtErrorBuildViewAt(&description, &location);
    /* Diagnostic allocation failure cannot replace a preserved language error. */
    xrtSetErrorTake(diagnostic != NULL ? diagnostic : xrtErrorRef(original));
    xrtErrorFree(original);
}
