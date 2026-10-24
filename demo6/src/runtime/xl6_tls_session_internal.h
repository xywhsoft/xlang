#ifndef XL6_TLS_SESSION_INTERNAL_H
#define XL6_TLS_SESSION_INTERNAL_H
#include "xl6_tls_role_context_internal.h"
/* One actual strong reference per language handle, native transport owner,
 * active entry or collector pin. The native session is uniquely owned here.
 * No raw native pointer escapes this private ABI. All sessions admitted by
 * the two role factories have no pool, external releaser or language callback;
 * future callback/transport producers must trace their own real owners. */
typedef struct XL6_TlsSessionV1 {
    volatile int32 refs;
    xatomic32 gate;
    xtlssession* native;
    /* Permanent role-selected resident entry, never generated/user code. */
    xtlsresult (*drive)(xtlssession*);
    const void* claim;
    bool cleared;
} XL6_TlsSessionV1;
typedef struct XL6_TlsSessionScopeV1 { XL6_TlsSessionV1* owner; } XL6_TlsSessionScopeV1;
/* Consumes a private, certified static-role session on every path. Never use
 * this factory to certify arbitrary C sessions or borrowed callback state. */
XL6_TlsSessionV1* xl6TlsSessionTakeInternalV1(xtlssession*,xtlsresult (*)(xtlssession*));
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
void* xl6TlsSessionClientCreateV1(void*,xstrview,int64_t,xstrview,int64_t);
void* xl6TlsSessionServerCreateV1(void*);
#endif
