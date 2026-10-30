/* 只绑定本包真实文件入口，通用夹具不替换 IO 或分配器。 */
#include "../include/lzma_native.h"
#define NATIVE_CODEC_NAME "lzma"
#define NATIVE_CODEC_PREFIX ".xrt-lzma-"
#define NATIVE_CODEC_COMPRESS xlzma_compress_file
#define NATIVE_CODEC_DECOMPRESS xlzma_decompress_file
#include "../../../../test/support/extlib_native_codec_file_rollback.h"
