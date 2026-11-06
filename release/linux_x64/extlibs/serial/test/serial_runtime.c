/* 独立 GCC 门使用固定 XRT 快照，不与其他套件共享正在重建的对象文件。 */
#define XRT_MODULE_THREAD
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "../include/serial.h"
