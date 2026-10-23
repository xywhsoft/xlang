#ifndef XL6_X509_CRL_PROFILES_INTERNAL_H
#define XL6_X509_CRL_PROFILES_INTERNAL_H
#include "xl6_x509_profiles_internal.h"

typedef struct XL6_X509CrlEntriesV1 {
    size_t count;
    xx509crlversion version;
    xbytesview content;
    xx509crlentry* items;
} XL6_X509CrlEntriesV1;
typedef struct XL6_X509CrlEntryV1 {xx509crlentry value;xx509crlversion version;} XL6_X509CrlEntryV1;
typedef struct XL6_X509IssuingDistributionPointV1 {xx509issuingpoint value;xbytesview raw;} XL6_X509IssuingDistributionPointV1;

bool xl6X509CrlVersionCheckV1(int64_t,xx509crlversion*);
const xx509crl* xl6X509ProfileCrlV1(void*);
bool xl6X509CrlProfileExtensionV1(const xx509crl*,uint8,xbytesview*);
void* xl6X509CrlEntryCopyV1(xbytesview,xx509crlversion);
void* xl6X509CrlEntriesCopyV1(xbytesview,xx509crlversion,int64_t);
void* xl6X509IssuingDistributionPointCopyV1(xbytesview);
#endif
