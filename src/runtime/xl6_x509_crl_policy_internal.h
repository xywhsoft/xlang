#ifndef XL6_X509_CRL_POLICY_INTERNAL_H
#define XL6_X509_CRL_POLICY_INTERNAL_H
#include "xl6_x509_crl_profiles_internal.h"

/* These are resident implementation contracts, not the public language ABI.
 * Native views never outlive their owners. No closure is stored in a record. */
enum {
    XL6_X509_REVOCATION_UNKNOWN_V1=0,
    XL6_X509_REVOCATION_GOOD_V1=1,
    XL6_X509_REVOCATION_REVOKED_V1=2,
    XL6_X509_REVOCATION_REMOVED_V1=3,
    XL6_X509_CRL_STRICT_FLAGS_V1=31
};
typedef struct XL6_X509ValidatedCrlV1 {
    xatomic64 references;
    xx509crl crl;
    xx509cert issuer;
    xx509crlvalid value;
    xtime time;
    uint64 skew;
    uint32 flags;
    uint8 data[];
} XL6_X509ValidatedCrlV1;
typedef struct XL6_X509CrlSetV1 {
    xx509crlset value;
    XL6_X509ValidatedCrlV1* base;
    XL6_X509ValidatedCrlV1* delta;
} XL6_X509CrlSetV1;
typedef struct XL6_X509RevocationV1 {
    xatomic64 references;
    bool observed;
    xx509crlversion version;
    xx509revocation value;
} XL6_X509RevocationV1;
typedef struct XL6_X509RevocationTargetV1 {
    xatomic64 references;
    xx509cert value;
    uint8 data[];
} XL6_X509RevocationTargetV1;

struct XL6_X509PathV1;
/* Actual immutable evidence, not a claimed status/issuer token. Shared prefixes
 * retain every applicable validated source and optional real signer path. */
typedef struct XL6_X509RevocationEvidenceV1 {
    xatomic64 references;
    struct XL6_X509RevocationEvidenceV1* previous;
    XL6_X509ValidatedCrlV1 *base,*delta;
    struct XL6_X509PathV1* signerPath;
    size_t count;
} XL6_X509RevocationEvidenceV1;
typedef struct XL6_X509RevocationCheckV1 {
    XL6_X509RevocationTargetV1* target;
    xx509revocationcheck value;
    XL6_X509RevocationV1* result;
    XL6_X509RevocationEvidenceV1* evidence;
    bool hasPolicy;
    xtime time;
    uint64 skew;
    uint32 flags;
} XL6_X509RevocationCheckV1;
XL6_X509RevocationEvidenceV1* xl6X509RevocationEvidenceRefV1(XL6_X509RevocationEvidenceV1*);
void xl6X509RevocationEvidenceDropV1(XL6_X509RevocationEvidenceV1*);
XL6_X509RevocationEvidenceV1* xl6X509RevocationEvidenceAddV1(
    XL6_X509RevocationEvidenceV1*,void*,bool,struct XL6_X509PathV1*);

bool xl6X509PolicyRetainV1(xatomic64*);
bool xl6X509CrlPolicyConfigV1(int64_t,int64_t,uint64_t,xx509crlcriticalproc,void*,xx509crlconfig*);
void* xl6X509ValidatedCrlCreateV1(void*,void*,const xx509crlconfig*);
void* xl6X509ValidatedCrlRefV1(void*);
void xl6X509ValidatedCrlDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509ValidatedCrlHandlePolicyV1Get(void);
void* xl6X509CrlSetCreateV1(void*,void*);
void xl6X509CrlSetDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509CrlSetHandlePolicyV1Get(void);

void* xl6X509RevocationCopyV1(const xx509revocation*,xx509crlversion,bool);
void* xl6X509RevocationRefV1(void*);
void xl6X509RevocationDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509RevocationHandlePolicyV1Get(void);
void* xl6X509ValidatedCrlQueryV1(void*,void*);
void* xl6X509CrlSetQueryV1(void*,void*);
int64_t xl6X509RevocationStatusV1(void*);
uint64_t xl6X509RevocationCoverageV1(void*);
bool xl6X509RevocationObservedV1(void*);
void* xl6X509RevocationEntryV1(void*);

void* xl6X509RevocationCheckCreateV1(void*);
void* xl6X509RevocationCheckWithCrlV1(void*,void*);
void* xl6X509RevocationCheckWithSetV1(void*,void*);
void* xl6X509RevocationCheckWithCrlPathV1(void*,void*,void*);
void* xl6X509RevocationCheckWithSetPathV1(void*,void*,void*);
void* xl6X509RevocationCheckResultV1(void*);
bool xl6X509RevocationCheckDeterminedV1(void*);
void xl6X509RevocationCheckDestroyV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509RevocationCheckHandlePolicyV1Get(void);
#endif
