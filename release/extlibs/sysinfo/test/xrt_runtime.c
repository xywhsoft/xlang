/* 按 manifest 启用产品依赖，测试额外开启内存诊断，不编译无关模块。 */
#define XRT_MODULE_UNICODE
#define XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
