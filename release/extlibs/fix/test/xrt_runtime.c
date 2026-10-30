/* 生产三模块加测试内存诊断；不以完整运行库掩盖依赖。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_NUMBER_FLOAT
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
