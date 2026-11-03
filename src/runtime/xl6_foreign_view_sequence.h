#ifndef XL6_FOREIGN_VIEW_SEQUENCE_H
#define XL6_FOREIGN_VIEW_SEQUENCE_H
#include "xl6_code_lease.h"
struct xrttype;
struct xvalue;
struct xvalueconverter;
struct XL6_SequenceAdmissionOpsV2;
struct XL6_OwnershipDomainV1;
struct XL6_ForeignView;
/* Resident owning pointer-slot ABI, distinct from a trivial C pointer and
 * from the immutable native-leaf codec. All nominal identities and nullable
 * policy are sealed by XLIR; this physical type alone grants no nominal cast.
 * Direct storage/read/iteration never boxes a view or its native payload. */
XL6_CODE_LEASE_API const struct xrttype* xl6ForeignViewStorageTypeV1(void);
XL6_CODE_LEASE_API struct xvalue* xl6ForeignViewSequenceCreateV1(unsigned int role,
    uint64_t ownerIdentity,uint64_t elementIdentity,uint64_t boxedIdentity,bool nullable);
XL6_CODE_LEASE_API const struct xvalueconverter* xl6ForeignViewSequenceConverterV1(void);
XL6_CODE_LEASE_API const struct XL6_SequenceAdmissionOpsV2* xl6ForeignViewSequenceAdmissionV1(void);
XL6_CODE_LEASE_API const struct XL6_OwnershipDomainV1* xl6OwnershipDomainSequenceForeignViewV1Get(void);
XL6_CODE_LEASE_API bool xl6ArrayExtractForeignViewV1(const struct xvalue*,uint64_t owner,
    uint64_t element,int64_t index,bool pop,uint64_t boxed,bool nullable,struct XL6_ForeignView**);
#endif
