/* 独立 C 夹具只启用本包与内存调试，避免引入无关网络实现。 */
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_RANDOM
#define XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
