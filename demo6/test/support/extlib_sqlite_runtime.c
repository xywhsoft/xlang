/* 独立 C 门禁只链接选定的 XRT 模块，不依赖宿主 DLL。 */
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_IMPLEMENTATION
#include "xrt.h"
