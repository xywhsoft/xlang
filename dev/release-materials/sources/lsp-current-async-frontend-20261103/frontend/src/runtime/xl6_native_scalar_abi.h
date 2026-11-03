#ifndef XL6_NATIVE_SCALAR_ABI_H
#define XL6_NATIVE_SCALAR_ABI_H
/* Closed logical C profiles. The resident provider selects the actual target
 * C width/signedness, and delegates conversion to its builtin XRT descriptor.
 * NONE on a bridge slot means no native-scalar codec, not a default profile. */
typedef enum XL6_NativeScalarProfileV1 {
    XL6_NATIVE_SCALAR_NONE = 0,
    XL6_NATIVE_SCALAR_CHAR, XL6_NATIVE_SCALAR_SCHAR, XL6_NATIVE_SCALAR_UCHAR,
    XL6_NATIVE_SCALAR_SHORT, XL6_NATIVE_SCALAR_USHORT,
    XL6_NATIVE_SCALAR_INT, XL6_NATIVE_SCALAR_UINT,
    XL6_NATIVE_SCALAR_LONG, XL6_NATIVE_SCALAR_ULONG,
    XL6_NATIVE_SCALAR_LONGLONG, XL6_NATIVE_SCALAR_ULONGLONG,
    XL6_NATIVE_SCALAR_SIZE, XL6_NATIVE_SCALAR_SSIZE,
    XL6_NATIVE_SCALAR_PTRDIFF, XL6_NATIVE_SCALAR_INTPTR, XL6_NATIVE_SCALAR_UINTPTR,
    XL6_NATIVE_SCALAR_BOOL, XL6_NATIVE_SCALAR_FLOAT, XL6_NATIVE_SCALAR_DOUBLE,
    XL6_NATIVE_SCALAR_WCHAR, XL6_NATIVE_SCALAR_TYPE_ID
} XL6_NativeScalarProfileV1;
/* Physical input slots for safe explicit native constructors. Native profiles
 * occupy BASE + profile, while language slots keep their fixed ABI widths. */
typedef enum XL6_ScalarConversionInputV1 {
    XL6_SCALAR_INPUT_INVALID=0, XL6_SCALAR_INPUT_BOOL,
    XL6_SCALAR_INPUT_I64, XL6_SCALAR_INPUT_U64, XL6_SCALAR_INPUT_F32,
    XL6_SCALAR_INPUT_F64, XL6_SCALAR_INPUT_TIME, XL6_SCALAR_INPUT_CHAR,
    XL6_SCALAR_INPUT_XVALUE, XL6_SCALAR_INPUT_NATIVE_BASE=128
} XL6_ScalarConversionInputV1;
#endif
