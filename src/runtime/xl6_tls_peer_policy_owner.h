#ifndef XL6_TLS_PEER_POLICY_OWNER_H
#define XL6_TLS_PEER_POLICY_OWNER_H
#include "include/xruntime_decl.h"
/* Installed only by a trusted typed producer. One real owner reference is
 * retained, traced and dropped; no borrowed/generated descriptor is kept.
 * Verify is an ADDITIONAL policy after the native chain/name checks succeed.
 * The immutable operation table and code outlive every consumer/collector pin. */
typedef struct XL6_TlsPeerPolicyOpsV1 {
    void* (*Ref)(void*);
    void (*Drop)(void*);
    xrtownershipref (*Ownership)(const void*);
    bool (*Verify)(void*,const struct xtlsverifiedpeer*);
} XL6_TlsPeerPolicyOpsV1;
typedef struct XL6_TlsPeerPolicyV1 {
    void* data;
    const XL6_TlsPeerPolicyOpsV1* ops;
} XL6_TlsPeerPolicyV1;
#endif
