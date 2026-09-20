#ifndef XL6_CLOSURE_H
#define XL6_CLOSURE_H
#include <stdint.h>
#include "xl6_code_lease.h"

/* Private, versioned physical closure ABI. Generated code may read the fixed
 * invocation/parameter fields, but only the resident runtime creates, retains
 * and destroys this record. Descriptors and callbacks are protected by lease;
 * environment storage is uniquely owned and traced as real outgoing slots.
 * Resident create/ref/release/box and scope-edge transitions participate in
 * the XRT mutation domain. Arbitrary environment mutation and generated calls
 * are not made quiescent by that fact; their storage needs its own protocol. */
typedef struct xlir_call_default {
    int abi; bool boolean_value; int64_t i64_value; uint64_t u64_value;
    double f64_value; const char* string_value;
} xlir_call_default;
typedef struct xlir_closure XL6_Closure;
struct xlir_closure {
    volatile int32_t refs;
    void (*invoke)(void);
    void (*raw_callback)(void);
    void* environment;
    void (*environment_free)(void*);
    const char* const* call_parameter_names;
    const xlir_call_default* call_parameter_defaults;
    size_t call_parameter_count;
    XL6_CodeContextTrace environment_trace;
    XL6_CodeLease* lease;
};
typedef struct XL6_ClosureScope {
    XL6_CodeScope code;
    XL6_Closure* closure;
} XL6_ClosureScope;
struct xrtcallframe;
struct xrtcallresult;
typedef bool (*XL6_ClosureBoxEntry)(void*, const struct xrtcallframe*, struct xrtcallresult*);
typedef bool (*XL6_ClosurePayloadTrace)(const void*, const void*, XL6_ModuleOwnershipVisit, void*);
typedef struct XL6_ClosureRuntimeV1 {
    /* Failure leaves environment owned by caller. The independent entry lease
     * must survive acquisition AND rollback. NULL owner is externally resident,
     * never an inference from an empty capture list or a missing destructor. */
    XL6_Closure* (*Create)(const XL6_CodeOwner*, void (*)(void), void (*)(void),
        void*, void (*)(void*), const char* const*, const xlir_call_default*, size_t, XL6_CodeContextTrace);
    XL6_Closure* (*Retain)(XL6_Closure*);
    void (*Release)(XL6_Closure*);
    struct xrtownershipref (*Ownership)(const XL6_Closure*);
    void (*Enter)(XL6_ClosureScope*, XL6_Closure*);
    void (*Leave)(XL6_ClosureScope*);
    void (*RetainEnvironment)(void*);
    void (*ReleaseEnvironment)(void*);
    void (*PayloadDrop)(void*, void*);
    XL6_ClosurePayloadTrace PayloadTrace;
    /* The adapter's code owner can differ from the closure's owner. Box owns
     * both dependencies and invokes/drops through resident outermost entries. */
    struct xvalue* (*Box)(const XL6_CodeOwner*, XL6_Closure*, XL6_ClosureBoxEntry, uint64_t);
} XL6_ClosureRuntimeV1;
/* Return actual resident callback addresses, not producer-side PE import
 * thunks. The embedding runtime must outlive every accepted closure/box. */
XL6_CODE_LEASE_API const XL6_ClosureRuntimeV1* xl6ClosureRuntimeV1Get(void);

/* V2 replaces producer-owned trace/drop callbacks with a closed physical
 * capture layout. The immutable descriptor has the same lifetime as invoke;
 * Create owns neither input on failure. Captures are immutable after creation:
 * shared mutable captures use resident cells, never environment field writes.
 * TRIVIAL is a verified non-owning carrier, not an opaque native owner. */
typedef enum XL6_ClosureCaptureKind {
    XL6_CLOSURE_CAPTURE_TRIVIAL = 1,
    XL6_CLOSURE_CAPTURE_STRING,
    XL6_CLOSURE_CAPTURE_XVALUE,
    XL6_CLOSURE_CAPTURE_FUTURE,
    XL6_CLOSURE_CAPTURE_CLOSURE,
    XL6_CLOSURE_CAPTURE_CELL
} XL6_ClosureCaptureKind;
typedef struct XL6_ClosureCaptureSlot {
    XL6_ClosureCaptureKind kind;
    size_t offset, bytes;
} XL6_ClosureCaptureSlot;
typedef struct XL6_ClosureEnvironmentV1 {
    size_t size, bytes, count;
    const XL6_ClosureCaptureSlot* slots;
} XL6_ClosureEnvironmentV1;
typedef struct XL6_ClosureRuntimeV2 {
    XL6_Closure* (*Create)(const XL6_CodeOwner*, void (*)(void), void (*)(void),
        void*, const XL6_ClosureEnvironmentV1*, const char* const*, const xlir_call_default*, size_t);
    /* Consuming rollback for an initialized, validated V2 environment. Caller
     * keeps its code owner alive across the entire operation. */
    void (*DropEnvironment)(const XL6_ClosureEnvironmentV1*, void*);
    /* Borrow a callable Value and construct a one-XVALUE environment. The
     * invocation entry receives a pointer to that owned xvalue* slot. */
    XL6_Closure* (*CreateValue)(const XL6_CodeOwner*, void (*)(void), const struct xvalue*);
} XL6_ClosureRuntimeV2;
XL6_CODE_LEASE_API const XL6_ClosureRuntimeV2* xl6ClosureRuntimeV2Get(void);
struct xrtownershipadapterv1;
/* Identity checked before payload access. V1 arbitrary callbacks are refused;
 * callers must still admit every child before inspecting its graph. */
XL6_CODE_LEASE_API const struct xrtownershipadapterv1* xl6ClosureOwnershipAdapterV1(struct xrtownershipref);
#endif
