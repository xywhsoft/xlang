/* 只编译：声明头不应主动改变宿主选择的 XRT 模块，也不能限制包含顺序。 */
#ifdef BROTLI_XRT_FIRST
#include "xrt.h"
#elif defined(BROTLI_CORE_FIRST)
#include "../include/xrt_brotli.h"
#endif
#ifdef XRT_MODULE_VALUE
#define BROTLI_VALUE_WAS_ENABLED
#endif
#include "../include/brotli_native.h"
#if defined(XRT_MODULE_VALUE) && !defined(BROTLI_VALUE_WAS_ENABLED)
#error "brotli bridge header changed the host XRT module selection"
#endif
#ifdef BROTLI_NATIVE_FIRST
#include "../include/xrt_brotli.h"
#endif

int brotli_native_header_probe(xvalue* callback)
{
    return xbrotli_compress_file(NULL, NULL, 4, 16, 0, false, 1, 1, callback);
}
