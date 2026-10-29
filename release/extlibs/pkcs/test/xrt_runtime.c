/* 与产品清单相同的模块，再加测试所需内存故障注入；不编译无关 XRT 功能。 */
#define XRT_MODULE_ASN1_DER
#define XRT_MODULE_BUFFER_HEX
#define XRT_MODULE_VALUE
#define XRT_MODULE_STRING
#define XRT_MODULE_MEMORY_DEBUG
#define XRT_IMPLEMENTATION
#include "xrt.h"
