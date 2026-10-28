/* 只编译：声明头不应改变宿主选择的 XRT 模块，也不能限制包含顺序。 */
#ifdef GZIP_XRT_FIRST
#include "xrt.h"
#elif defined(GZIP_CORE_FIRST)
#include "../include/xrt_gzip.h"
#endif
#ifdef XRT_MODULE_VALUE
#define GZIP_VALUE_WAS_ENABLED
#endif
#include "../include/gzip_native.h"
#if defined(XRT_MODULE_VALUE) && !defined(GZIP_VALUE_WAS_ENABLED)
#error "gzip bridge header changed the host XRT module selection"
#endif
#ifdef GZIP_NATIVE_FIRST
#include "../include/xrt_gzip.h"
#endif

int gzip_native_header_probe(xvalue* callback)
{
    return xgzip_compress_file(NULL, NULL, -1, 0, false, 1, 1, callback);
}
