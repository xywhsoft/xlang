/* 生产仅需UTF-8解码；测试再启用分配故障，不编译无关的全套运行库。 */
#define XRT_MODULE_UNICODE
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#define XRT_IMPLEMENTATION
#include "xrt.h"
