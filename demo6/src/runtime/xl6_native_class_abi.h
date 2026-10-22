#ifndef XL6_NATIVE_CLASS_ABI_H
#define XL6_NATIVE_CLASS_ABI_H
#include "xl6_sequence_abi.h"
struct XL6_NativeHandlePolicyV1;
/* Compiler-sealed native field projection. Borrowed only during the resident
 * call, never stored as module-owned callbacks/descriptor data. */
typedef struct XL6_NativeClassFieldContextV1 {
    const char* fieldName;
    size_t fieldSize;
    const struct XL6_NativeHandlePolicyV1* policy;
} XL6_NativeClassFieldContextV1;
typedef struct XL6_NativeClassArrayContextV1 {
    XL6_SequenceLayoutV1 layout;
    const char* fieldName;
    size_t fieldSize;
    const struct XL6_NativeHandlePolicyV1* policy;
} XL6_NativeClassArrayContextV1;
struct XL6_CodeOwner;
struct XL6_ClassIdentitySchemaV3;
/* Sealed single-native-field result recipe. Schema/name storage is static
 * module data protected by the real Class CodeLease (or process-static for
 * an ownerless static executable). The by-value call view is never retained.
 * No native placeholder, generated factory or finalizer is permitted. */
typedef struct XL6_NativeClassResultContextV1 {
    XL6_NativeClassFieldContextV1 field;
    const struct XL6_CodeOwner* codeOwner;
    const struct XL6_ClassIdentitySchemaV3* schema;
} XL6_NativeClassResultContextV1;
#endif
