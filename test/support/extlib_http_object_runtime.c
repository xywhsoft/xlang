/* 协议对象测试不引入 socket、TLS、任务调度或第二份 XRT。 */
#define XRT_MODULE_HTTP
#define XRT_MODULE_HTTP_PARAM
#define XRT_MODULE_IO_BUFFER
#define XRT_MODULE_TIME_TEXT
#define XRT_MODULE_ATOMIC
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
