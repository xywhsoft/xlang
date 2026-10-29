#ifndef XL6_X509_PROFILES_INTERNAL_H
#define XL6_X509_PROFILES_INTERNAL_H
#include "xl6_x509_records_internal.h"

/* Indexed snapshots own their complete sequence content and index in one
 * allocation. No borrowed cursor or view reaches a language object. */
typedef struct XL6_X509GeneralNamesV1 {size_t count;xbytesview content;xx509genname* items;} XL6_X509GeneralNamesV1;
typedef struct XL6_X509OidsV1 {size_t count;xbytesview content;xbytesview* items;} XL6_X509OidsV1;
typedef struct XL6_X509DistributionPointsV1 {size_t count;xbytesview content;xx509distributionpoint* items;} XL6_X509DistributionPointsV1;
typedef struct XL6_X509GeneralSubtreesV1 {size_t count;xbytesview content;xx509subtree* items;} XL6_X509GeneralSubtreesV1;
typedef struct XL6_X509GeneralNameV1 {xx509genname value;bool constraintBase;} XL6_X509GeneralNameV1;
typedef struct XL6_X509KeyUsageV1 {uint16 flags;} XL6_X509KeyUsageV1;
typedef struct XL6_X509BasicConstraintsV1 {xx509basicconstraints value;} XL6_X509BasicConstraintsV1;
typedef struct XL6_X509AuthorityKeyIdentifierV1 {xx509authoritykeyid value;xbytesview raw;} XL6_X509AuthorityKeyIdentifierV1;
typedef struct XL6_X509DistributionPointV1 {xx509distributionpoint value;} XL6_X509DistributionPointV1;
typedef struct XL6_X509NameConstraintsV1 {xx509nameconstraints value;xbytesview raw;} XL6_X509NameConstraintsV1;
typedef struct XL6_X509GeneralSubtreeV1 {xx509subtree value;} XL6_X509GeneralSubtreeV1;

bool xl6X509ProfileInputV1(const xvalue*,int64_t,xbytesview*);
bool xl6X509SequenceContentV1(xbytesview,xbytesview*);
xbytesview xl6X509CursorContentV1(xdercursor);
xvalue* xl6X509EncodeContentV1(uint32,xbytesview);
xstrview xl6X509ProfileTextV1(xbytesview);
bool xl6X509ProfileExtensionV1(const xx509cert*,uint8,xbytesview*);
const xx509cert* xl6X509ProfileCertificateV1(void*);
void* xl6X509GeneralNamesCopyV1(xbytesview,int64_t);
void* xl6X509OidsCopyV1(xbytesview,int64_t);
void* xl6X509DistributionPointsCopyV1(xbytesview,int64_t);
void* xl6X509GeneralSubtreesCopyV1(xbytesview,int64_t);
void* xl6X509GeneralNameCopyV1(const xx509genname*,bool);
void* xl6X509AuthorityKeyIdentifierCopyV1(xbytesview);
void* xl6X509DistributionPointCopyV1(xbytesview);
void* xl6X509NameConstraintsCopyV1(xbytesview);
void* xl6X509GeneralSubtreeCopyV1(xbytesview);

static inline const void* xl6X509ProfileCheckedV1(void* raw)
{
    if(!raw)xl6X509FailV1(XERR_ARGUMENT,"missing X.509 profile receiver");
    return raw;
}
#endif
