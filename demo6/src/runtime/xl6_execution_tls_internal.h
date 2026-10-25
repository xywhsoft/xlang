#ifndef XL6_EXECUTION_TLS_INTERNAL_H
#define XL6_EXECUTION_TLS_INTERNAL_H
#include "xl6_execution_resource_internal.h"
#include "xl6_execution_net_engine.h"
#include "xl6_execution_tls.h"
#include "xl6_tls_metadata_role.h"
#include "xl6_tls_selector_owner.h"
#include <stdlib.h>
/* All builders are call-only, trusted resident functions. They consume the
 * supplied Data credit iff they return a native owned Stream; they never
 * retain options or generated callbacks. There is no language raw importer. */
typedef struct XL6_TlsStreamCreateOptionsV1 {
    XL6_ExecutionResourceV1* engine;
    xtlsstream* accepted; /* private, actual owned Listener/Dial result borrow */
    XL6_ExecutionResourceV1* accepting; /* prepared real accept, no submission yet */
    XL6_ExecutionResourceV1* resolving; /* real Resolver parent for a prepared Dial */
    bool dialing; /* preparation submits nothing */
    const XL6_TlsMetadataRoleV1* metadata; /* permanent actual creator's role */
    xtlsstream* (*Build)(xnetengine*,XL6_ExecutionResourceV1*,const xtlsstreamownershipv1*,const struct XL6_TlsStreamCreateOptionsV1*);
    const struct XL6_TlsClientConnectOptionsV1* client;
} XL6_TlsStreamCreateOptionsV1;
typedef struct XL6_TlsListenerCreateOptionsV1 {
    XL6_ExecutionResourceV1* engine;
    const struct xtlslistenerconfig* config; /* copied synchronously */
    const XL6_TlsMetadataRoleV1* metadata;
    XL6_TlsSelectorV1 selector; /* call borrow; provider retains independently */
} XL6_TlsListenerCreateOptionsV1;

typedef enum XL6_TlsPendingKindV1 {
    XL6_TLS_PENDING_NONE_V1,XL6_TLS_PENDING_ACCEPT_V1,XL6_TLS_PENDING_DIAL_V1
} XL6_TlsPendingKindV1;
typedef struct XL6_TlsStreamResourceV1 {
    XL6_ExecutionResourceV1 base;
    XL6_ExecutionResourceV1 *parent,*accepting,*resolving;
    xatomicptr native,pending;
    xatomic32 prepared;
    XL6_TlsPendingKindV1 pendingKind; /* immutable after construction */
    const XL6_TlsMetadataRoleV1* metadata; /* immutable, nonowning resident code */
} XL6_TlsStreamResourceV1;
XL6_EXECUTION_INTERNAL bool xl6ExecutionTlsPromotePendingV1(XL6_ExecutionResourceV1*,XL6_TlsPendingKindV1);
typedef struct XL6_TlsScopeV1 {
    XL6_ExecutionScopeV1 execution;
    xtlsstream* stream;
    struct xtlslistener* listener;
    XL6_ExecutionResourceV1* engineOwner; /* scoped borrow only */
    const XL6_TlsMetadataRoleV1* metadata;
} XL6_TlsScopeV1;
XL6_EXECUTION_INTERNAL bool xl6ExecutionTlsStreamEnterV1(XL6_ExecutionResourceV1*,bool,XL6_TlsScopeV1*);
XL6_EXECUTION_INTERNAL bool xl6ExecutionTlsListenerEnterV1(XL6_ExecutionResourceV1*,bool,XL6_TlsScopeV1*);
static inline void xl6ExecutionTlsLeaveV1(XL6_TlsScopeV1* scope)
{xl6ExecutionResourceLeaveV1(&scope->execution);scope->stream=NULL;scope->listener=NULL;scope->engineOwner=NULL;scope->metadata=NULL;}
static inline void xl6ExecutionTlsParentDropV1(XL6_ExecutionResourceV1** slot)
{
    xrtownershipscope mutation={0};if(!xrtOwnershipMutationBegin(&mutation))abort();
    XL6_ExecutionResourceV1* parent=*slot;*slot=NULL;
    if(!xrtOwnershipScopeEnd(&mutation))abort();
    xl6ExecutionResourceReleaseV1(parent);
}
static inline bool xl6ExecutionTlsEngineEnterV1(XL6_ExecutionResourceV1* engine,XL6_ExecutionScopeV1* scope)
{
    if(!xl6ExecutionResourceEnterV1(engine,true,scope))return false;
    if(engine->family!=xl6ExecutionNetEngineFamilyV1Get()||!scope->engine){
        xl6ExecutionResourceLeaveV1(scope);xrtSetErrorKind(XERR_TYPE);return false;}
    return true;
}
#endif
