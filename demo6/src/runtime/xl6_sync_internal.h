#ifndef XL6_SYNC_INTERNAL_H
#define XL6_SYNC_INTERNAL_H
/* Private resident-provider ABI, not host registration or a language callback
 * API. Only the three compiled native providers construct this prefix. Their
 * physical Unit/Unlock functions and ops are resident, with no source-module
 * semantic callback, owner data or extensible admission registration. */
#include "src/runtime/xl6_sync.h"
#include "src/api/xl_foreign_error.h"
#include <stdatomic.h>
#include <stdlib.h>

typedef struct XL6_SyncOwner {
    volatile int32_t refs;
    _Atomic(uint32_t) gate;
    uint64_t identity, token;
    unsigned int kind, errorKind;
    size_t active;
    const void* claim;
    bool initialized, cleared, finishing, pending;
    struct XL6_SyncOwner* next;
    bool (*unit)(struct XL6_SyncOwner*);
    bool (*unlock)(struct XL6_SyncOwner*,unsigned int);
} XL6_SyncOwner;

#if defined(__GNUC__) && !defined(_WIN32)
#define XL6_SYNC_INTERNAL __attribute__((visibility("hidden")))
#else
#define XL6_SYNC_INTERNAL
#endif
XL6_SYNC_INTERNAL struct xvalue* xl6SyncOwnerWrapTakeNative(XL6_SyncOwner**);
XL6_SYNC_INTERNAL XL6_SyncOwner* xl6SyncOwnerBeginNative(const struct xvalue*,uint64_t,unsigned int,bool);
XL6_SYNC_INTERNAL void xl6SyncOwnerEndNative(XL6_SyncOwner*);
XL6_SYNC_INTERNAL void xl6SyncOwnerDropNative(void*,void*);

/* Only the provider supplies fixed native storage and kernels. The common
 * private handle ops authenticate before a method checks immutable kind and
 * casts this prefix. Helpers are hidden, never exported/JIT-registered. */
#define XL6_SYNC_CREATE(Name,Kind,OwnerType,Init,Unit,Unlock) \
static bool nativeUnit(XL6_SyncOwner* base) { return Unit(&((OwnerType*)base)->storage); } \
struct xvalue* xl6##Name##CreateV1(uint64_t identity) { \
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort(); \
    OwnerType* owner=NULL; struct xvalue* value=NULL; \
    if (!identity) xrtSetErrorKind(XERR_ARGUMENT); \
    else if ((owner=xrtCalloc(1,sizeof(*owner)))!=NULL) { \
        owner->base.refs=1; owner->base.identity=identity; owner->base.kind=Kind; \
        owner->base.unit=nativeUnit; owner->base.unlock=Unlock; atomic_init(&owner->base.gate,0); \
        owner->base.initialized=Init(&owner->storage); \
        if (owner->base.initialized) { \
            XL6_SyncOwner* base=&owner->base; value=xl6SyncOwnerWrapTakeNative(&base); \
            owner=(OwnerType*)base; \
        } \
    } \
    if (!value) xl6ErrorScopeCaptureFailure(&work); \
    if (owner) xl6SyncOwnerDropNative(owner,NULL); \
    xl6_error_result result=xl6ErrorScopeEnd(&work); \
    if (!result.ok) { xl6ErrorScopePropagateFailure(result); xrtValueRelease(value); return NULL; } \
    return value; \
}
#define XL6_SYNC_METHOD(Name,Kind,OwnerType,Operation,Unlock) \
bool xl6##Name##V1(const struct xvalue* value,uint64_t identity) { \
    xl6_error_scope work={0}; if (!xl6ErrorScopeBegin(&work)) abort(); \
    XL6_SyncOwner* base=xl6SyncOwnerBeginNative(value,identity,Kind,Unlock); \
    bool ok=base && Operation(&((OwnerType*)base)->storage); \
    xl6SyncOwnerEndNative(base); \
    xl6_error_result result=xl6ErrorScopeEnd(&work); xrtErrorFree(result.error); return ok; \
}
#endif
