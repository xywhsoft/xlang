/* 固定使用上游 portable 后端，无 CPU 探测、SIMD 对象或共享派发状态。 */
#define BLAKE3_USE_NEON 0
#define blake3_compress_in_place blake3_compress_in_place_portable
#define blake3_compress_xof blake3_compress_xof_portable
#define blake3_hash_many blake3_hash_many_portable
#include "third_party/blake3_impl.h"

/* 缓冲尺寸也与单路后端一致；不改写上游文件。 */
#undef MAX_SIMD_DEGREE
#undef MAX_SIMD_DEGREE_OR_2
#define MAX_SIMD_DEGREE 1
#define MAX_SIMD_DEGREE_OR_2 2
#include "third_party/blake3.c"
#include "third_party/blake3_portable.c"

size_t blake3_simd_degree(void) { return 1; }
