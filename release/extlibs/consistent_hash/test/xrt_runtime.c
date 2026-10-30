/* 独立 C 测试使用正式 XRT 实现，不依赖 xl.exe 的运行时。 */
#define XRT_MODULE_HASH64
#define XRT_MODULE_MUTEX
#define XRT_MODULE_STRING
#define XRT_MODULE_UNICODE
#define XRT_MODULE_THREAD
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
