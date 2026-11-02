/* 压缩库的独立 C 验证运行时；不使用开发中的 XRT，也不链接第二份宿主运行库。 */
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_ERROR_FORMAT
#define XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_VALUE
#define XRT_IMPLEMENTATION
#include "xrt.h"
/* XRT 与 XRUNTIME 已分离；测试宿主按公开合同先提供一份 XRT。 */
#define XRUNTIME_MODULE_RUNTIME_VALUE_CALLABLE
#define XRUNTIME_IMPLEMENTATION
#include "xruntime.h"
