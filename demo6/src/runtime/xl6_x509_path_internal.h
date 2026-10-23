#ifndef XL6_X509_PATH_INTERNAL_H
#define XL6_X509_PATH_INTERNAL_H
#include "xl6_x509_records_internal.h"

/* Resident foundations. These are not borrowed XRT views or a public language
 * ABI. A path contains the target first; its separate anchor is never a path
 * element. Only a successfully validated path may leave an outcome. */
enum {
    XL6_X509_PATH_VALID_V1=1,
    XL6_X509_PATH_REJECTED_V1=2,
    XL6_X509_PATH_UNSUPPORTED_V1=3
};
typedef struct XL6_X509AnchorV1 {
    xx509cert certificate;
    xx509anchor value;
    uint8 data[];
} XL6_X509AnchorV1;
typedef struct XL6_X509PathV1 {
    xatomic64 references;
    size_t count;
    xx509cert anchorCertificate;
    xx509anchor anchor;
    xx509pathconfig policy;
    xx509cert certificates[];
} XL6_X509PathV1;
typedef struct XL6_X509PathOutcomeV1 {
    int64_t status;
    int32_t code;
    int64_t depth;
    xerror* diagnostic;
    XL6_X509PathV1* path;
} XL6_X509PathOutcomeV1;
typedef struct XL6_X509PathPolicyV1 {
    xx509pathconfig value;
    int64_t maxCertificates,maxBytes;
    uint8 data[];
} XL6_X509PathPolicyV1;
bool xl6X509PolicyRetainV1(xatomic64*);
void* xl6X509PathRefV1(void*);

void* xl6X509AnchorCreateV1(void*);
void xl6X509AnchorDestroyV1(void*);
void* xl6X509AnchorCertificateV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509AnchorHandlePolicyV1Get(void);

bool xl6X509PathPolicyValidV1(const xx509pathconfig*);
void* xl6X509PathValidateCreateV1(void* const*,size_t,void*,const xx509pathconfig*,int64_t,int64_t);
void xl6X509PathOutcomeDestroyV1(void*);
int64_t xl6X509PathOutcomeStatusV1(void*);
int64_t xl6X509PathOutcomeCodeV1(void*);
int64_t xl6X509PathOutcomeDepthV1(void*);
bool xl6X509PathOutcomeRequireV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509PathOutcomeHandlePolicyV1Get(void);

void xl6X509PathDestroyV1(void*);
void* xl6X509PathCertificateV1(void*,int64_t);
void* xl6X509PathAnchorV1(void*);
const XL6_NativeHandlePolicyV1* xl6X509PathHandlePolicyV1Get(void);
#endif
