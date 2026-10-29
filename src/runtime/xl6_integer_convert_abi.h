#ifndef XL6_INTEGER_CONVERT_ABI_H
#define XL6_INTEGER_CONVERT_ABI_H
/* Physical width profiles, not language TypeIds. Except U64, output uses the
 * language I64 slot. IR asserts the exact profile correspondence. */
typedef enum XL6_IntegerConvertTargetV1 {
    XL6_INTEGER_SIGNED_8 = 1, XL6_INTEGER_UNSIGNED_8,
    XL6_INTEGER_SIGNED_16, XL6_INTEGER_UNSIGNED_16,
    XL6_INTEGER_SIGNED_32, XL6_INTEGER_UNSIGNED_32,
    XL6_INTEGER_SIGNED_64, XL6_INTEGER_UNSIGNED_64
} XL6_IntegerConvertTargetV1;
#endif
