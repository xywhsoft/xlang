#ifndef XL6_NATIVE_HANDLE_H
#define XL6_NATIVE_HANDLE_H
#include "xl6_code_lease.h"
#include <stddef.h>
#include <stdlib.h>

struct xvalue;
struct xvaluehandleops;
struct xrtownershipref;

/* Immutable, resident resource declaration. These identities are not a
 * universal handle certificate: the host admits each audited policy by exact
 * Ops AND Trace identity, before running any user-supplied graph callback.
 * Payload callbacks and every transitive native dependency must stay resident.
 * Trace describes ALL owning language edges; it does not authorize children.
 * No policy or generated module address is retained as handle UserData. */
typedef struct XL6_NativeHandlePolicyV1 {
    size_t size;
    uint64_t abiTag;
    const struct xvaluehandleops* Ops;
    bool (*Trace)(const struct xvalue*,
        bool (*)(struct xrtownershipref, void*), void*);
} XL6_NativeHandlePolicyV1;
#define XL6_NATIVE_HANDLE_POLICY_V1_TAG UINT64_C(0x584C364E484E4431)

/* A valid resident policy consumes and clears the independent private slot on
 * both success and allocation/bind failure. Rollback retires its real payload
 * exactly once and preserves the first failure, even without a diagnostic.
 * Invalid slot/policy is a caller contract error and leaves the slot unchanged
 * (there is no trusted destructor with which to consume an unknown policy).
 * No enclosing ownership mutation may span this call's arbitrary cleanup. */
XL6_CODE_LEASE_API struct xvalue* xl6NativeHandleTakeV1(
    void** slot, const XL6_NativeHandlePolicyV1* policy);
XL6_CODE_LEASE_API bool xl6NativeHandleTraceLeaf(const struct xvalue*,
    bool (*)(struct xrtownershipref, void*), void*);

/* These three private native payloads have no owning language edges. Commands
 * hold owned text/config/duplicated OS capabilities, live processes hold native
 * XRT resources, statuses hold plain data. None retains a generated callback.
 * This list does NOT certify custom IO, arbitrary extensions or other handles. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessCommandHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessStatusHandlePolicyV1Get(void);
/* An iterator owns only a copied byte allocation, never its source Buffer. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6BufferIteratorHandlePolicyV1Get(void);
/* The actual XRT buffer contains Data/Size/Capacity, not language values. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6BufferHandlePolicyV1Get(void);
/* XL_Context owns only native configuration/OS libraries. This does not
 * certify XLC_Context, XlangHandler, borrowed callback data or extensions. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6XlangContextHandlePolicyV1Get(void);

/* Prototypes are physical SDK declarations. The .inc export table belongs
 * to resident source units, never to a generated header's sibling closure. */
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6DateTimeHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6UnicodeValidatorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6TextIteratorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternMatchHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternBuilderHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternEditHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternKeyHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessPipelineHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PipelineResultHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessStageHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessResultHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexMatchHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexIteratorHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexSplitHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexSplitPartHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexSetHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RegexReplacerHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6SignalQueueHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6SignalEventHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6PatternPublicationHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6ProcessRunOptionsHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6RandomStateHandlePolicyV1Get(void);
XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* xl6FsFileHandlePolicyV1Get(void);

/* Used only in audited resident implementation units, with complete XRT types.
 * Keep the same explicit error boundary as a generated native destructor; a
 * callback must propagate cleanup failure without replacing an earlier one. */
#define XL6_DEFINE_NATIVE_HANDLE_POLICY(Getter, Destroy, TraceCallback) \
    static void Getter##Drop(void* payload, void* userData) { \
        xl6_error_scope cleanup = {0}; xl6_error_result outcome; \
        (void)userData; \
        if (!xl6ErrorScopeBeginCleanup(&cleanup, false)) abort(); \
        Destroy(payload); \
        outcome = xl6ErrorScopeEnd(&cleanup); \
        if (!outcome.ok) xl6ErrorScopePropagateFailure(outcome); \
    } \
    XL6_CODE_LEASE_API const XL6_NativeHandlePolicyV1* Getter(void) { \
        static const struct xvaluehandleops ops = {NULL, Getter##Drop, NULL, NULL}; \
        static const XL6_NativeHandlePolicyV1 policy = {sizeof(policy), \
            XL6_NATIVE_HANDLE_POLICY_V1_TAG, &ops, TraceCallback}; \
        return &policy; \
    }
#define XL6_DEFINE_NATIVE_LEAF_HANDLE_POLICY(Getter, Destroy) \
    XL6_DEFINE_NATIVE_HANDLE_POLICY(Getter, Destroy, xl6NativeHandleTraceLeaf)
#endif
