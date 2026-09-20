#ifndef XL6_FUTURE_PAYLOAD_H
#define XL6_FUTURE_PAYLOAD_H
#include <stdbool.h>
struct xpromise;
struct xerror;
struct xrtownershipref;
#ifndef XL6_FUTURE_PAYLOAD_API
#if defined(_WIN32) && defined(XL_BUILD_DLL)
#define XL6_FUTURE_PAYLOAD_API __declspec(dllexport)
#else
#define XL6_FUTURE_PAYLOAD_API
#endif
#endif
/* Resident Future box destructors, selected by verified AsyncResultAbi.
 * Each consumes the box and its one owned element; context is unused.
 * DropValue consumes only the box (trivial/tagged/non-owning pointer ABI).
 * No descriptor or callback in the producing module is needed for these
 * operations. This does NOT make a contained closure/class/native finalizer
 * resident: code-bearing object identity has its own lifetime contract.
 * The embedding runtime and its XRT instance outlive every accepted box. */
typedef enum XL6_FuturePayloadKind {
    XL6_FUTURE_PAYLOAD_INVALID = 0,
    XL6_FUTURE_PAYLOAD_VALUE,
    XL6_FUTURE_PAYLOAD_STRING,
    XL6_FUTURE_PAYLOAD_XVALUE,
    XL6_FUTURE_PAYLOAD_ERROR,
    XL6_FUTURE_PAYLOAD_FUTURE,
    XL6_FUTURE_PAYLOAD_CLOSURE,
    XL6_FUTURE_PAYLOAD_GENERATOR,
    XL6_FUTURE_PAYLOAD_COUNT
} XL6_FuturePayloadKind;
typedef void (*XL6_FuturePayloadDrop)(void*, void*);
/* Obtain the actual resident address, not a function address which the linker
 * may implement as an import thunk in the unloadable producer (notably PE).
 * Invalid kinds return NULL; verified producers never select invalid kinds. */
XL6_FUTURE_PAYLOAD_API XL6_FuturePayloadDrop xl6PayloadDropGet(XL6_FuturePayloadKind);
/* Compatible with XRT's xfutureownershiptrace without importing a full
 * implementation header into every generated unit. The actual resident
 * address is also needed at task submission, before any worker can run. */
typedef bool (*XL6_FuturePayloadTrace)(const void*, const void*,
    bool (*)(struct xrtownershipref, void*), void*);
XL6_FUTURE_PAYLOAD_API XL6_FuturePayloadTrace xl6PayloadTraceGet(XL6_FuturePayloadKind);
/* Publish the matching resident destructor and physical ownership adapter
 * atomically. A failed call does not consume the box. No callback descriptor
 * in the producing module is needed for these resident payload kinds. */
XL6_FUTURE_PAYLOAD_API bool xl6PayloadResolveOwned(struct xpromise*, void*, XL6_FuturePayloadKind);
struct xrtownershipadapterv1;
struct xfuturepayloadownershipv1;
/* Actual immutable resident policy, also selected before executor admission.
 * Invalid kinds return NULL without changing the diagnostic. */
XL6_FUTURE_PAYLOAD_API const struct xfuturepayloadownershipv1* xl6PayloadPolicyGet(XL6_FuturePayloadKind);
/* Admit the physical Future only for this closed resident payload family.
 * This authorizes neither a contained class/closure nor a pending waiter;
 * the enclosing resolver must independently admit every physical child. */
XL6_FUTURE_PAYLOAD_API const struct xrtownershipadapterv1* xl6PayloadOwnershipAdapterV1(struct xrtownershipref);
/* Observe the stable XRT Future state discriminator before reading a payload.
 * Success clears the current diagnostic; failure borrows/retains its error.
 * Cancellation has the language's canonical diagnostic. If allocating that
 * diagnostic fails, the allocation-free XRT cancellation kind still wins, so
 * observation cannot turn cancellation into a failed Future. */
XL6_FUTURE_PAYLOAD_API bool xl6PayloadObserveOutcome(int, const struct xerror*);
#endif
