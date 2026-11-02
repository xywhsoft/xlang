/* 按 manifest 启用产品依赖，测试额外开启线程与内存诊断。 */
#define XRT_MODULE_UNICODE
#define XRT_MODULE_NUMBER_INTEGER
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_IMPLEMENTATION
#include "xrt.h"
