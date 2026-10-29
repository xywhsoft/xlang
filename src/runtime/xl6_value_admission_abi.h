#ifndef XL6_VALUE_ADMISSION_ABI_H
#define XL6_VALUE_ADMISSION_ABI_H

/* Data-only language value domains shared by XLIR and resident validation.
 * This is NOT an xvalue conversion, physical slot type or ownership policy.
 * NONE is unsupported as an element rule (container-only checks use it).
 * Nominal/recursive domains require their own verified identity/schema rule;
 * a pointer-shaped carrier never grants admission to an arbitrary xvalue. */
typedef enum XL6_ValueAdmissionV1 {
    XL6_VALUE_ADMIT_NONE = 0,
    XL6_VALUE_ADMIT_ANY,
    XL6_VALUE_ADMIT_BOOL,
    XL6_VALUE_ADMIT_I8,
    XL6_VALUE_ADMIT_I16,
    XL6_VALUE_ADMIT_I32,
    XL6_VALUE_ADMIT_I64,
    /* Narrow unsigned language integers use the canonical signed-i64 boxed
     * carrier; bounds exclude negatives. Only u64 uses XVALUE_UINT. */
    XL6_VALUE_ADMIT_U8,
    XL6_VALUE_ADMIT_U16,
    XL6_VALUE_ADMIT_U32,
    XL6_VALUE_ADMIT_U64,
    XL6_VALUE_ADMIT_F32,
    XL6_VALUE_ADMIT_F64,
    XL6_VALUE_ADMIT_CHAR,
    XL6_VALUE_ADMIT_STRING,
    XL6_VALUE_ADMIT_BYTES,
    XL6_VALUE_ADMIT_TIME
} XL6_ValueAdmissionV1;
#endif
