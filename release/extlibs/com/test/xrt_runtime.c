/* 发布模块加测试专用内存故障注入，不增加扩展包的运行依赖。 */
#define XRT_MODULE_VALUE_CONTAINER
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"

#define XL_VALUE_READ_WIRE_IMPLEMENTATION
#include "../../_common/xl_value_read_wire.h"
