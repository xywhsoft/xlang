#ifndef XL6_TREE_KEY_ABI_H
#define XL6_TREE_KEY_ABI_H

/* Closed ordering tags shared by Typed IR and the resident tree runtime.
 * These describe the key's exact physical comparison contract, not a
 * generated comparator or a generic language protocol callback. */
typedef enum XL6_TreeKeyOrderV1 {
    XL6_TREE_KEY_INVALID_V1 = 0,
    XL6_TREE_KEY_BOOL_V1 = 1,
    XL6_TREE_KEY_SIGNED_I64_V1 = 2,
    XL6_TREE_KEY_UNSIGNED_U64_V1 = 3,
    XL6_TREE_KEY_FLOAT_TOTAL_V1 = 4,
    XL6_TREE_KEY_STRING_V1 = 5,
    XL6_TREE_KEY_TYPE_ID_V1 = 6,
    XL6_TREE_KEY_CHAR_V1 = 7,
    XL6_TREE_KEY_C_SCALAR_V1 = 8,
    XL6_TREE_KEY_BYTES_V1 = 9
} XL6_TreeKeyOrderV1;

#endif
