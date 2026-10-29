/* 只编译：桥接头独立、与核心头及预先配置的 XRT 头组合均应成立。 */
#ifdef BZ2_XRT_FIRST
#include "xrt.h"
#elif defined(BZ2_CORE_FIRST)
#include "../include/xrt_bz2.h"
#endif
#ifdef XRT_MODULE_VALUE
#define BZ2_VALUE_WAS_ENABLED
#endif
#include "../include/bz2_native.h"
#if defined(XRT_MODULE_VALUE) && !defined(BZ2_VALUE_WAS_ENABLED)
#error "bz2 bridge header changed the host XRT module selection"
#endif
#ifdef BZ2_NATIVE_FIRST
#include "../include/xrt_bz2.h"
#endif

int bz2_native_header_probe(xvalue* callback)
{
    return xbz2_compress_file(NULL, NULL, 9, false, 1, 1, callback);
}
