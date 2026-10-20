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
#endif
