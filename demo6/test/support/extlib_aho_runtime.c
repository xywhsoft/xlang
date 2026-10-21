/* 独立 C 验收只启用本包使用的 XRT 模块，不重复接入语言运行库。 */
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_UNICODE
#define XRT_MODULE_STRING
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_IMPLEMENTATION
#include "xrt.h"
