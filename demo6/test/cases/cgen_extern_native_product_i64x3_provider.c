#include "src/runtime/xl6_product_abi.h"
/* Explicit independent sidecar (not an automatically discovered same-stem
 * companion). Not a standard-library resident symbol: the same closed
 * value ABI must work for an independently compiled foreign provider. */
XL6_ProductI64x3V1 xl6_test_native_product_i64x3(int64_t seed)
{
    XL6_ProductI64x3V1 result = {seed, -17, INT64_MAX};
    return result;
}
