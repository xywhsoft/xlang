/* 只编译：桥接头不能隐式修改 C 宿主的 XRT 模块选择。 */
#ifdef SNAPPY_XRT_FIRST
#include "xrt.h"
#elif defined(SNAPPY_CORE_FIRST)
#include "../include/xrt_snappy.h"
#endif
#ifdef XRT_MODULE_VALUE
#define SNAPPY_VALUE_WAS_ENABLED
#endif
#include "../include/snappy_native.h"
#if defined(XRT_MODULE_VALUE) && !defined(SNAPPY_VALUE_WAS_ENABLED)
#error "snappy bridge header changed the host XRT module selection"
#endif
#ifdef SNAPPY_NATIVE_FIRST
#include "../include/xrt_snappy.h"
#endif

int snappy_native_header_probe(xvalue* callback)
{
    return xsnappy_compress_file(NULL, NULL, false, 1, 1, callback);
}
