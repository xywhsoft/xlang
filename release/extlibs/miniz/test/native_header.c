/* 只编译、不执行：两种包含顺序均须给出一致的 C 桥接声明。 */
#ifdef MINIZ_CORE_FIRST
#include "../include/xrt_miniz.h"
#include "../include/miniz_native.h"
#else
#include "../include/miniz_native.h"
#include "../include/xrt_miniz.h"
#endif

int miniz_header_callback_type(xvalue* callback)
{
    return xminiz_compress_file(NULL, NULL, 6, false, 1, 1, callback);
}
