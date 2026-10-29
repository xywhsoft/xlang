/* 小型算法库的独立 C 验证运行时；只使用仓库锁定的 XRT 快照。 */
#define XRT_MODULE_TEMP_MEMORY
#define XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_HASH64
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
