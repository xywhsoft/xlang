#ifndef XL6_TLS_SESSION_INTERNAL_H
#define XL6_TLS_SESSION_INTERNAL_H
#include "xl6_tls_role_context_internal.h"
/* One actual strong reference per language handle, native transport owner,
 * active entry or collector pin. The native session is uniquely owned here.
 * No raw native pointer escapes this private ABI. All sessions admitted by
 * the two role factories have no pool or external releaser. A typed server
 * selector owns its actual closure capability as an explicit outgoing edge. */
/* Permanent resident operations selected by the creating role. Keeping native
 * role entry points here lets shared observations avoid linking the opposite
 * handshake implementation. NULL optional operations mean not that role. */
typedef struct XL6_TlsSessionRoleOpsV1 {
    xtlsresult (*drive)(xtlssession*);
    xtlsresult (*keyUpdate)(xtlssession*,xtlskeyupdate);
    size_t (*certificateCount)(const xtlssession*);
    const xx509cert* (*certificate)(const xtlssession*,size_t);
    bool (*serverName)(const xtlssession*,xbytesview*);
    bool (*serverCookie)(const xtlssession*,uint64*);
} XL6_TlsSessionRoleOpsV1;
typedef struct XL6_TlsSessionV1 {
    volatile int32 refs;
    xatomic32 gate;
    xtlssession* native;
    const XL6_TlsSessionRoleOpsV1* role;
    XL6_TlsSelectorV1 selector;
    const void* claim;
    bool cleared;
} XL6_TlsSessionV1;
typedef struct XL6_TlsSessionScopeV1 { XL6_TlsSessionV1* owner; } XL6_TlsSessionScopeV1;
/* Consumes a private, certified static-role session on every path. Never use
 * this factory to certify arbitrary C sessions or borrowed callback state. */
XL6_TlsSessionV1* xl6TlsSessionTakeInternalV1(xtlssession*,const XL6_TlsSessionRoleOpsV1*);
XL6_TlsSessionV1* xl6TlsSessionTakeSelectorInternalV1(xtlssession*,const XL6_TlsSessionRoleOpsV1*,XL6_TlsSelectorV1);
bool xl6TlsSessionHoldInternalV1(const void*);
void xl6TlsSessionDropInternalV1(const void*);
bool xl6TlsSessionEnterInternalV1(XL6_TlsSessionV1*,bool,XL6_TlsSessionScopeV1*);
void xl6TlsSessionLeaveInternalV1(XL6_TlsSessionScopeV1*);
xrtownershipref xl6TlsSessionOwnershipV1(const void*);
const xrtownershipadapterv1* xl6TlsSessionOwnershipAdapterV1(xrtownershipref);
const xrtownershipquiescencev1* xl6TlsSessionQuiescenceV1Get(void);
const XL6_NativeHandlePolicyV1* xl6TlsSessionHandlePolicyV1Get(void);
void xl6TlsSessionDestroyV1(void*);
void xl6TlsSessionDisposeV1(void*);
bool xl6TlsSessionDisposedV1(void*);
/* Native entry gate must already be held. */
bool xl6TlsSessionPeerReadyInternalV1(const XL6_TlsSessionV1*);
void* xl6TlsSessionClientCreateV1(void*,xstrview,int64_t,xstrview,int64_t);
void* xl6TlsSessionServerCreateV1(void*);
#endif
