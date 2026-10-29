/* 产品只依赖时间能力，测试额外启用内存故障诊断。 */
#define XRT_MODULE_TIME
#define XRT_MODULE_TIME_LOCAL
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
