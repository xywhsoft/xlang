#ifndef XL6_FOREIGN_ERROR_ABI
#define XL6_FOREIGN_ERROR_ABI

#include <stdbool.h>
typedef struct xerror xerror;

/* Shared runtime ABI, never module-local generated TLS. A synchronous foreign
 * CALL owns one stack scope. End returns an explicit success flag and an owned
 * error (possibly NULL on allocation failure), restoring the pre-call
 * diagnostic independently. The caller must consume the returned error even
 * if argument cleanup changes the XRT diagnostic slot.
 * Scopes cannot be copied, suspended across execution contexts, or ended out
 * of order. Each successful Begin requires exactly one End. */
typedef struct xl6_foreign_error_scope {
    struct xl6_foreign_error_scope* previous;
    xerror* pending;
    xerror* previous_diagnostic;
    bool failed;
} xl6_foreign_error_scope;
typedef struct xl6_foreign_error_result {
    bool ok;
    xerror* error;
} xl6_foreign_error_result;

/* Cleanup and foreign calls nest on ONE execution-context-local boundary
 * stack. A second independent cleanup TLS stack would publish failures past
 * an intervening language catch. Keep the original ABI names as aliases for
 * existing generated modules; new cleanup programs use the neutral names. */
typedef xl6_foreign_error_scope xl6_error_scope;
typedef xl6_foreign_error_result xl6_error_result;

#ifndef XL6_FOREIGN_API
#if defined(_WIN32) && (defined(XL_BUILD_DLL) || defined(XL6_BUILD_SHARED))
#define XL6_FOREIGN_API __declspec(dllexport)
#elif defined(__GNUC__) && !defined(_WIN32)
#define XL6_FOREIGN_API __attribute__((visibility("default")))
#else
#define XL6_FOREIGN_API
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

XL6_FOREIGN_API bool xl6ErrorScopeBegin(xl6_error_scope* scope);
/* Finalization joins an already escaping outcome instead of handling it.
 * Inherit the immediate parent's first failure (including a NULL diagnostic),
 * otherwise the incoming diagnostic. knownFailure marks a caller's explicit
 * failed carrier when no parent scope/diagnostic can represent that failure.
 * End still returns an owned result; only the boundary publishes it. */
XL6_FOREIGN_API bool xl6ErrorScopeBeginCleanup(xl6_error_scope* scope, bool knownFailure);
XL6_FOREIGN_API xl6_error_result xl6ErrorScopeEnd(xl6_error_scope* scope);

/* Called after an explicit failed cleanup step, with this scope on top.
 * Consumes its diagnostic into the first-error accumulator and clears the
 * diagnostic slot so subsequent steps execute independently. A failed step
 * without an allocated error still sets the separate failure bit and latches
 * a NULL first diagnostic: a later error must not replace the first failure.
 * Every step must still run: this operation never performs control transfer. */
XL6_FOREIGN_API void xl6ErrorScopeCaptureFailure(xl6_error_scope* scope);

/* Consumes a failed result at a void native callback boundary (e.g. an object
 * finalizer). Publishes the ORIGINAL diagnostic and propagates failure only
 * to the immediate enclosing scope. Do not use on an ordinary language error
 * edge: its catch/cleanup CFG owns the result until it actually escapes. */
XL6_FOREIGN_API void xl6ErrorScopePropagateFailure(xl6_error_result failure);

XL6_FOREIGN_API bool xl6ForeignCallBegin(xl6_foreign_error_scope* scope);
XL6_FOREIGN_API xl6_foreign_error_result xl6ForeignCallEnd(xl6_foreign_error_scope* scope);

/* Called only when a verified language callback returns failure, not whenever
 * language code raises: errors caught inside the callback must not escape.
 * Retains the first original error in the immediate error scope. Publishes a
 * separate callback diagnostic with the original payload, source location and
 * cause; native code may consume that diagnostic without handling the language
 * exception. With no scope, only the native diagnostic is published. */
XL6_FOREIGN_API void xl6ForeignCallbackFail(void);

#ifdef __cplusplus
}
#endif

#endif
