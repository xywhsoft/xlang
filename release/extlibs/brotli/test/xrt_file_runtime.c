/* 普通 C 文件接口的独立宿主：只用 XRT，不编译 xruntime/value 回调桥。 */
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_IO_FILE
#define XRT_MODULE_FILE_TEMP
#define XRT_MODULE_FILE_WHOLE
#define XRT_MODULE_DIR
#define XRT_MODULE_DIR_TEMP
#define XRT_MODULE_PATH
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
