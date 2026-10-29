/* 独立 C 门使用固定 XRT 快照，内存故障注入不修改 xl.dll 宿主的分配策略。 */
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_THREAD
#define XRT_IMPLEMENTATION
#include "../include/xrt_mqtt_net.h"
