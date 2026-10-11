#ifndef XL6_NATIVE_VALUE_IDS_H
#define XL6_NATIVE_VALUE_IDS_H
#include <stdint.h>
/* Language TypeId hashing uses XLIR's established seed, not xrtTypeId's
 * native ABI hash. This is type:SlotMapKey in that language namespace.
 * The compiler contract tests must independently recompute and verify it. */
#define XL6_SLOT_MAP_KEY_LANGUAGE_VALUE_ID UINT64_C(0x397E103E16D68E26)
#endif
