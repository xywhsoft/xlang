#include "xl6_text_value.h"
#include "xl6_native_handle.h"
#include "src/api/xl_foreign_error.h"

/* Source and read-only Token own independent actual XRT references. They do
 * not own registrations: the native token list only indexes registrations.
 * Parent references are traced by XRT, never guessed as leaf pointers. */
void* xl6CancellationSourceCreateV1(void) { return xrtCancelCreate(); }
void* xl6CancellationSourceChildV1(void* parent) { return xrtCancelChild(parent); }
void xl6CancellationSourceDestroyV1(void* source) { xrtCancelDestroy(source); }
bool xl6CancellationSourceCancelV1(void* source) { return xrtCancelRequest(source); }
bool xl6CancellationSourceRequestedV1(void* source) { return xrtCancelRequested(source); }
void* xl6CancellationTokenCreateV1(void* source) { return xrtCancelRef(source); }
void xl6CancellationTokenDestroyV1(void* token) { xrtCancelDestroy(token); }
bool xl6CancellationTokenRequestedV1(void* token) { return xrtCancelRequested(token); }
void xl6CancellationTokenThrowV1(void* token)
{ if (xrtCancelRequested(token)) xrtSetErrorKind(XERR_CANCELLED); }

static bool trace(const xvalue* value, xrtownershipvisitor visitor, void* context)
{
    void* raw = NULL;
    return visitor && xrtValueGetHandle(value, &raw, NULL, NULL) && raw &&
        visitor(xrtCancelOwnership(raw), context);
}
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6CancellationSourceHandlePolicyV1Get, xl6CancellationSourceDestroyV1, trace)
XL6_DEFINE_NATIVE_HANDLE_POLICY(xl6CancellationTokenHandlePolicyV1Get, xl6CancellationTokenDestroyV1, trace)
