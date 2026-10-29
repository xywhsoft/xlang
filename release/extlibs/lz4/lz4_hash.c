/* Frame 的 xxHash 版本随 LZ4 一起固定，不污染 xxhash 包的公共符号。 */
#define XXH_NAMESPACE XL6_LZ4_
#include "third_party/xxhash.c"
