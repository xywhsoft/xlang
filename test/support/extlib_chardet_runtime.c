/* 独立 C 测试只开启探测及验证设施，不引入第二份全功能运行库。 */
#define XRT_MODULE_CHARSET_DETECT
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_MODULE_ATOMIC
#define XRT_IMPLEMENTATION
#include "xrt.h"
