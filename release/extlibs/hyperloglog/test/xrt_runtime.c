/* 包级 C 夹具只启用哈希、独立实例线程与内存调试。 */
#define XRT_MODULE_HASH64
#define XRT_MODULE_THREAD
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
