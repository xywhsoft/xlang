#ifndef XL6_FOREIGN_VIEW_VALUE_H
#define XL6_FOREIGN_VIEW_VALUE_H
#include "xl6_foreign_view.h"
struct xvalue;
/* Box only at a real xvalue storage boundary, such as an ordinary class
 * field. Direct native calls, locals and closure captures keep their typed
 * control pointer and never use this representation. The shell owns exactly
 * one view reference; no second native payload/control record is allocated.
 * identity is the compiler-sealed nominal storage domain, never a guessed
 * native address or C spelling. A NULL view boxes as language null. */
XL6_CODE_LEASE_API struct xvalue* xl6ForeignViewBoxV1(XL6_ForeignView*,uint64_t identity);
/* Independent owning copy into an empty slot. The source is borrowed and
 * remains owned by its caller. Refusal leaves output empty, and never drops
 * an occupied output. Null is admitted only when explicitly requested.
 * Type IDs authorize storage projection, not native access: a closed view
 * remains copyable, while its methods still fail their native admission. */
XL6_CODE_LEASE_API bool xl6ForeignViewUnboxV1(const struct xvalue*,uint64_t identity,bool nullable,XL6_ForeignView**);
XL6_CODE_LEASE_API const XL6_OwnershipDomainV1* xl6OwnershipDomainForeignViewValueV1Get(void);
#endif
