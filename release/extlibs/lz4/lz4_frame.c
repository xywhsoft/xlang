/* 与其他扩展库隔离内部 xxHash 符号，官方算法源码保持原样。 */
#define XXH_NAMESPACE XL6_LZ4_
#include "third_party/lz4frame.c"
