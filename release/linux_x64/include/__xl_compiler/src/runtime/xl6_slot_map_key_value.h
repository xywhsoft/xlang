#ifndef XL6_SLOT_MAP_KEY_VALUE_H
#define XL6_SLOT_MAP_KEY_VALUE_H
#include "xl6_slot_map_key.h"
#include "xl6_native_leaf_value.h"
#include "src/type/xl6_native_value_ids.h"
/* Separate resident projection. Raw xrttype copying needs only KEY_NATIVE;
 * the checked CopyValue boundary currently shares this permanent leaf profile
 * with content hashing/boxing. Neither boundary selects Set or SlotMap storage.
 * All results are owned. Unbox needs
 * a private zero-initialized output and preserves it on refusal. Same-ID
 * foreign handles are not keys. These are not persistence encoders. */
XL6_TEXT_API xvalue* xl6SlotMapKeyBoxV1(XL6_SlotMapKeyV1);
/* C by-value transport, independent owning result, borrowed input. Failure
 * returns the empty record and a checked diagnostic, never an alias owner. */
XL6_TEXT_API XL6_SlotMapKeyV1 xl6SlotMapKeyCloneV1(XL6_SlotMapKeyV1);
XL6_TEXT_API bool xl6SlotMapKeyUnboxV1(const xvalue*,XL6_SlotMapKeyV1*);
XL6_TEXT_API bool xl6SlotMapKeyValueAdmitsV1(const xvalue*);
XL6_TEXT_API const xvalueconverter* xl6SlotMapKeyConverterV1(void);
XL6_TEXT_API uint64 xl6SlotMapKeyHashV1(XL6_SlotMapKeyV1);
/* Exact typed-key content equality, not ordering or mutable-reference
 * identity. Source/erased Set recipes must explicitly select this capability
 * before enabling the language key API. Open Value Sets consume the sealed
 * generic immutable-native-value producer, not a key-specific dependency. */
XL6_TEXT_API const xtypedsetkeypolicy* xl6SlotMapKeyContentPolicyV1(void);
/* Permanent native value contract for shared direct checked copy. */
XL6_TEXT_API const XL6_NativeLeafValueProfileV1* xl6SlotMapKeyValueProfileV1Get(void);
XL6_TEXT_API const xrtownershipadapterv1* xl6SlotMapKeyValueOwnershipAdapterV1(xrtownershipref);
#endif
