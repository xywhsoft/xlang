#ifndef XL6_FOREIGN_VIEW_VALUE_STORAGE_H
#define XL6_FOREIGN_VIEW_VALUE_STORAGE_H
#include "xl6_foreign_view_value.h"
/* Resident typed-container projection only. An unbound shell is a private
 * temporary: the pinned Sequence binds its immutable boxedIdentity and
 * re-admits the result before publication. Never store/publish an identity-0
 * shell. The ordinary Box/Unbox interface continues to require a nominal ID. */
XL6_CODE_LEASE_API struct xvalue* xl6ForeignViewStorageBoxV1(XL6_ForeignView*);
/* The caller must first admit the source against its sealed nominal policy.
 * This independently authenticates the resident ops/trace before retaining;
 * a matching TypeId alone never authorizes reading a foreign handle. */
XL6_CODE_LEASE_API bool xl6ForeignViewStorageUnboxV1(const struct xvalue*,XL6_ForeignView**);
/* Allocation-free and diagnostic-transparent; does not invoke foreign ops. */
XL6_CODE_LEASE_API bool xl6ForeignViewValueAdmitsV1(const struct xvalue*,uint64_t,bool nullable);
#endif
