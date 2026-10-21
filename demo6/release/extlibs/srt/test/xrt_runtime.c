/* 包级 C 测试只编入 SRT 实际依赖和分配故障注入模块。 */
#define XRT_MODULE_BUFFER
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_UNICODE
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
