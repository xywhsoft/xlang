/* 独立 C 门禁只启用字符串、内存检查和线程设施。 */
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#define XRT_IMPLEMENTATION
#include "xrt.h"
