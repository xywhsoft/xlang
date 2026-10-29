#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "xl6_closure.h"
#include "src/api/xl_foreign_error.h"

/* No callback, language edge or code pointer is retained by an initialized
 * Once. Only the synchronous call borrows a closure and enters its real lease. */
typedef struct XL6_OnceState { xonce once; } XL6_OnceState;
typedef struct XL6_OnceCall { XL6_Closure* closure; bool hasResult; } XL6_OnceCall;

void* xl6OnceCreateV1(void) { return xrtCalloc(1, sizeof(XL6_OnceState)); }
void xl6OnceDestroyV1(void* raw) { xrtFree(raw); }

static bool initialize(void* raw)
{
    XL6_OnceCall* call = raw;
    XL6_ClosureScope scope = {0};
    const XL6_ClosureRuntimeV1* runtime = xl6ClosureRuntimeV1Get();
    bool accepted = true;
    runtime->Enter(&scope, call->closure);
    bool invoked = call->hasResult ?
        ((bool (*)(void*, bool*))call->closure->invoke)(call->closure->environment, &accepted) :
        ((bool (*)(void*))call->closure->invoke)(call->closure->environment);
    /* A false language result means retry, not exception. A failed checked
     * invocation propagates independently of diagnostic allocation. Errors
     * caught inside the callback do not reach this path. */
    if (!invoked) xl6ForeignCallbackFail();
    runtime->Leave(&scope);
    return invoked && accepted;
}

static bool run(void* raw, XL6_Closure* callback, bool hasResult)
{
    if (!raw || !callback || !callback->invoke) {
        xrtSetErrorKind(XERR_ARGUMENT);
        return false;
    }
    XL6_OnceCall call = {callback, hasResult};
    return xrtOnce(&((XL6_OnceState*)raw)->once, initialize, &call);
}
void xl6OnceRunV1(void* raw, XL6_Closure* callback) { (void)run(raw, callback, false); }
bool xl6OnceTryRunV1(void* raw, XL6_Closure* callback) { return run(raw, callback, true); }
XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(xl6OnceHandlePolicyV1Get, xl6OnceDestroyV1)
