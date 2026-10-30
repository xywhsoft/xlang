/* 只编译、不执行：独立 C 调用方可以按任意顺序包含两层公开头。 */
#ifdef LZMA_CORE_FIRST
#include "../include/xrt_lzma.h"
#include "../include/lzma_native.h"
#else
#include "../include/lzma_native.h"
#include "../include/xrt_lzma.h"
#endif

int lzma_header_callback_type(xvalue* callback)
{
    return xlzma_compress_file(NULL, NULL, 5, false, 1, 1, callback);
}
