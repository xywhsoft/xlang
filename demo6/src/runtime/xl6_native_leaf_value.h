#ifndef XL6_NATIVE_LEAF_VALUE_H
#define XL6_NATIVE_LEAF_VALUE_H
#include "xl6_text_value.h"
/* A process-resident, immutable native VALUE capability. Neither a type ID
 * nor a foreign handle callback grants this capability. Publishers supply a
 * permanent native descriptor: zero is empty, Copy owns independently, Drop
 * restores empty, content never changes, and there are NO language graph edges.
 * No generated callbacks, module-local profiles or mutable handles are allowed.
 * C/native publishers are trusted; this is not a sandbox for arbitrary pointers. */
typedef struct XL6_NativeLeafValueProfileV1 {
    const xrttype* (*Type)(void);
    uint64_t identity;
    const xtypedsetkeypolicy* content;
    bool (*Empty)(const void*);
} XL6_NativeLeafValueProfileV1;
XL6_TEXT_API xvalue* xl6NativeLeafValueBoxV1(const XL6_NativeLeafValueProfileV1*,const void*);
XL6_TEXT_API bool xl6NativeLeafValueUnboxV1(const xvalue*,const XL6_NativeLeafValueProfileV1*,void*);
XL6_TEXT_API bool xl6NativeLeafValueAdmitsV1(const xvalue*,const XL6_NativeLeafValueProfileV1*);
XL6_TEXT_API bool xl6NativeLeafValueIsV1(const xvalue*);
XL6_TEXT_API const xrtownershipadapterv1* xl6NativeLeafValueOwnershipAdapterV1(
    xrtownershipref,const XL6_NativeLeafValueProfileV1*);
#endif
