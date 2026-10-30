/* 独立 C 测试只启用本包及故障注入所需模块，不引入网络等无关依赖。 */
#define XRT_MODULE_MATH
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
