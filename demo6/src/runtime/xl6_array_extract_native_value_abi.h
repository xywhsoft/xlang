#ifndef XL6_ARRAY_EXTRACT_NATIVE_VALUE_ABI_H
#define XL6_ARRAY_EXTRACT_NATIVE_VALUE_ABI_H
#include <stdbool.h>
#include <stdint.h>
struct xvalue;
struct XL6_NativeLeafValueProfileV1;
struct XL6_NativeCollectionValueProfileV1;
/* Private, aligned, empty output; independent status and ownership. No boxed
 * temporary for an admitted native slot. A wire Array is projected before
 * removal. On any failure the admitted output is restored to empty. */
bool xl6ArrayExtractNativeValueV1(const struct xvalue*,uint64_t,uint64_t,
    int64_t,bool,const struct XL6_NativeLeafValueProfileV1*,void*);
bool xl6ArrayExtractNativeValueV2(const struct xvalue*,uint64_t,uint64_t,
    int64_t,bool,const struct XL6_NativeCollectionValueProfileV1*,void*);
#endif
