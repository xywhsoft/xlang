#ifndef XL6_FOREIGN_VIEW_H
#define XL6_FOREIGN_VIEW_H

#include "xl6_code_lease.h"
#include "xl6_ownership_catalog.h"
#include "xl6_foreign_callback_scope.h"
#include "xl6_foreign_context.h"
#include <stddef.h>
#include <stdint.h>

typedef struct XL6_ForeignView XL6_ForeignView;

/* Compiler/native-publisher contract, not a sandbox for arbitrary C pointers.
 * The copied descriptor and every transitive native dependency are protected
 * by the supplied code owner. NULL code owner means externally resident C.
 * Family names are exact canonical physical C identities within one native
 * package/runtime. Aliases and native subclasses share their root family.
 *
 * Watch is a nonallocating UI-thread attachment with zeroed stable storage.
 * It invokes no user code and retains no input on failure. Notification is
 * once, before native user destruction/free, after detaching the storage.
 * Unwatch is nonallocating and safe on detached zeroed storage. Watch and
 * Unwatch are either both present or both absent. An externally destroyable
 * root also needs native CallBegin/CallEnd physical-lifetime protection.
 *
 * Destroy returns true only after consuming the native owner. Failure that
 * leaves a native owner retains its observation for checked retry; a consumed
 * owner must already have delivered invalidation, even on failure. It never unwinds
 * across C. CallEnd and Destroy can run native/language user code; no mutation
 * scope, collector Freeze or queue lock may enclose them.
 *
 * TraceRaw describes ALL actual refcounted owning slots in a root's uniquely
 * owned C payload, including its unique internal allocations. Independently
 * shared native nodes remain separate references and need their own admission.
 * Child views borrow this payload; they do not duplicate its owning edges.
 * TraceRaw is read-only under whole-domain Freeze, with no allocation, locks,
 * language invocation, reference transitions or policy guessing.
 * Busy is a creator-thread, frozen read-only probe of independent native
 * activity. An observed root requires it: the resident active count alone
 * cannot see operations begun by an external C event loop. */
typedef struct XL6_ForeignViewDescriptorV1 {
    size_t size;
    uint64_t abiTag;
    const char* family;
    size_t watchBytes, watchAlignment;
    bool (*Watch)(void* raw,void* storage,void (*invalidate)(void*),void* user);
    void (*Unwatch)(void* storage);
    bool (*Destroy)(void* raw);
    size_t callBytes, callAlignment;
    bool (*CallBegin)(void* root,void* storage);
    bool (*CallEnd)(void* storage);
    bool (*TraceRaw)(const void*,XL6_ModuleOwnershipVisit,void*);
    bool (*Busy)(const void*);
} XL6_ForeignViewDescriptorV1;
#define XL6_FOREIGN_VIEW_V1_TAG UINT64_C(0x584C364656494531)

/* Stable compiler-owned storage; never copy a live call or callback scope.
 * nativeStorage is a separately typed C record emitted from the declaration,
 * not a guessed/fixed-capacity byte buffer. The root's native CallEnd may
 * destroy its whole payload. After it returns, resident code releases pins. */
typedef struct XL6_ForeignViewCallV1 {
    struct XL6_ForeignViewCallV1* self;
    XL6_ForeignView* view;
    XL6_ForeignView* root;
    void* raw;
    void* nativeStorage;
    XL6_ForeignCallContextV1 execution;
} XL6_ForeignViewCallV1;

/* Failure never consumes raw. BorrowChild requires a current successful call
 * on the same creator thread/root; repeated exposure reuses its live control
 * record. Neither ordinary copying nor an existing lookup allocates a wrapper.
 * All language owning references keep the root/context alive. */
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewCreateRootV1(void*,const XL6_ForeignViewDescriptorV1*,const XL6_CodeOwner*);
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewBorrowChildV1(void*,XL6_ForeignView*,const XL6_ForeignViewDescriptorV1*,const XL6_CodeOwner*);
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewRetainV1(XL6_ForeignView*);
/* Consumes its language reference on success AND checked cleanup failure.
 * A void native drop adapter must propagate a false result's separate failure
 * bit, including when no diagnostic could be allocated. */
XL6_CODE_LEASE_API bool xl6ForeignViewReleaseV1(XL6_ForeignView*);
XL6_CODE_LEASE_API bool xl6ForeignViewCallBeginV1(XL6_ForeignView*,XL6_ForeignViewCallV1*,void*,size_t);
XL6_CODE_LEASE_API bool xl6ForeignViewCallEndV1(XL6_ForeignViewCallV1*);
XL6_CODE_LEASE_API bool xl6ForeignViewCloseV1(XL6_ForeignView*);
/* Internal native binding owner: keeps the control/code readable without
 * creating a language owning value or suppressing default close. A retained
 * callback uses this for its implicit context, not a hidden strong language
 * self-reference. It is still a REAL graph edge and must be traced. */
XL6_CODE_LEASE_API XL6_ForeignView* xl6ForeignViewNativePinV1(XL6_ForeignView*);
XL6_CODE_LEASE_API void xl6ForeignViewNativeUnpinV1(XL6_ForeignView*);

/* Native invalidation only clears/queues; it cannot release the final code
 * dependency while the native notifier is still on the C stack. Drain runs
 * on the creator thread after an entire foreign call returns, or explicitly
 * from the host outside native callbacks. Wrong-thread cleanup stays queued
 * for checked recovery. A callback boundary itself never drains work. */
XL6_CODE_LEASE_API bool xl6ForeignViewDrainV1(void);
/* Read-only queue observation; thread==0 counts all creator threads. */
XL6_CODE_LEASE_API size_t xl6ForeignViewPendingV1(uint64_t thread);
/* Freeze-only physical anchors, not additional owning references. */
XL6_CODE_LEASE_API bool xl6ForeignViewDiscoverV1(const XL6_CodeOwner*,XL6_ModuleOwnershipVisit,void*);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignViewV1Get(void);
XL6_CODE_LEASE_API struct xrtownershipref xl6ForeignViewOwnershipV1(const XL6_ForeignView*);

#endif
