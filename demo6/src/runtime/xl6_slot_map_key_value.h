#ifndef XL6_SLOT_MAP_KEY_VALUE_H
#define XL6_SLOT_MAP_KEY_VALUE_H
#include "xl6_slot_map_key.h"
/* Separate resident projection. Native key copying does not select boxing,
 * hashing, Set storage or SlotMap storage. All results are owned. Unbox needs
 * a private zero-initialized output and preserves it on refusal. Same-ID
 * foreign handles are not keys. These are not persistence encoders. */
XL6_TEXT_API xvalue* xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1);
XL6_TEXT_API bool xl6SlotMapKeyUnboxV1(const xvalue*,XL6_SlotMapKeyV1*);
XL6_TEXT_API bool xl6SlotMapKeyValueAdmitsV1(const xvalue*);
XL6_TEXT_API const xvalueconverter* xl6SlotMapKeyConverterV1(void);
XL6_TEXT_API uint64 xl6SlotMapKeyHashV1(XL6_SlotMapKeyV1);
/* Exact typed-key content equality, not ordering or mutable-reference
 * identity. Source/erased Set recipes must explicitly select this capability
 * before enabling the language key API. No generic Set policy is changed. */
XL6_TEXT_API const xtypedsetkeypolicy* xl6SlotMapKeyContentPolicyV1(void);
XL6_TEXT_API const xrtownershipadapterv1* xl6SlotMapKeyValueOwnershipAdapterV1(xrtownershipref);
#endif
