#ifndef XL6_VALUE_DEFAULT_ABI_H
#define XL6_VALUE_DEFAULT_ABI_H
/* Data-only language defaults. This is neither a C representation nor a
 * constructor callback; a non-null nominal value is not defaulted to Null. */
typedef enum XL6_ValueDefaultAbiV1 {
    XL6_VALUE_DEFAULT_INVALID=0,
    XL6_VALUE_DEFAULT_FORBIDDEN,
    XL6_VALUE_DEFAULT_NULL,
    XL6_VALUE_DEFAULT_BOOL,
    XL6_VALUE_DEFAULT_INT,
    XL6_VALUE_DEFAULT_UINT,
    XL6_VALUE_DEFAULT_FLOAT,
    XL6_VALUE_DEFAULT_CHAR,
    XL6_VALUE_DEFAULT_STRING,
    XL6_VALUE_DEFAULT_BYTES,
    XL6_VALUE_DEFAULT_TIME
} XL6_ValueDefaultAbiV1;
#endif
